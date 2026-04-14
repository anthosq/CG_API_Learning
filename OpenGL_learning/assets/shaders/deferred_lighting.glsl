// Deferred Lighting Shader
// 全屏三角形 Pass：读取 G-Buffer，执行与 pbr.glsl 等价的 PBR 光照计算。
// 透明物体不经过此 Pass，仍走 Forward+ 路径。
//
// G-Buffer 绑定（由 C++ 端设置）：
//   u_GBufBaseColorAO      slot 0   RGBA8  BaseColor.rgb + AO.a
//   u_GBufNormalRoughMetal slot 1   RGBA8  OctNormal.rg + Roughness.b + Metallic.a
//   u_GBufEmissiveShadingID slot 2  RGBA8  Emissive.rgb + ShadingModelID.a
//   u_GBufDepth            slot 3   DEPTH  重建 WorldPosition 用

#type vertex
#version 430 core

out vec2 v_TexCoord;

// 无 VBO：用 gl_VertexID 生成覆盖全屏的三角形
// glDrawArrays(GL_TRIANGLES, 0, 3)
void main() {
    vec2 uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    v_TexCoord  = uv;
}

#type fragment
#version 430 core

layout (location = 0) out vec4 o_Color;
layout (location = 1) out vec4 o_SSSColor;  // SSS diffuse（仅 shadingModelID==1 且 u_EnableSSS 时写入）

in vec2 v_TexCoord;

// G-Buffer
uniform sampler2D u_GBufBaseColorAO;
uniform sampler2D u_GBufNormalRoughMetal;
uniform sampler2D u_GBufEmissiveShadingID;
uniform sampler2D u_GBufDepth;

// Camera
layout (std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_View;
    mat4 u_Projection;
    vec4 u_CameraPos;
};
uniform mat4 u_InvViewProjection;

// Lighting
#define MAX_POINT_LIGHTS    256
#define MAX_LIGHTS_PER_TILE 128
#define TILE_SIZE           16

layout (std140) uniform LightingData {
    vec4 u_DirLightDirection;
    vec4 u_DirLightColor;
    vec4 u_SpotLightPosRange;
    vec4 u_SpotLightDirAngle;
    vec4 u_SpotLightColorIntensity;
    vec4 u_SpotLightParams;
    ivec4 u_LightCounts;
    vec4 u_AmbientColor;
};

struct PointLightData {
    vec4 PosRadius;
    vec4 ColorIntensity;
    vec4 Params;
};
layout(std430, binding = 0) readonly buffer PointLightSSBO {
    PointLightData u_PointLights[];
};
layout(std430, binding = 1) readonly buffer TileLightCounts {
    int u_TileLightCounts[];
};
layout(std430, binding = 2) readonly buffer TileLightIndices {
    int u_TileLightIndices[];
};
uniform ivec2 u_TileCount;

// IBL
uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilterMap;
uniform sampler2D   u_BrdfLUT;
uniform float       u_EnvironmentIntensity;

// SSS
uniform sampler2D u_SkinLUT;
uniform float     u_SSSCurvatureScale         = 1.0;   // 全局曲率缩放，由 SettingsPanel 控制
uniform float     u_SSSTranslucency           = 0.0;   // 透光强度（0=关闭）
uniform float     u_SSSTranslucencyDistortion = 0.1;   // 透光法线扭曲：让光路偏向法线方向，使过渡更柔和

// SSAO
uniform sampler2D u_SSAOMap;

// 点光源阴影
uniform samplerCubeArray u_PointShadowMaps;

// 方向光阴影
layout(std140, binding = 3) uniform ShadowData {
    mat4  u_LightSpaceMatrices[4];
    vec4  u_CascadeSplits;
    float u_MaxShadowDistance;
    float u_ShadowFade;
    int   u_CascadeCount;
    float u_CascadeTransitionFade;
};
uniform sampler2DArray u_ShadowMap;  // 原始深度（无硬件比较），PCSS 和 PCF 均用此采样器
uniform bool  u_SoftShadows;
uniform float u_LightSize;          // PCSS 虚拟光源半径（UV 空间）；0 = 固定核 PCF
uniform bool  u_ShowCascades;

const float PI = 3.14159265359;

// Octahedral Decode：[0,1]² → 单位法线（与 gbuffer.glsl OctEncode 对应）
vec3 OctDecode(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.x += (n.x >= 0.0) ? -t : t;
    n.y += (n.y >= 0.0) ? -t : t;
    return normalize(n);
}

// BRDF 函数（与 pbr.glsl 完全一致）
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / max(NdotV * (1.0 - k) + k, 0.0001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CookTorranceBRDF(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 F0, vec3 albedo, float metallic, float roughness) {
    vec3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3  specular = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
    specular = min(specular, vec3(10.0));

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    return (kD * albedo / PI + specular) * radiance * max(dot(N, L), 0.0);
}

// IBL（与 pbr.glsl 完全一致）
vec3 CalculateIBL(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, float ao) {
    vec3 R = reflect(-V, N);
    float NdotV = max(dot(N, V), 0.0);

    vec3 F  = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD = (1.0 - F) * (1.0 - metallic);

    vec3 irradiance   = texture(u_IrradianceMap, N).rgb;
    vec3 diffuseIBL   = kD * irradiance * albedo;

    int  mipLevels       = textureQueryLevels(u_PrefilterMap);
    vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * float(mipLevels - 1)).rgb;
    vec2 brdf            = texture(u_BrdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specularIBL     = prefilteredColor * (F0 * brdf.x + brdf.y);

    return (diffuseIBL * ao + specularIBL) * u_EnvironmentIntensity;
}

// 阴影：Poisson PCF（与 pbr.glsl 完全一致）
const vec2 g_PoissonDisk[64] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870), vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845), vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554), vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507), vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367), vec2( 0.14383161, -0.14100790),
    vec2(-0.41392300, -0.43975700), vec2(-0.97915300, -0.20124500),
    vec2(-0.86557900, -0.28869500), vec2(-0.24370400, -0.18637800),
    vec2(-0.29492000, -0.05574800), vec2(-0.60445200, -0.54425100),
    vec2(-0.41805600, -0.58767900), vec2(-0.54915600, -0.41587700),
    vec2(-0.23808000, -0.61176100), vec2(-0.26700400, -0.45970200),
    vec2(-0.10000600, -0.22911600), vec2(-0.10192800, -0.38038200),
    vec2(-0.68146700, -0.70077300), vec2(-0.76348800, -0.54338600),
    vec2(-0.54903000, -0.75074900), vec2(-0.80904500, -0.40873800),
    vec2(-0.38813400, -0.77344800), vec2(-0.42939200, -0.89489200),
    vec2(-0.13159700,  0.06505800), vec2(-0.27500200,  0.10292200),
    vec2(-0.10611700, -0.06832700), vec2(-0.29458600, -0.89151500),
    vec2(-0.62941800,  0.37938700), vec2(-0.40725700,  0.33974800),
    vec2( 0.07165000, -0.38428400), vec2( 0.02201800, -0.26379300),
    vec2( 0.00387900, -0.13607300), vec2(-0.13753300, -0.76784400),
    vec2(-0.05087400, -0.90606800), vec2( 0.11413300, -0.07005300),
    vec2( 0.16331400, -0.21723100), vec2(-0.10026200, -0.58799200),
    vec2(-0.00494200,  0.12536800), vec2( 0.03530200, -0.61931000),
    vec2( 0.19564600, -0.45902200), vec2( 0.30396900, -0.34636200),
    vec2(-0.67811800,  0.68509900), vec2(-0.62841800,  0.50797800),
    vec2(-0.50847300,  0.45875300), vec2( 0.03213400, -0.78203000),
    vec2( 0.12259500,  0.28035300), vec2(-0.04364300,  0.31211900),
    vec2( 0.13299300,  0.08517000), vec2(-0.19210600,  0.28584800),
    vec2( 0.18362100, -0.71324200), vec2( 0.26522000, -0.59671600),
    vec2(-0.00962800, -0.48305800), vec2(-0.01851600,  0.43570300)
);

uint SelectCascade(float viewDepth) {
    for (int i = 0; i < u_CascadeCount - 1; i++) {
        if (viewDepth < u_CascadeSplits[i])
            return uint(i);
    }
    return uint(u_CascadeCount - 1);
}

// PCSS 阶段一：Blocker Search（64 采样，searchRadius ~0.05）
float FindBlockerDistance(uint cascade, vec2 shadowUV, float depth, float bias) {
    const float lightRadiusUV = 0.05;
    float searchRadius = lightRadiusUV * (depth - 0.0) / depth;
    float sumBlocker  = 0.0;
    int   numBlockers = 0;
    float layer = float(cascade);
    for (int i = 0; i < 64; i++) {
        vec2  offset = g_PoissonDisk[i] * searchRadius;
        float z      = textureLod(u_ShadowMap, vec3(shadowUV + offset, layer), 0).r;
        if (z < depth - bias) {
            sumBlocker += z;
            numBlockers++;
        }
    }
    return (numBlockers > 0) ? sumBlocker / float(numBlockers) : -1.0;
}

// 单个 cascade 采样（硬阴影 / 固定核 PCF / PCSS）
float SampleShadow(uint cascade, vec3 worldPos, float bias) {
    vec4 lightSpacePos = u_LightSpaceMatrices[cascade] * vec4(worldPos, 1.0);
    vec3 projCoords    = lightSpacePos.xyz / lightSpacePos.w;
    vec2 shadowUV      = projCoords.xy * 0.5 + 0.5;
    float depth        = projCoords.z * 0.5 + 0.5;

    if (any(lessThan(shadowUV, vec2(0.0))) || any(greaterThan(shadowUV, vec2(1.0))))
        return 1.0;

    float layer = float(cascade);

    if (!u_SoftShadows) {
        float z = textureLod(u_ShadowMap, vec3(shadowUV, layer), 0).r;
        return step(depth - bias, z);
    }

    if (u_LightSize > 0.0) {
        float avgBlocker = FindBlockerDistance(cascade, shadowUV, depth, bias);
        if (avgBlocker < 0.0) return 1.0;

        float penumbraWidth = (depth - avgBlocker) / avgBlocker;
        const float NEAR    = 0.01;
        float uvRadius      = penumbraWidth * u_LightSize * NEAR / depth;
        uvRadius            = min(uvRadius, 0.002);

        float sum = 0.0;
        for (int i = 0; i < 64; i++) {
            vec2  offset = g_PoissonDisk[i] * uvRadius;
            float z      = textureLod(u_ShadowMap, vec3(shadowUV + offset, layer), 0).r;
            sum         += step(depth - bias, z);
        }
        return sum / 64.0;
    } else {
        float texelSize = 1.0 / float(textureSize(u_ShadowMap, 0).x);
        float sum = 0.0;
        for (int i = 0; i < 64; i++) {
            vec2  offset = g_PoissonDisk[i] * texelSize * 1.5;
            float z      = textureLod(u_ShadowMap, vec3(shadowUV + offset, layer), 0).r;
            sum         += step(depth - bias, z);
        }
        return sum / 64.0;
    }
}

float ShadowCalculation(vec3 worldPos, float viewDepth, vec3 normal, vec3 lightDir) {
    float distFade = clamp(1.0 - (viewDepth - (u_MaxShadowDistance - u_ShadowFade))
                                 / u_ShadowFade, 0.0, 1.0);
    if (distFade <= 0.0) return 1.0;

    float NdotL = max(dot(normal, lightDir), 0.0);
    float bias  = max(0.001 * (1.0 - NdotL), 0.0002);
    uint  cascade = SelectCascade(viewDepth);
    float shadow;

    float half_fade = u_CascadeTransitionFade * 0.5;
    float c0 = smoothstep(u_CascadeSplits[0] + half_fade, u_CascadeSplits[0] - half_fade, viewDepth);
    float c1 = smoothstep(u_CascadeSplits[1] + half_fade, u_CascadeSplits[1] - half_fade, viewDepth);
    float c2 = smoothstep(u_CascadeSplits[2] + half_fade, u_CascadeSplits[2] - half_fade, viewDepth);

    if (c0 > 0.0 && c0 < 1.0)
        shadow = mix(SampleShadow(1u, worldPos, bias), SampleShadow(0u, worldPos, bias), c0);
    else if (c1 > 0.0 && c1 < 1.0 && u_CascadeCount > 2)
        shadow = mix(SampleShadow(2u, worldPos, bias), SampleShadow(1u, worldPos, bias), c1);
    else if (c2 > 0.0 && c2 < 1.0 && u_CascadeCount > 3)
        shadow = mix(SampleShadow(3u, worldPos, bias), SampleShadow(2u, worldPos, bias), c2);
    else
        shadow = SampleShadow(cascade, worldPos, bias);

    return mix(1.0, shadow, distFade);
}

// 次表面散射（PISR 预积分皮肤散射）— 返回纯 irradiance（不含 albedo/subsurfaceColor）
// 用预积分 LUT 替换 Lambert 漫射项，高光仍沿用 GGX
// albedo * subsurfaceColor 在 ssss_composite 模糊之后再乘，保证模糊的是 irradiance 而非颜色调制后的结果
// radius: per-material 散射宽度（从 GBuffer 解包），与全局 CurvatureScale 共同驱动 LUT V 轴
vec3 CalculateSSSDiffuse(vec3 N, vec3 L, float radius) {
    vec3  ndx       = dFdx(N);
    vec3  ndy       = dFdy(N);
    float curvature = clamp(sqrt(dot(ndx, ndx) + dot(ndy, ndy)) * radius * u_SSSCurvatureScale, 0.0, 1.0);

    float NdotL  = dot(N, L);
    vec2  lutUV  = vec2(NdotL * 0.5 + 0.5, curvature);
    vec2  lutVal = texture(u_SkinLUT, lutUV).rg;

    // R/G 通道对应红色/绿色散射积分，蓝色近似为绿色衰减（皮肤蓝色散射极少）
    vec3 scatter = vec3(lutVal.r, lutVal.g, lutVal.g * 0.7);

    return scatter / PI;
}

// 点光源衰减 + 阴影，输出 L（归一化方向）和 radiance（已含 attenuation * shadow）
void GetPointLightRadiance(int index, vec3 fragPos, vec3 N,
                           out vec3 L, out vec3 radiance) {
    vec3  lightPos       = u_PointLights[index].PosRadius.xyz;
    float radius         = u_PointLights[index].PosRadius.w;
    vec3  lightColor     = u_PointLights[index].ColorIntensity.rgb;
    float lightIntensity = u_PointLights[index].ColorIntensity.w;
    float falloff        = u_PointLights[index].Params.x;

    vec3  toLight  = lightPos - fragPos;
    float distance = length(toLight);
    L = normalize(toLight);

    float attenuation;
    if (radius > 0.0) {
        float distRatio = distance / radius;
        attenuation = max(1.0 - distRatio * distRatio, 0.0);
        attenuation *= attenuation;
    } else {
        attenuation = 1.0 / (1.0 + falloff * distance * distance);
    }

    float shadow     = 1.0;
    float shadowSlot = u_PointLights[index].Params.y;
    float farPlane   = u_PointLights[index].Params.z;
    if (shadowSlot >= 0.0 && farPlane > 0.0) {
        vec3  fragToLight  = fragPos - lightPos;
        float dist         = length(fragToLight);
        float currentDepth = dist / farPlane;
        float NdotL = max(dot(N, normalize(-fragToLight)), 0.0);
        float bias  = max(0.0005 * (1.0 - NdotL), 0.0001);

        if (u_SoftShadows) {
            const vec3 offsets[20] = vec3[](
                vec3( 1, 1, 1), vec3( 1,-1, 1), vec3(-1,-1, 1), vec3(-1, 1, 1),
                vec3( 1, 1,-1), vec3( 1,-1,-1), vec3(-1,-1,-1), vec3(-1, 1,-1),
                vec3( 1, 1, 0), vec3( 1,-1, 0), vec3(-1,-1, 0), vec3(-1, 1, 0),
                vec3( 1, 0, 1), vec3(-1, 0, 1), vec3( 1, 0,-1), vec3(-1, 0,-1),
                vec3( 0, 1, 1), vec3( 0,-1, 1), vec3( 0,-1,-1), vec3( 0, 1,-1)
            );
            float diskRadius = (1.0 + dist / farPlane) / 50.0;
            float angle = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))))
                          * 6.28318530718;
            float c = cos(angle), s = sin(angle);
            float sum = 0.0;
            for (int i = 0; i < 20; i++) {
                vec3 o = offsets[i];
                vec3 rotated = vec3(o.x * c - o.z * s, o.y, o.x * s + o.z * c);
                float closest = texture(u_PointShadowMaps,
                                        vec4(fragToLight + rotated * diskRadius, shadowSlot)).r;
                sum += (currentDepth - bias > closest) ? 0.0 : 1.0;
            }
            shadow = sum / 20.0;
        } else {
            float closest = texture(u_PointShadowMaps, vec4(fragToLight, shadowSlot)).r;
            shadow = (currentDepth - bias > closest) ? 0.0 : 1.0;
        }
    }

    radiance = lightColor * lightIntensity * attenuation * shadow;
}

vec3 CalculatePointLight(int index, vec3 fragPos, vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness) {
    vec3 L, radiance;
    GetPointLightRadiance(index, fragPos, N, L, radiance);
    return CookTorranceBRDF(N, V, L, radiance, F0, albedo, metallic, roughness);
}

// 聚光灯衰减，输出 L 和 radiance
void GetSpotLightRadiance(vec3 fragPos, out vec3 L, out vec3 radiance) {
    vec3  lightPos       = u_SpotLightPosRange.xyz;
    float range          = u_SpotLightPosRange.w;
    vec3  spotDir        = normalize(u_SpotLightDirAngle.xyz);
    float cutoffAngle    = radians(u_SpotLightDirAngle.w);
    vec3  lightColor     = u_SpotLightColorIntensity.rgb;
    float lightIntensity = u_SpotLightColorIntensity.w;
    float falloff        = u_SpotLightParams.x;
    float angleAttenuation = u_SpotLightParams.y;

    vec3  toLight  = lightPos - fragPos;
    float distance = length(toLight);
    L = normalize(toLight);

    float theta       = dot(L, -spotDir);
    float outerCutoff = cos(cutoffAngle);
    float innerCutoff = cos(cutoffAngle * 0.8);
    float epsilon     = innerCutoff - outerCutoff;
    float spotEffect  = pow(clamp((theta - outerCutoff) / epsilon, 0.0, 1.0), angleAttenuation);

    float distanceAttenuation;
    if (range > 0.0) {
        float distRatio = distance / range;
        distanceAttenuation = max(1.0 - distRatio, 0.0);
        distanceAttenuation *= distanceAttenuation;
    } else {
        distanceAttenuation = 1.0 / (1.0 + falloff * distance * distance);
    }

    radiance = lightColor * lightIntensity * spotEffect * distanceAttenuation;
}

vec3 CalculateSpotLight(vec3 fragPos, vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness) {
    vec3 L, radiance;
    GetSpotLightRadiance(fragPos, L, radiance);
    return CookTorranceBRDF(N, V, L, radiance, F0, albedo, metallic, roughness);
}

void main() {
    // 从 G-Buffer 读取几何数据
    vec4  gbuf0 = texture(u_GBufBaseColorAO,       v_TexCoord);
    vec4  gbuf1 = texture(u_GBufNormalRoughMetal,  v_TexCoord);
    vec4  gbuf2 = texture(u_GBufEmissiveShadingID, v_TexCoord);
    float depth = texture(u_GBufDepth,             v_TexCoord).r;

    // 跳过天空盒像素（深度 = 1.0 说明没有几何体写入）
    if (depth >= 1.0) {
        o_Color    = vec4(0.0, 0.0, 0.0, 1.0);
        o_SSSColor = vec4(0.0);
        return;
    }

    vec3  albedo    = gbuf0.rgb;
    float ao        = gbuf0.a;
    vec3  N         = OctDecode(gbuf1.rg);
    float roughness = gbuf1.b;
    float metallic  = gbuf1.a;
    // Att2.a 解包: [1:0]=ShadingModelID  [7:2]=QuantRadius (见 gbuffer.glsl 打包逻辑)
    int   packedA        = int(gbuf2.a * 255.0 + 0.5);
    int   shadingModelID = packedA & 0x3;
    float subsurfaceRadius = float((packedA >> 2) & 63) / 63.0 * 5.0;  // 还原到 [0,5]

    // Att2.rgb 含义按 ShadingModel 区分
    vec3 emissive       = (shadingModelID == 1) ? vec3(0.0) : gbuf2.rgb;
    vec3 subsurfaceColor = gbuf2.rgb;  // SSS 时为 SubsurfaceColor，其余时不使用

    // WorldPosition 从 Depth 重建
    vec4 clipPos  = vec4(v_TexCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = u_InvViewProjection * clipPos;
    vec3 fragPos  = worldPos.xyz / worldPos.w;

    // 屏幕空间 AO
    ao *= texture(u_SSAOMap, v_TexCoord).r;

    vec3 V   = normalize(u_CameraPos.xyz - fragPos);
    vec3 F0  = mix(vec3(0.04), albedo, metallic);

    // 相机空间深度，用于 cascade 选择和阴影距离淡出
    float viewDepth = -(u_View * vec4(fragPos, 1.0)).z;

    // Unlit：只输出自发光，跳过全部光照
    if (shadingModelID == 2) {
        o_Color    = vec4(emissive + albedo, 1.0);
        o_SSSColor = vec4(0.0);
        return;
    }

    vec3 Lo    = vec3(0.0);
    vec3 sssLo = vec3(0.0);  // SSS diffuse 专用累积（写入 o_SSSColor，后续 blur）

    // 方向光 + CSM 阴影
    if (u_DirLightDirection.w > 0.0) {
        vec3  L        = normalize(-u_DirLightDirection.xyz);
        float shadow   = ShadowCalculation(fragPos, viewDepth, N, L);
        vec3  radiance = u_DirLightColor.rgb * u_DirLightDirection.w;

        if (shadingModelID == 1) {
            // Subsurface：LUT 散射漫反射 → o_SSSColor，GGX 高光 → o_Color
            vec3 sssD = CalculateSSSDiffuse(N, L, subsurfaceRadius);
            vec3 spec = CookTorranceBRDF(N, V, L, radiance, F0, vec3(0.0), metallic, roughness);
            sssLo += sssD * radiance * shadow;
            Lo    += spec * shadow;
        } else {
            Lo += CookTorranceBRDF(N, V, L, radiance, F0, albedo, metallic, roughness) * shadow;
        }
    }

    // Tiled Forward+ 点光源（读 tile 光源列表，与 pbr.glsl 完全一致）
    ivec2 tileID    = ivec2(gl_FragCoord.xy) / TILE_SIZE;
    int   tileIndex = tileID.y * u_TileCount.x + tileID.x;
    int   tileCount = u_TileLightCounts[tileIndex];
    int   tileBase  = tileIndex * MAX_LIGHTS_PER_TILE;
    for (int i = 0; i < tileCount; i++) {
        int lightIdx = u_TileLightIndices[tileBase + i];
        if (shadingModelID == 1) {
            vec3 ptL, ptRadiance;
            GetPointLightRadiance(lightIdx, fragPos, N, ptL, ptRadiance);
            sssLo += CalculateSSSDiffuse(N, ptL, subsurfaceRadius) * ptRadiance;
            Lo    += CookTorranceBRDF(N, V, ptL, ptRadiance, F0, vec3(0.0), metallic, roughness);
        } else {
            Lo += CalculatePointLight(lightIdx, fragPos, N, V, F0, albedo, metallic, roughness);
        }
    }

    // 聚光灯
    if (u_LightCounts.y > 0) {
        if (shadingModelID == 1) {
            vec3 spL, spRadiance;
            GetSpotLightRadiance(fragPos, spL, spRadiance);
            sssLo += CalculateSSSDiffuse(N, spL, subsurfaceRadius) * spRadiance;
            Lo    += CookTorranceBRDF(N, V, spL, spRadiance, F0, vec3(0.0), metallic, roughness);
        } else {
            Lo += CalculateSpotLight(fragPos, N, V, F0, albedo, metallic, roughness);
        }
    }

    // IBL / 环境光
    // SSS 材质：IBL diffuse irradiance 加入 sssLo（经 blur 后乘 subsurfaceColor 出射）
    //            IBL specular 不 blur，直接写入 o_Color
    // DefaultLit：全量 IBL 写入 o_Color（原有路径）
    vec3 ambient;
    if (u_EnvironmentIntensity > 0.0) {
        if (shadingModelID == 1) {
            float NdotV = max(dot(N, V), 0.0);
            vec3  kS    = FresnelSchlickRoughness(NdotV, F0, roughness);
            vec3  kD    = (1.0 - kS) * (1.0 - metallic);

            // IBL diffuse irradiance（不含 albedo）→ sssLo，走 blur → composite × subsurfaceColor
            vec3 iblIrradiance = texture(u_IrradianceMap, N).rgb * u_EnvironmentIntensity * ao;
            sssLo += kD * iblIrradiance;

            // IBL specular → o_Color（保持清晰，不参与模糊）
            vec3 R                = reflect(-V, N);
            int  mipLevels        = textureQueryLevels(u_PrefilterMap);
            vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * float(mipLevels - 1)).rgb;
            vec2 brdf             = texture(u_BrdfLUT, vec2(NdotV, roughness)).rg;
            ambient = prefilteredColor * (F0 * brdf.x + brdf.y) * u_EnvironmentIntensity;
        } else {
            ambient = CalculateIBL(N, V, F0, albedo, metallic, roughness, ao);
        }
    } else {
        ambient = u_AmbientColor.rgb * u_AmbientColor.w * albedo * ao * (1.0 - metallic);
    }

    vec3 finalColor = ambient + Lo + emissive;
    if (u_ShowCascades) {
        const vec3 cascadeColors[4] = vec3[](
            vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0),
            vec3(0.0, 0.0, 1.0), vec3(1.0, 1.0, 0.0)
        );
        uint c = SelectCascade(viewDepth);
        finalColor = mix(finalColor, cascadeColors[c], 0.4);
    }
    o_Color    = vec4(finalColor, 1.0);
    o_SSSColor = vec4(sssLo, 1.0);
}
