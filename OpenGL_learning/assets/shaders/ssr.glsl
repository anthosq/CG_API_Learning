// Screen-Space Reflections (SSR) — Compute Shader
// 算法概述：
//   1. 从 GBuffer 重建视图空间位置和法线
//   2. 计算反射方向（视图空间 reflect），投影到屏幕空间得到 reflectSS
//   3. LinearRaymarch：线性步进 + 二分细化（替代 Hi-Z，消除色块伪影）
//   4. ValidateHit 计算命中置信度
//   5. ConeTracing 采样颜色金字塔（roughness → mip）
//   6. FresnelSchlickRoughness 加权
//   7. 输出 rgba16f：rgb = Fresnel 加权反射色，a = confidence

#type compute
#version 430 core

layout(local_size_x = 8, local_size_y = 8) in;

// 输出：rgb = Fresnel 加权 SSR 颜色，a = 命中置信度
layout(binding = 0, rgba16f) restrict writeonly uniform image2D o_SSR;

// GBuffer 输入
uniform sampler2D u_GDepth;      // 场景深度（DEPTH24_STENCIL8）
uniform sampler2D u_GNormal;     // Oct(Normal).rg + Roughness.b + Metallic.a
uniform sampler2D u_GBaseColor;  // BaseColor.rgb + AO.a

// SSR 输入
uniform sampler2D u_HiZ;           // 深度图（mip 0 用于步进，全链用于 ValidateHit）
uniform sampler2D u_SceneColor;    // HDR 颜色（mip0 采样用）
uniform sampler2D u_SceneColorMip; // 场景颜色金字塔（完整 mip 链，ConeTracing 用）
uniform int       u_SceneColorMipCount;

// 相机矩阵
uniform mat4  u_View;
uniform mat4  u_Projection;
uniform mat4  u_InvProjection;

// 分辨率（始终为全分辨率）
uniform vec2  u_Resolution;

// SSR 参数
uniform int   u_MaxSteps;
uniform float u_DepthTolerance;
uniform float u_FadeStart;
uniform float u_FacingFade;
uniform int   u_HiZMipCount;
uniform float u_Near;
uniform float u_Far;
uniform int   u_FrameIndex;
// 背面深度贴图：正面剔除渲染，depth ∈ [frontFace, backFace] 为有效命中区间
uniform sampler2D u_BackFaceDepth;

// Octahedral 解码
vec3 OctDecode(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.x += (n.x >= 0.0) ? -t : t;
    n.y += (n.y >= 0.0) ? -t : t;
    return normalize(n);
}

// 从 [0,1] 深度和屏幕 UV 重建视图空间坐标
vec3 ReconstructPositionVS(vec2 uv, float depth) {
    vec4 ndcPos  = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = u_InvProjection * ndcPos;
    return viewPos.xyz / viewPos.w;
}

// [0,1] NDC 深度 → 视图空间线性距离
float LinearizeDepth(float depth) {
    return (2.0 * u_Near * u_Far) / (u_Far + u_Near - depth * (u_Far - u_Near));
}

// 采样深度图 mip0（NEAREST）
float SampleDepth(vec2 uv) {
    return textureLod(u_HiZ, uv, 0.0).r;
}

// IGN（Interleaved Gradient Noise）：空间低差异噪声
float IGNoise(ivec2 coord) {
    return fract(52.9829189 * fract(dot(vec2(coord), vec2(0.06711056, 0.00583715))));
}

// 线性屏幕空间射线步进 + 二分细化
//
// 步距 = max(w,h) / MaxSteps 像素，在屏幕对角线范围内均匀步进。
// 检测到射线越过表面（pos.z > surfaceZ，标准深度 0=近/1=远）后，
// 用 8 步二分将误差压缩至 1/256 步距，得到像素级精确交点。
// 每个像素完全独立，不共享粗粒度 cell 边界，不产生大色块伪影。
vec3 LinearRaymarch(vec3 origin, vec3 direction, float jitter, out bool validHit)
{
    float pixelLen = max(abs(direction.x) * u_Resolution.x,
                         abs(direction.y) * u_Resolution.y);
    if (pixelLen < 1e-5) { validHit = false; return origin; }

    // 每步推进 max(w,h)/MaxSteps 个像素
    float maxDim     = max(u_Resolution.x, u_Resolution.y);
    float pixPerStep = maxDim / float(u_MaxSteps);
    vec3  stepDir    = direction * (pixPerStep / pixelLen);

    // IGN 抖动：每步偏移 0~1 步，打破固定 banding
    vec3 pos     = origin + stepDir * jitter;
    vec3 prevPos = pos;
    validHit = false;

    for (int i = 0; i < u_MaxSteps; i++) {
        prevPos = pos;
        pos    += stepDir;

        if (any(lessThan(pos.xy, vec2(0.0))) ||
            any(greaterThan(pos.xy, vec2(1.0)))) break;

        float surfaceZ = SampleDepth(pos.xy);

        // 跳过天空/背景
        if (surfaceZ >= 0.9999) continue;

        // 标准深度（0=近/1=far）：pos.z > surfaceZ → 射线越过了正面表面
        if (pos.z > surfaceZ) {
            // 背面深度测试：pos.z > backFaceZ → 射线已穿透背面 → 拒绝（反射拉长的根因）
            // 仅当 backFaceZ 与 surfaceZ 有实质性厚度差时才拒绝：
            //   双面网格（backFaceZ ≈ surfaceZ）→ 跳过测试，视为单面表面
            //   真实闭合体（backFaceZ >> surfaceZ）→ 拒绝穿透
            float backFaceZ = textureLod(u_BackFaceDepth, pos.xy, 0.0).r;
            if (backFaceZ > surfaceZ + 0.001 && pos.z > backFaceZ) continue;

            // 射线位于 [frontFaceZ, backFaceZ] 内 → 有效命中，二分细化
            vec3 lo = prevPos, hi = pos;
            for (int j = 0; j < 8; j++) {
                vec3  mid  = (lo + hi) * 0.5;
                float midZ = SampleDepth(mid.xy);
                if (mid.z > midZ) hi = mid;
                else              lo = mid;
            }
            validHit = true;
            return (lo + hi) * 0.5;
        }
    }

    return pos;
}

// 命中置信度
float ValidateHit(vec2 originUV, vec3 hitPos, float roughness,
                  vec3 rayDirVS, vec3 toPosVS)
{
    vec2 hitUV = hitPos.xy;

    if (any(lessThan(hitUV, vec2(0.0))) || any(greaterThan(hitUV, vec2(1.0))))
        return 0.0;

    // 自相交拒绝：曼哈顿距离 < 1 像素
    vec2 manhDist = abs(hitUV - originUV) * u_Resolution;
    if (all(lessThan(manhDist, vec2(1.0))))
        return 0.0;

    // 背景拒绝
    float surfaceZ = SampleDepth(hitUV);
    if (surfaceZ >= 0.9999)
        return 0.0;

    // 深度置信：视图空间线性深度偏差
    float hitLinear  = LinearizeDepth(hitPos.z);
    float surfLinear = LinearizeDepth(surfaceZ);
    float depthDelta = abs(hitLinear - surfLinear);
    float depthTol   = u_DepthTolerance * (1.0 + roughness * 2.0);
    float depthConf  = 1.0 - smoothstep(0.0, depthTol, depthDelta);

    // 边缘 Vignette
    float fade = 1.0 - u_FadeStart;
    vec2  v2   = smoothstep(vec2(0.0), vec2(fade), hitUV) *
                 (1.0 - smoothstep(vec2(1.0 - fade), vec2(1.0), hitUV));
    float vignette = v2.x * v2.y;

    // 掠射角淡出
    float facingConf = clamp(dot(toPosVS, rayDirVS) + u_FacingFade, 0.0, 1.0);

    return depthConf * vignette * facingConf * (1.0 - roughness);
}

// Cone Tracing — 沿命中射线采样颜色金字塔（roughness → mip）
float IsoscelesTriangleOpposite(float adjacentLength, float coneTheta) {
    return 2.0 * tan(coneTheta) * adjacentLength;
}
float IsoscelesTriangleInRadius(float a, float h) {
    float a2 = a * a; float fh2 = 4.0 * h * h;
    return (a * (sqrt(a2 + fh2) - a)) / (4.0 * h);
}
float IsoscelesTriangleNextAdjacent(float adjacentLength, float incircleRadius) {
    return adjacentLength - incircleRadius * 2.0;
}

vec3 ConeTracing(float roughness, vec2 rayOriginSS, vec2 hitUV) {
    float coneTheta = roughness * 3.14159265359 * 0.025;
    vec2  deltaPos  = hitUV - rayOriginSS;
    float adjacentLength = length(deltaPos);
    if (adjacentLength < 1e-5)
        return textureLod(u_SceneColorMip, hitUV, 0.0).rgb;

    vec2  adjacentUnit   = deltaPos / adjacentLength;
    vec3  totalColor     = vec3(0.0);
    float remainingAlpha = 1.0;

    for (int i = 0; i < 7; ++i) {
        float oppositeLength = IsoscelesTriangleOpposite(adjacentLength, coneTheta);
        float incircleSize   = IsoscelesTriangleInRadius(oppositeLength, adjacentLength);
        vec2  samplePos      = rayOriginSS + adjacentUnit * (adjacentLength - incircleSize);
        float mipLevel       = clamp(log2(incircleSize * max(u_Resolution.x, u_Resolution.y)),
                                     0.0, float(u_SceneColorMipCount - 1));
        vec3  sampleColor    = textureLod(u_SceneColorMip, samplePos, mipLevel).rgb;
        float weight         = min(remainingAlpha, 1.0 / 7.0);
        totalColor     += sampleColor * weight;
        remainingAlpha -= weight;
        adjacentLength  = IsoscelesTriangleNextAdjacent(adjacentLength, incircleSize);
        if (adjacentLength <= 0.0) break;
    }
    return totalColor + textureLod(u_SceneColorMip, hitUV, 0.0).rgb * max(remainingAlpha, 0.0);
}

// Fresnel-Schlick
vec3 FresnelSchlickRoughness(vec3 F0, float cosTheta, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
           pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    ivec2 coord   = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imgSize = imageSize(o_SSR);

    if (coord.x >= imgSize.x || coord.y >= imgSize.y) {
        imageStore(o_SSR, coord, vec4(0.0));
        return;
    }

    vec2 uv = (vec2(coord) + 0.5) / vec2(imgSize);

    float depth = texture(u_GDepth,     uv).r;
    vec4  nrm   = texture(u_GNormal,    uv);
    vec4  bcAO  = texture(u_GBaseColor, uv);

    float roughness = nrm.b;
    float metalness = nrm.a;
    vec3  baseColor = bcAO.rgb;

    if (depth >= 0.9999 || roughness > 0.8) {
        imageStore(o_SSR, coord, vec4(0.0));
        return;
    }

    // 重建视图空间位置与法线
    vec3 posVS     = ReconstructPositionVS(uv, depth);
    vec3 worldN    = OctDecode(nrm.rg);
    vec3 normalVS  = normalize(mat3(u_View) * worldN);
    vec3 toPosVS   = normalize(posVS);
    vec3 reflectVS = reflect(toPosVS, normalVS);

    // 屏幕空间射线方向
    vec4 ppSS4 = u_Projection * vec4(posVS + reflectVS, 1.0);
    if (ppSS4.w <= 0.0) {
        imageStore(o_SSR, coord, vec4(0.0));
        return;
    }
    vec3 ppSS;
    ppSS.xy = (ppSS4.xy / ppSS4.w) * 0.5 + 0.5;
    ppSS.z  = (ppSS4.z  / ppSS4.w) * 0.5 + 0.5;

    vec3 reflectSS = ppSS - vec3(uv, depth);

    // 射线基本垂直于屏幕 → 不可靠，跳过
    float pixelLen = max(abs(reflectSS.x) * u_Resolution.x,
                         abs(reflectSS.y) * u_Resolution.y);
    if (pixelLen < 0.5) {
        imageStore(o_SSR, coord, vec4(0.0));
        return;
    }

    // IGN 空间噪声 + 黄金比例时间抖动
    float noise = fract(IGNoise(coord) + float(u_FrameIndex & 0xFF) * 0.61803398875);

    bool validHit;
    vec3 hitPos = LinearRaymarch(vec3(uv, depth), reflectSS, noise, validHit);

    vec3  hitColor   = validHit ? ConeTracing(roughness, uv, hitPos.xy) : vec3(0.0);
    float confidence = validHit ? ValidateHit(uv, hitPos, roughness, reflectVS, toPosVS) : 0.0;

    vec3 F0 = mix(vec3(0.04), baseColor, metalness);
    vec3 F  = FresnelSchlickRoughness(F0, max(dot(normalVS, -toPosVS), 0.0), roughness);
    hitColor *= F;

    imageStore(o_SSR, coord, vec4(hitColor, max(confidence, 0.0)));
}
