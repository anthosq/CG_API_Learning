// brdf.glsl — Cook-Torrance PBR BRDF 共享函数库
//
// 用法：在 fragment shader 的 #version 声明之后 #include 本文件。
// 本文件不含 #version 或 #type，是纯 GLSL 函数库。
//
// 包含：
//   DistributionGGX       GGX 法线分布函数 (NDF)
//   GeometrySchlickGGX    单方向 Schlick-GGX 几何遮蔽
//   GeometrySmith         Smith 双向几何遮蔽
//   FresnelSchlick        Schlick Fresnel 近似
//   FresnelSchlickRoughness  含粗糙度修正的 Fresnel（用于 IBL）
//   CookTorranceBRDF      完整 Cook-Torrance 镜面 + Lambert 漫反射

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
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

vec3 CookTorranceBRDF(vec3 N, vec3 V, vec3 L, vec3 radiance,
                      vec3 F0, vec3 albedo, float metallic, float roughness)
{
    vec3  H   = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 specular = (NDF * G * F) /
                    (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
    specular = min(specular, vec3(10.0));  // 限制高光峰值，防止低粗糙度爆光（来自 Hazel）

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    return (kD * albedo / PI + specular) * radiance * max(dot(N, L), 0.0);
}
