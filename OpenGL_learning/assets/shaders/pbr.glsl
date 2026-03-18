// PBR Shader - Metallic-Roughness Workflow
// Based on Cook-Torrance BRDF
// Reference: Hazel Engine, LearnOpenGL PBR Tutorial

#type vertex
#version 330 core

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;
layout (location = 3) in vec3 a_Tangent;
layout (location = 4) in vec3 a_Bitangent;

// Camera UBO (binding = 0)
layout (std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_View;
    mat4 u_Projection;
    vec4 u_CameraPos;  // xyz = position, w = unused
};

// Per-object uniforms
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

    // Normal in world space
    vec3 N = normalize(u_NormalMatrix * a_Normal);
    vs_out.Normal = N;

    // TBN matrix for normal mapping
    vec3 T = normalize(u_NormalMatrix * a_Tangent);
    vec3 B = normalize(u_NormalMatrix * a_Bitangent);
    // Re-orthogonalize T with respect to N
    T = normalize(T - dot(T, N) * N);
    // Recalculate B
    B = cross(N, T);
    vs_out.TBN = mat3(T, B, N);

    gl_Position = u_ViewProjection * worldPos;
}

#type fragment
#version 330 core

layout (location = 0) out vec4 FragColor;
layout (location = 1) out int EntityID;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    mat3 TBN;
    flat int EntityID;
} fs_in;

// Camera UBO (binding = 0)
layout (std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_View;
    mat4 u_Projection;
    vec4 u_CameraPos;
};

// Lighting UBO (binding = 1)
layout (std140) uniform LightingData {
    vec4 u_DirLightDirection;  // xyz = direction, w = intensity
    vec4 u_DirLightColor;      // xyz = color, w = ambient intensity
    vec4 u_PointLightPositions[4];
    vec4 u_PointLightColors[4];    // xyz = color, w = intensity
    vec4 u_PointLightParams[4];    // x = constant, y = linear, z = quadratic, w = radius
    ivec4 u_LightCounts;           // x = point light count
    vec4 u_AmbientColor;           // xyz = color, w = intensity
};

// Material properties
uniform vec3 u_AlbedoColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform vec3 u_EmissiveColor;
uniform float u_EmissiveIntensity;

// Material textures
uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_AOMap;
uniform sampler2D u_EmissiveMap;

// Texture flags
uniform bool u_UseAlbedoMap;
uniform bool u_UseNormalMap;
uniform bool u_UseMetallicMap;
uniform bool u_UseRoughnessMap;
uniform bool u_UseAOMap;
uniform bool u_UseEmissiveMap;

// IBL textures (optional)
uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilterMap;
uniform sampler2D u_BrdfLUT;
uniform bool u_UseIBL;
uniform float u_EnvironmentIntensity;

const float PI = 3.14159265359;

// ============================================================================
// PBR Functions
// ============================================================================

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

// Schlick-GGX Geometry Function (single direction)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;  // Direct lighting

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0001);
}

// Smith's Geometry Function (combines view and light directions)
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

// ============================================================================
// Light Calculations
// ============================================================================

vec3 CalculateDirectionalLight(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness) {
    vec3 L = normalize(-u_DirLightDirection.xyz);
    vec3 H = normalize(V + L);

    float intensity = u_DirLightDirection.w;
    vec3 radiance = u_DirLightColor.rgb * intensity;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    // kS is equal to Fresnel
    vec3 kS = F;
    // For energy conservation, diffuse and specular can't be above 1.0
    vec3 kD = vec3(1.0) - kS;
    // Metallic surfaces don't have diffuse
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

vec3 CalculatePointLight(int index, vec3 fragPos, vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness) {
    vec3 lightPos = u_PointLightPositions[index].xyz;
    vec3 lightColor = u_PointLightColors[index].rgb;
    float lightIntensity = u_PointLightColors[index].w;

    vec3 L = normalize(lightPos - fragPos);
    vec3 H = normalize(V + L);

    // Attenuation
    float distance = length(lightPos - fragPos);
    float constant = u_PointLightParams[index].x;
    float linear = u_PointLightParams[index].y;
    float quadratic = u_PointLightParams[index].z;
    float attenuation = 1.0 / (constant + linear * distance + quadratic * distance * distance);

    vec3 radiance = lightColor * lightIntensity * attenuation;

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// ============================================================================
// IBL Functions
// ============================================================================

vec3 CalculateIBL(vec3 N, vec3 V, vec3 F0, vec3 albedo, float metallic, float roughness, float ao) {
    vec3 R = reflect(-V, N);

    // Fresnel with roughness for IBL
    vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    // Diffuse IBL (irradiance map)
    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    vec3 diffuse = irradiance * albedo;

    // Specular IBL (prefiltered environment map + BRDF LUT)
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(u_BrdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    vec3 ambient = (kD * diffuse + specular) * ao * u_EnvironmentIntensity;

    return ambient;
}

// ============================================================================
// Main
// ============================================================================

void main() {
    // Sample material properties
    vec3 albedo = u_UseAlbedoMap ? texture(u_AlbedoMap, fs_in.TexCoord).rgb * u_AlbedoColor : u_AlbedoColor;
    float metallic = u_UseMetallicMap ? texture(u_MetallicMap, fs_in.TexCoord).r * u_Metallic : u_Metallic;
    float roughness = u_UseRoughnessMap ? texture(u_RoughnessMap, fs_in.TexCoord).r * u_Roughness : u_Roughness;
    float ao = u_UseAOMap ? texture(u_AOMap, fs_in.TexCoord).r * u_AO : u_AO;

    // Clamp roughness to prevent division issues
    roughness = clamp(roughness, 0.04, 1.0);

    // Normal mapping
    vec3 N;
    if (u_UseNormalMap) {
        vec3 normalMap = texture(u_NormalMap, fs_in.TexCoord).rgb;
        normalMap = normalMap * 2.0 - 1.0;  // [0,1] -> [-1,1]
        N = normalize(fs_in.TBN * normalMap);
    } else {
        N = normalize(fs_in.Normal);
    }

    vec3 V = normalize(u_CameraPos.xyz - fs_in.FragPos);

    // Calculate F0 (reflectance at normal incidence)
    // For dielectrics: 0.04, for metals: albedo color
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Reflectance equation
    vec3 Lo = vec3(0.0);

    // Directional light
    if (u_DirLightDirection.w > 0.0) {
        Lo += CalculateDirectionalLight(N, V, F0, albedo, metallic, roughness);
    }

    // Point lights
    int pointLightCount = u_LightCounts.x;
    for (int i = 0; i < pointLightCount && i < 4; i++) {
        Lo += CalculatePointLight(i, fs_in.FragPos, N, V, F0, albedo, metallic, roughness);
    }

    // Ambient / IBL
    vec3 ambient;
    if (u_UseIBL) {
        ambient = CalculateIBL(N, V, F0, albedo, metallic, roughness, ao);
    } else {
        // Simple ambient fallback
        ambient = u_AmbientColor.rgb * u_AmbientColor.w * albedo * ao;
    }

    vec3 color = ambient + Lo;

    // Emissive
    vec3 emissive = vec3(0.0);
    if (u_UseEmissiveMap) {
        emissive = texture(u_EmissiveMap, fs_in.TexCoord).rgb * u_EmissiveColor * u_EmissiveIntensity;
    } else if (u_EmissiveIntensity > 0.0) {
        emissive = u_EmissiveColor * u_EmissiveIntensity;
    }
    color += emissive;

    // HDR tonemapping (Reinhard)
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
    EntityID = fs_in.EntityID;
}
