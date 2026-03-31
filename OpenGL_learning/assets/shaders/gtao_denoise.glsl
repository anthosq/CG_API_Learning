// gtao_denoise.glsl -- GTAO Edge-Aware Spatial Denoise
//
// The raw GTAO output has per-pixel noise from stochastic slice/sample offsets.
// This pass smooths it with a 3x3 neighborhood filter, weighting each neighbor
// by its geometric edge connectivity so AO does not bleed across surface boundaries.
//
// Inputs:
//   u_AOTerm  -- raw GTAO output (R32UI packed: xyz=bent normal [0,1], w=visibility)
//   u_Edges   -- edge map (R8: 4x2-bit LRTB weights, 0=hard edge, 3=same surface)
//
// Outputs:
//   o_DenoisedAO  (r32ui) -- denoised packed AO + bent normal
//   o_AOFinal     (r16f)  -- AO scalar only, bound to u_SSAOMap in lighting pass
//
// Edge map bit layout (packed by gtao.glsl CalculateEdges/PackEdges):
//   bits [7:6] = Left, [5:4] = Right, [3:2] = Top (+y), [1:0] = Bottom (-y)

#type compute
#version 430 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(r32ui, binding = 0) uniform writeonly uimage2D o_DenoisedAO;
layout(r16f,  binding = 1) uniform writeonly image2D  o_AOFinal;

uniform usampler2D u_AOTerm;
uniform sampler2D  u_Edges;
uniform float      u_DenoiseBlurBeta;

// Unpack R32UI -> vec4(bentNormal.xyz in [0,1] encoding, visibility)
// We keep bentNormal in [0,1] during accumulation to avoid sign issues when averaging.
vec4 ReadSample(ivec2 coord) {
    uint p = texelFetch(u_AOTerm, coord, 0).r;
    return vec4(
        float( p        & 0xFFu) / 255.0,
        float((p >>  8) & 0xFFu) / 255.0,
        float((p >> 16) & 0xFFu) / 255.0,
        float((p >> 24) & 0xFFu) / 255.0
    );
}

// Repack (bentNormal in [-1,1], visibility) -> R32UI
uint PackResult(vec3 bentNormal, float visibility) {
    vec4 v = clamp(vec4(bentNormal * 0.5 + 0.5, visibility), 0.0, 1.0);
    return  uint(v.x * 255.0 + 0.5)
          | uint(v.y * 255.0 + 0.5) << 8u
          | uint(v.z * 255.0 + 0.5) << 16u
          | uint(v.w * 255.0 + 0.5) << 24u;
}

// Decode R8 edge map -> vec4(L, R, T, B) in [0,1].
// T = +y direction (top of screen in OpenGL).
vec4 ReadEdges(ivec2 coord) {
    uint v = uint(texelFetch(u_Edges, coord, 0).r * 255.5);
    return clamp(vec4(
        float((v >> 6) & 0x03u),
        float((v >> 4) & 0x03u),
        float((v >> 2) & 0x03u),
        float((v >> 0) & 0x03u)
    ) / 3.0, 0.0, 1.0);
}

void main() {
    ivec2 coord   = ivec2(gl_GlobalInvocationID.xy);
    ivec2 texSize = imageSize(o_DenoisedAO);
    if (any(greaterThanEqual(coord, texSize))) return;

    const float blurWeight = u_DenoiseBlurBeta / 5.0;
    const float diagWeight = 0.85 * 0.5;

    vec4 center = ReadSample(coord);
    vec4 edgesC = ReadEdges(coord);   // .x=L .y=R .z=T .w=B

    // Read edges of 4-connected neighbors for symmetry enforcement
    vec4 edgesL = ReadEdges(coord + ivec2(-1,  0));
    vec4 edgesR = ReadEdges(coord + ivec2( 1,  0));
    vec4 edgesT = ReadEdges(coord + ivec2( 0,  1));
    vec4 edgesB = ReadEdges(coord + ivec2( 0, -1));

    // Symmetry: both sides of each edge must agree; multiply clamps to the minimum
    edgesC *= vec4(edgesL.y, edgesR.x, edgesT.w, edgesB.z);

    // Allow slight AO leak when pixel is almost entirely surrounded by edges
    // (prevents aliasing on thin geometry without softening real edges)
    float edginess = clamp(4.0 - 2.5 - dot(edgesC, vec4(1.0)), 0.0, 1.0) / 1.5 * 0.5;
    edgesC = clamp(edgesC + edginess, 0.0, 1.0);

    // Diagonal weights: reachable through two edge-connected paths
    float wTL = diagWeight * (edgesC.x * edgesL.z + edgesC.z * edgesT.x);
    float wTR = diagWeight * (edgesC.y * edgesR.z + edgesC.z * edgesT.y);
    float wBL = diagWeight * (edgesC.x * edgesL.w + edgesC.w * edgesB.x);
    float wBR = diagWeight * (edgesC.y * edgesR.w + edgesC.w * edgesB.y);

    vec4  sum       = center * blurWeight;
    float sumWeight = blurWeight;

    sum += ReadSample(coord + ivec2(-1,  0)) * edgesC.x;  sumWeight += edgesC.x;
    sum += ReadSample(coord + ivec2( 1,  0)) * edgesC.y;  sumWeight += edgesC.y;
    sum += ReadSample(coord + ivec2( 0,  1)) * edgesC.z;  sumWeight += edgesC.z;
    sum += ReadSample(coord + ivec2( 0, -1)) * edgesC.w;  sumWeight += edgesC.w;
    sum += ReadSample(coord + ivec2(-1,  1)) * wTL;        sumWeight += wTL;
    sum += ReadSample(coord + ivec2( 1,  1)) * wTR;        sumWeight += wTR;
    sum += ReadSample(coord + ivec2(-1, -1)) * wBL;        sumWeight += wBL;
    sum += ReadSample(coord + ivec2( 1, -1)) * wBR;        sumWeight += wBR;

    vec4  result  = sum / sumWeight;
    float outVis  = result.w;
    vec3  outBent = normalize(result.xyz * 2.0 - 1.0);

    imageStore(o_DenoisedAO, coord, uvec4(PackResult(outBent, outVis)));
    imageStore(o_AOFinal,    coord, vec4(outVis));
}
