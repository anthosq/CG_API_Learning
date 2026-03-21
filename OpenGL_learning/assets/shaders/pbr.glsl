// PBR Shader - Metallic-Roughness Workflow
// Based on Cook-Torrance BRDF

#type vertex
#version 430 core

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;
layout (location = 3) in vec3 a_Tangent;
layout (location = 4) in vec3 a_Bitangent;

layout (std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_View;
    mat4 u_Projection;
    vec4 u_CameraPos;
};

uniform mat4 u_Model;
uniform mat3 u_NormalMatrix;
uniform int u_EntityID;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    mat3 TBN;
    flat int EntityID;
} vs_out;

void main() {
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    vs_out.FragPos = worldPos.xyz;
    vs_out.TexCoord = a_TexCoord;
    vs_out.EntityID = u_EntityID;

    vec3 N = normalize(u_NormalMatrix * a_Normal);
    vs_out.Normal = N;

    vec3 T = normalize(u_NormalMatrix * a_Tangent);
    vec3 B = normalize(u_NormalMatrix * a_Bitangent);
    T = normalize(T - dot(T, N) * N);
    B = cross(N, T);
    vs_out.TBN = mat3(T, B, N);

    gl_Position = u_ViewProjection * worldPos;
}

#type fragment
#version 430 core

layout (location = 0) out vec4 FragColor;
layout (location = 1) out int EntityID;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    mat3 TBN;
    flat int EntityID;
} fs_in;

layout (std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_View;
    mat4 u_Projection;
    vec4 u_CameraPos;
};

#define MAX_POINT_LIGHTS 16

layout (std140) uniform LightingData {
    vec4 u_DirLightDirection;      // xyz = direction, w = intensity
    vec4 u_DirLightColor;          // xyz = color, w = shadowAmount

    vec4 u_PointLightPosRadius[MAX_POINT_LIGHTS];      // xyz = position, w = radius
    vec4 u_PointLightColorIntensity[MAX_POINT_LIGHTS]; // xyz = color, w = intensity
    vec4 u_PointLightParams[MAX_POINT_LIGHTS];         // x = falloff

    vec4 u_SpotLightPosRange;          // xyz = position, w = range
    vec4 u_SpotLightDirAngle;          // xyz = direction, w = angle (degrees)
    vec4 u_SpotLightColorIntensity;    // xyz = color, w = intensity
    vec4 u_SpotLightParams;            // x = falloff, y = angleAttenuation

    ivec4 u_LightCounts;               // x = point light count, y = spotlight enabled (0/1)
    vec4 u_AmbientColor;               // xyz = color, w = intensity
};

// Material properties
uniform vec3 u_AlbedoColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform vec3 u_EmissiveColor;
uniform float u_EmissiveIntensity;

// Material textures (always bound with defaults)
uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_AOMap;
uniform sampler2D u_EmissiveMap;

// IBL textures (for future use)
uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilterMap;
uniform sampler2D u_BrdfLUT;
uniform float u_EnvironmentIntensity;

const float PI = 3.14159265359;

// GGX/Trowbridge-Reitz Normal Distribution Function
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0001);
}

// Schlick-GGX Geometry Function
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0001);
}

// Smith's Geometry Function
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel-Schlick Approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness (for IBL)
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Cook-Torrance BRDF calculation
vec3 CookTorranceBRDF(vec3 N, vec3 V, vec3 L, vec3 radiance, vec3 F0, vec3 albedo, float metallic, float roughness) {
    vec3 H = normalize(V + L);

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    specular = min(specular, vec3(10.0));  // 限制高光峰值，防止低粗糙度时的爆光 (来自 Hazel)

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

vec3 CalculateDirectionalLight(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness) {
    vec3 L = normalize(-u_DirLightDirection.xyz);
    float intensity = u_DirLightDirection.w;
    vec3 radiance = u_DirLightColor.rgb * intensity;

    return CookTorranceBRDF(N, V, L, radiance, F0, albedo, metallic, roughness);
}

vec3 CalculatePointLight(int index, vec3 fragPos, vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness) {
    vec3 lightPos = u_PointLightPosRadius[index].xyz;
    float radius = u_PointLightPosRadius[index].w;
    vec3 lightColor = u_PointLightColorIntensity[index].rgb;
    float lightIntensity = u_PointLightColorIntensity[index].w;
    float falloff = u_PointLightParams[index].x;

    vec3 L = lightPos - fragPos;
    float distance = length(L);
    L = normalize(L);

    // Distance attenuation with radius-based falloff
    float attenuation = 1.0;
    if (radius > 0.0) {
        float distRatio = distance / radius;
        attenuation = max(1.0 - distRatio * distRatio, 0.0);
        attenuation *= attenuation;
    } else {
        attenuation = 1.0 / (1.0 + falloff * distance * distance);
    }

    vec3 radiance = lightColor * lightIntensity * attenuation;

    return CookTorranceBRDF(N, V, L, radiance, F0, albedo, metallic, roughness);
}

vec3 CalculateSpotLight(vec3 fragPos, vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness) {
    vec3 lightPos = u_SpotLightPosRange.xyz;
    float range = u_SpotLightPosRange.w;
    vec3 spotDir = normalize(u_SpotLightDirAngle.xyz);
    float cutoffAngle = radians(u_SpotLightDirAngle.w);
    vec3 lightColor = u_SpotLightColorIntensity.rgb;
    float lightIntensity = u_SpotLightColorIntensity.w;
    float falloff = u_SpotLightParams.x;
    float angleAttenuation = u_SpotLightParams.y;

    vec3 L = lightPos - fragPos;
    float distance = length(L);
    L = normalize(L);

    // Angle attenuation (spotlight cone)
    float theta = dot(L, -spotDir);
    float outerCutoff = cos(cutoffAngle);
    float innerCutoff = cos(cutoffAngle * 0.8);
    float epsilon = innerCutoff - outerCutoff;
    float spotEffect = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
    spotEffect = pow(spotEffect, angleAttenuation);

    // Distance attenuation
    float distanceAttenuation = 1.0;
    if (range > 0.0) {
        float distRatio = distance / range;
        distanceAttenuation = max(1.0 - distRatio, 0.0);
        distanceAttenuation *= distanceAttenuation;
    } else {
        distanceAttenuation = 1.0 / (1.0 + falloff * distance * distance);
    }

    vec3 radiance = lightColor * lightIntensity * spotEffect * distanceAttenuation;

    return CookTorranceBRDF(N, V, L, radiance, F0, albedo, metallic, roughness);
}

// IBL - Split-Sum 近似 (参考 Hazel IBL 函数)
vec3 CalculateIBL(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, float ao) {
    vec3 R = reflect(-V, N);
    float NdotV = max(dot(N, V), 0.0);

    vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD = (1.0 - F) * (1.0 - metallic);

    // 漫反射 IBL: 从辐照度图采样
    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    vec3 diffuseIBL = kD * irradiance * albedo;

    // 高光 IBL: 从预滤波环境图按粗糙度采样对应 mip 级别
    int mipLevels = textureQueryLevels(u_PrefilterMap);
    vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * float(mipLevels - 1)).rgb;

    // 从 BRDF LUT 读取 Fresnel 积分结果 (x = scale, y = bias)
    // 使用 F (含视角 Fresnel) 而非 F0, 与 LearnOpenGL/Hazel 保持一致, 掠射角处更亮
    vec2 brdf = texture(u_BrdfLUT, vec2(max(NdotV, 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);  // F0, not F (split-sum convention)

    // AO 只影响间接漫反射, 不影响镜面反射 (Hazel 约定)
    return (diffuseIBL * ao + specularIBL) * u_EnvironmentIntensity;
}

void main() {
    // Sample textures (defaults are bound when no texture specified)
    vec3 albedo = texture(u_AlbedoMap, fs_in.TexCoord).rgb * u_AlbedoColor;
    float metallic = texture(u_MetallicMap, fs_in.TexCoord).r * u_Metallic;
    float roughness = texture(u_RoughnessMap, fs_in.TexCoord).r * u_Roughness;
    float ao = texture(u_AOMap, fs_in.TexCoord).r * u_AO;

    roughness = clamp(roughness, 0.04, 1.0);

    // Normal mapping
    vec3 normalMap = texture(u_NormalMap, fs_in.TexCoord).rgb;
    normalMap = normalMap * 2.0 - 1.0;
    vec3 N = normalize(fs_in.TBN * normalMap);

    vec3 V = normalize(u_CameraPos.xyz - fs_in.FragPos);

    // F0 (reflectance at normal incidence)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    // Directional light
    if (u_DirLightDirection.w > 0.0) {
        Lo += CalculateDirectionalLight(N, V, F0, albedo, metallic, roughness);
    }

    // Point lights
    int pointLightCount = min(u_LightCounts.x, MAX_POINT_LIGHTS);
    for (int i = 0; i < pointLightCount; i++) {
        Lo += CalculatePointLight(i, fs_in.FragPos, N, V, F0, albedo, metallic, roughness);
    }

    // Spotlight
    if (u_LightCounts.y > 0) {
        Lo += CalculateSpotLight(fs_in.FragPos, N, V, F0, albedo, metallic, roughness);
    }

    // Ambient: IBL 优先, 无环境贴图时退回简单环境光
    vec3 ambient;
    if (u_EnvironmentIntensity > 0.0) {
        ambient = CalculateIBL(N, V, F0, albedo, metallic, roughness, ao);
    } else {
        ambient = u_AmbientColor.rgb * u_AmbientColor.w * albedo * ao * (1.0 - metallic);
    }

    vec3 color = ambient + Lo;

    // Emissive
    vec3 emissive = texture(u_EmissiveMap, fs_in.TexCoord).rgb * u_EmissiveColor * u_EmissiveIntensity;
    color += emissive;

    // HDR tonemapping (Reinhard)
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    // Debug: 显示 prefilter mip0 采样 - 结果正常
    // vec3 R = reflect(-V, N);
    // vec3 debugPrefilter = textureLod(u_PrefilterMap, R, 0.0).rgb;
    // FragColor = vec4(debugPrefilter, 1.0);

    // Debug: 显示反射向量
    // vec3 R = reflect(-V, N);
    // FragColor = vec4(normalize(R) * 0.5 + 0.5, 1.0);

    // int mipLevels = max(1, textureQueryLevels(u_PrefilterMap));
    // float lod = roughness * float(mipLevels - 1);
    // vec3 sampleLod = textureLod(u_PrefilterMap, R, lod).rgb;
    // FragColor = vec4(sampleLod, 1.0);

    // int mipLevels = textureQueryLevels(u_PrefilterMap);
    // float v = clamp(float(mipLevels) / 12.0, 0.0, 1.0); // 假设最多 12 级用于可视化
    // FragColor = vec4(vec3(v), 1.0);

    // vec3 env = texture(u_IrradianceMap, R).rgb;
    // vec3 pre0 = textureLod(u_PrefilterMap, R, 0.0).rgb;

    // 左右对比：env 左半球，prefilter 右半球（用 x 分量决定）
    // if (R.x > 0.0) FragColor = vec4(env, 1.0);
    // else FragColor = vec4(pre0, 1.0);

    // vec3 N = normalize(fs_in.TBN * vec3(0.0, 0.0, 1.0));
    // vec3 V = normalize(u_CameraPos - fs_in.FragPos);
    // vec3 R = reflect(-V, N);
    // float NdotV = max(dot(N, V), 0.0);

    // int mipLevels = max(1, textureQueryLevels(u_PrefilterMap));
    // float maxMip = float(mipLevels - 1);
    // float lod = (u_Roughness <= 0.0001) ? 0.0 : u_Roughness * maxMip;
    // vec3 prefiltered = textureLod(u_PrefilterMap, R, lod).rgb;

    // vec2 brdf = texture(u_BrdfLUT, vec2(NdotV, u_Roughness)).rg;
    // vec3 specularIBL = prefiltered * (F0 * brdf.x + brdf.y);
    // FragColor = vec4(specularIBL, 1.0);
    // vec2 lut = texture(u_BrdfLUT, vec2(0.5, 0.5)).rg;
    // FragColor = vec4(lut.r, lut.g, 0.0, 1.0);

    FragColor = vec4(color, 1.0);
    EntityID = fs_in.EntityID;
}
