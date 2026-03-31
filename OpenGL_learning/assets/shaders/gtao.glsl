// gtao.glsl — Ground Truth Ambient Occlusion (XeGTAO Port, OpenGL)
//
// Reference: Intel XeGTAO (Jimenez et al. 2016)
// Original:  Copyright (C) 2016-2021 Intel Corporation, MIT License
// Port:      HLSL Compute → GLSL Compute for OpenGL 4.3+
//
// Algorithm:
//   For each pixel, cast GTAO_SLICES direction slices through the hemisphere.
//   In each slice, find the max horizon angle on both sides (±phi).
//   Compute the exact analytic integral of the cos-weighted visibility function.
//   Simultaneously accumulate the average unoccluded direction (bent normal).
//   Output packed AO + bent normal in R32UI (R8G8B8A8) and an edge map in R8.
//
// Coordinate convention (OpenGL, right-handed view space):
//   - Camera at origin, looks in -Z direction.
//   - viewspacePos.z = -linearDepth  (negative for visible geometry)
//   - viewVec = normalize(-viewspacePos)  →  points in +Z direction toward camera
//   - viewspaceNormal.y is NOT flipped (OpenGL UV origin bottom-left, no D3D inversion needed)
//
// Key differences from the HLSL reference:
//   - omega.y uses +sin(phi) instead of -sin(phi) (OpenGL UV.y increases upward, not downward)
//   - Bent normal local frame uses (0,0,+1) as reference, t1 is positive
//   - ViewspacePos.z = -linearDepth (not +depth as in D3D XeGTAO)
//
// GTAO_SLICES × GTAO_STEPS × 2 = 9 × 3 × 2 = 54 depth taps per pixel

#type compute
#version 430 core

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

uniform sampler2D  u_ViewNormal;
uniform sampler2D  u_HiZDepth;
uniform usampler2D u_HilbertLut;

layout(r32ui, binding = 0) uniform writeonly uimage2D o_AOTerm;
layout(r8,   binding = 1) uniform writeonly image2D   o_Edges;

layout(std140, binding = 0) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_View;
    mat4 u_Projection;
    vec4 u_CameraPos;
};

uniform vec2  u_NDCToViewMul;
uniform vec2  u_InvResolution;
uniform float u_EffectRadius;
uniform float u_FalloffRange;
uniform float u_RadiusMultiplier;
uniform float u_FinalValuePower;
uniform float u_SampleDistPower;
uniform float u_DepthMIPOffset;
uniform int   u_NoiseIndex;
uniform bool  u_UseGBufferNormals;

#define GTAO_SLICES      9
#define GTAO_STEPS       3
#define GTAO_PI          3.14159265358979323846
#define GTAO_HALF_PI     1.57079632679489661923
#define GTAO_DEPTH_MIPS  5
#define GTAO_TERM_SCALE  1.5

// Device depth [0,1] → positive linear distance from camera
float LinearizeDepth(float deviceDepth) {
    float ndc_z = deviceDepth * 2.0 - 1.0;
    return -u_Projection[3][2] / (-ndc_z - u_Projection[2][2]);
}

// Reconstruct view-space position (OpenGL: view +Z toward camera)
// view.z = -linearDepth (negative for visible geometry)
vec3 ViewspacePos(vec2 screenUV, float linearDepth) {
    vec2 ndc = screenUV * 2.0 - 1.0;
    return vec3(ndc * u_NDCToViewMul * linearDepth, -linearDepth);
}

// Approximation from Lagarde 2014, max error < 5e-5
float FastACos(float x) {
    float ax  = abs(x);
    float res = (-0.156583 * ax + GTAO_HALF_PI) * sqrt(1.0 - ax);
    return (x >= 0.0) ? res : GTAO_PI - res;
}

// Octahedral decode for GBuffer world normals (Deferred path)
vec3 OctDecode(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0);
    n.x += (n.x >= 0.0) ? -t : t;
    n.y += (n.y >= 0.0) ? -t : t;
    return normalize(n);
}

// Returns per-edge sharpness [0=hard discontinuity, 1=same surface]
// Based on slope-compensated depth difference, scaled by view depth
vec4 CalculateEdges(float cZ, float lZ, float rZ, float tZ, float bZ) {
    vec4 edges = vec4(lZ, rZ, tZ, bZ) - cZ;
    float slopeLR = (edges.y - edges.x) * 0.5;
    float slopeTB = (edges.w - edges.z) * 0.5;
    vec4 adj = edges + vec4(slopeLR, -slopeLR, slopeTB, -slopeTB);
    return clamp(vec4(1.25) - min(abs(edges), abs(adj)) / (cZ * 0.011), 0.0, 1.0);
}

// Quantize 4 edge values to 2 bits each and pack into a single float in [0,1]
float PackEdges(vec4 edgesLRTB) {
    edgesLRTB = round(clamp(edgesLRTB, 0.0, 1.0) * 2.9);
    return dot(edgesLRTB, vec4(64.0/255.0, 16.0/255.0, 4.0/255.0, 1.0/255.0));
}

// Pack visibility (w) + bent normal (xyz) into R8G8B8A8 stored as R32UI
uint PackAOBentNormal(float visibility, vec3 bentNormal) {
    vec4 v = clamp(vec4(bentNormal * 0.5 + 0.5, visibility), 0.0, 1.0);
    return  uint(v.x * 255.0 + 0.5)
          | uint(v.y * 255.0 + 0.5) << 8u
          | uint(v.z * 255.0 + 0.5) << 16u
          | uint(v.w * 255.0 + 0.5) << 24u;
}

// Spatiotemporal noise via Hilbert LUT + R2 low-discrepancy sequence.
// Returns (noiseSlice, noiseSample) ∈ [0,1)² — spatially decorrelated per pixel,
// temporally shifted by frameIndex.
vec2 SpatioTemporalNoise(ivec2 pixCoord, int frameIndex) {
    uint idx = texelFetch(u_HilbertLut, pixCoord % 64, 0).r;
    idx += uint(288 * (frameIndex % 64));
    return fract(0.5 + float(idx) * vec2(0.75487766624669276, 0.56984029099));
}

void main() {
    ivec2 pixCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 texSize  = imageSize(o_AOTerm);
    if (any(greaterThanEqual(pixCoord, texSize))) return;

    vec2 screenUV = (vec2(pixCoord) + 0.5) * u_InvResolution;

    // Decode view-space normal
    vec3 viewspaceNormal;
    if (u_UseGBufferNormals) {
        vec3 worldN = OctDecode(texture(u_ViewNormal, screenUV).rg);
        viewspaceNormal = normalize(mat3(u_View) * worldN);
    } else {
        viewspaceNormal = normalize(texture(u_ViewNormal, screenUV).xyz * 2.0 - 1.0);
    }

    float rawZ = textureLod(u_HiZDepth, screenUV, 0.0).r;

    if (rawZ >= 0.9999) {
        imageStore(o_AOTerm, pixCoord, uvec4(PackAOBentNormal(1.0, viewspaceNormal)));
        imageStore(o_Edges,  pixCoord, vec4(1.0));
        return;
    }

    float viewspaceZ = LinearizeDepth(rawZ);

    // Edge detection from linearized neighbor depths
    float lZ = LinearizeDepth(textureLod(u_HiZDepth, screenUV + vec2(-u_InvResolution.x, 0.0), 0.0).r);
    float rZ = LinearizeDepth(textureLod(u_HiZDepth, screenUV + vec2( u_InvResolution.x, 0.0), 0.0).r);
    float tZ = LinearizeDepth(textureLod(u_HiZDepth, screenUV + vec2(0.0,  u_InvResolution.y), 0.0).r);
    float bZ = LinearizeDepth(textureLod(u_HiZDepth, screenUV + vec2(0.0, -u_InvResolution.y), 0.0).r);
    imageStore(o_Edges, pixCoord, vec4(PackEdges(CalculateEdges(viewspaceZ, lZ, rZ, tZ, bZ))));

    viewspaceZ *= 0.99999;  // nudge toward camera to avoid fp self-intersection (0.99920 is for FP16; we use R32F)

    vec3  pixCenterPos = ViewspacePos(screenUV, viewspaceZ);
    vec3  viewVec      = normalize(-pixCenterPos);  // points toward camera (+Z in OpenGL)

    float worldRadius  = u_EffectRadius * u_RadiusMultiplier;
    // 1 pixel = 2/width NDC = 2 * NDCToViewMul / width view-space units at depth z
    float pixelSizeAtZ      = viewspaceZ * u_NDCToViewMul.x * u_InvResolution.x * 2.0;
    float screenspaceRadius = worldRadius / pixelSizeAtZ;

    float falloffFrom  = worldRadius * (1.0 - u_FalloffRange);
    float falloffRange = worldRadius * u_FalloffRange;
    float falloffMul   = -1.0 / falloffRange;
    float falloffAdd   = falloffFrom / falloffRange + 1.0;

    // Smooth fade-in for very small projected radii (< ~10px), avoids instability
    float visibility = clamp((10.0 - screenspaceRadius) / 100.0, 0.0, 1.0) * 0.5;

    const float PIXEL_TOO_CLOSE = 1.3;
    if (screenspaceRadius < PIXEL_TOO_CLOSE) {
        imageStore(o_AOTerm, pixCoord, uvec4(PackAOBentNormal(1.0, viewspaceNormal)));
        return;
    }

    float minS = PIXEL_TOO_CLOSE / screenspaceRadius;

    vec2  noise       = SpatioTemporalNoise(pixCoord, u_NoiseIndex);
    float noiseSlice  = noise.x;
    float noiseSample = noise.y;

    vec3 bentNormal = vec3(0.0);

    for (int slice = 0; slice < GTAO_SLICES; slice++) {

        float sliceK = (float(slice) + noiseSlice) / float(GTAO_SLICES);
        float phi    = sliceK * GTAO_PI;

        // omega: screen-space search direction.
        // OpenGL UV.y increases upward (same as view-space Y), so both components share sign
        // with the 3D dirVec. Use +sin(phi), not -sin(phi) as in the D3D original.
        vec2  omega  = vec2(cos(phi), sin(phi)) * screenspaceRadius;

        // 3D view-space direction of this slice (consistent with omega above)
        vec3 dirVec = vec3(cos(phi), sin(phi), 0.0);

        vec3 orthoDir = dirVec - dot(dirVec, viewVec) * viewVec;
        vec3 axisVec  = normalize(cross(orthoDir, viewVec));

        vec3  projNorm    = viewspaceNormal - axisVec * dot(viewspaceNormal, axisVec);
        float projNormLen = length(projNorm);

        float signN = sign(dot(orthoDir, projNorm));
        float cosN  = clamp(dot(projNorm, viewVec) / projNormLen, 0.0, 1.0);
        float n     = signN * FastACos(cosN);

        // Horizon cosine lower bounds: tangent-plane limits
        float lowH0 = cos(n + GTAO_HALF_PI);  // = -sin(n), floor for +phi side
        float lowH1 = cos(n - GTAO_HALF_PI);  // = +sin(n), floor for -phi side
        float h0cos = lowH0;
        float h1cos = lowH1;

        for (int step = 0; step < GTAO_STEPS; step++) {
            float stepNoise = fract(noiseSample + float(slice + step * GTAO_STEPS) * 0.6180339887);
            float s = pow((float(step) + stepNoise) / float(GTAO_STEPS), u_SampleDistPower) + minS;

            vec2  sampleOffset = round(s * omega) * u_InvResolution;
            float sampleLenPx  = length(s * omega);
            float mipLevel     = clamp(log2(sampleLenPx) - u_DepthMIPOffset, 0.0, float(GTAO_DEPTH_MIPS));

            // +phi side
            {
                vec2  uv  = screenUV + sampleOffset;
                float sZ  = LinearizeDepth(textureLod(u_HiZDepth, uv, mipLevel).r);
                vec3  d   = ViewspacePos(uv, sZ) - pixCenterPos;
                float dist = length(d);
                float shc  = dot(d / dist, viewVec);
                shc = mix(lowH0, shc, clamp(dist * falloffMul + falloffAdd, 0.0, 1.0));
                h0cos = max(h0cos, shc);
            }

            // -phi side
            {
                vec2  uv  = screenUV - sampleOffset;
                float sZ  = LinearizeDepth(textureLod(u_HiZDepth, uv, mipLevel).r);
                vec3  d   = ViewspacePos(uv, sZ) - pixCenterPos;
                float dist = length(d);
                float shc  = dot(d / dist, viewVec);
                shc = mix(lowH1, shc, clamp(dist * falloffMul + falloffAdd, 0.0, 1.0));
                h1cos = max(h1cos, shc);
            }
        }

        // Convert horizon cosines to signed angles:
        //   h1cos (from -phi sampling) → left horizon h0 (negative angle)
        //   h0cos (from +phi sampling) → right horizon h1 (positive angle)
        float h0 = -FastACos(h1cos);
        float h1 =  FastACos(h0cos);

        projNormLen = mix(projNormLen, 1.0, 0.05);  // slight fudge against overdarkening

        // Exact analytic integral (GTAO paper Eq. 20)
        float iarc0 = (cosN + 2.0 * h0 * sin(n) - cos(2.0 * h0 - n)) * 0.25;
        float iarc1 = (cosN + 2.0 * h1 * sin(n) - cos(2.0 * h1 - n)) * 0.25;
        visibility += projNormLen * (iarc0 + iarc1);

        // Bent normal Algorithm 2 (GTAO paper)
        float t0 = (6.0*sin(h0-n) - sin(3.0*h0-n) + 6.0*sin(h1-n) - sin(3.0*h1-n)
                    + 16.0*sin(n) - 3.0*(sin(h0+n) + sin(h1+n))) * (1.0/12.0);
        float t1 = (-cos(3.0*h0-n) - cos(3.0*h1-n) + 8.0*cos(n)
                    - 3.0*(cos(h0+n) + cos(h1+n))) * (1.0/12.0);

        // Build local bent direction in slice frame, then rotate to view space.
        // OpenGL convention: viewVec ≈ (0,0,+1), so reference direction is (0,0,+1).
        // t1 > 0 means "toward camera" = +Z direction, hence +t1 for the Z component.
        vec3 localBent = vec3(dirVec.x * t0, dirVec.y * t0, t1);

        // Rodriguez rotation from (0,0,+1) to viewVec
        float e = dot(vec3(0.0, 0.0, 1.0), viewVec);
        if (abs(e) < 0.9997) {
            vec3  v  = cross(vec3(0.0, 0.0, 1.0), viewVec);
            float h_ = 1.0 / (1.0 + e);
            localBent = mat3(
                e + h_*v.x*v.x,    h_*v.x*v.y - v.z,  h_*v.x*v.z + v.y,
                h_*v.x*v.y + v.z,  e + h_*v.y*v.y,    h_*v.y*v.z - v.x,
                h_*v.x*v.z - v.y,  h_*v.y*v.z + v.x,  e + h_*v.z*v.z
            ) * localBent;
        }
        bentNormal += localBent * projNormLen;
    }

    visibility /= float(GTAO_SLICES);
    visibility  = pow(visibility, u_FinalValuePower);
    visibility  = max(0.03, visibility);
    visibility  = clamp(visibility / GTAO_TERM_SCALE, 0.0, 1.0);

    imageStore(o_AOTerm, pixCoord, uvec4(PackAOBentNormal(visibility, normalize(bentNormal))));
}
