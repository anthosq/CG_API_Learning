// G-Buffer 写入 Shader
// 几何 Pass：采样材质贴图，输出打包的几何数据到 4 个 attachment，不做任何光照计算。
//
// 输出布局：
//   Attachment 0 (RGBA8)  BaseColor.rgb + AO.a
//   Attachment 1 (RGBA8)  OctNormal.rg + Roughness.b + Metallic.a
//   Attachment 2 (RGBA8)  Emissive.rgb + ShadingModelID.a (0 = Default Lit)
//   Attachment 3 (R32I)   EntityID

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
uniform int  u_EntityID;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    mat3 TBN;
    flat int EntityID;
} vs_out;

void main() {
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    vs_out.FragPos  = worldPos.xyz;
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

layout (location = 0) out vec4 o_BaseColorAO;
layout (location = 1) out vec4 o_NormalRoughMetal;
layout (location = 2) out vec4 o_EmissiveShadingID;
layout (location = 3) out int  o_EntityID;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    mat3 TBN;
    flat int EntityID;
} fs_in;

uniform vec3  u_AlbedoColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform vec3  u_EmissiveColor;
uniform float u_EmissiveIntensity;

uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_AOMap;
uniform sampler2D u_EmissiveMap;

// 将单位法线编码为正八面体展开的 [0,1]² 正方形
// 等面积映射，全球面无奇点，2×8bit 精度约 0.45°
vec2 OctEncode(vec3 n) {
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    vec2 oct = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * sign(n.xy);
    return oct * 0.5 + 0.5;
}

void main() {
    vec3 albedo = texture(u_AlbedoMap, fs_in.TexCoord).rgb
                  * pow(max(u_AlbedoColor, vec3(0.0)), vec3(2.2));

    float metallic  = texture(u_MetallicMap,   fs_in.TexCoord).r * u_Metallic;
    float roughness = texture(u_RoughnessMap,  fs_in.TexCoord).r * u_Roughness;
    float ao        = texture(u_AOMap,         fs_in.TexCoord).r * u_AO;
    roughness = clamp(roughness, 0.04, 1.0);

    vec3 normalMap = texture(u_NormalMap, fs_in.TexCoord).rgb * 2.0 - 1.0;
    vec3 worldNormal = normalize(fs_in.TBN * normalMap);

    vec3 emissive = texture(u_EmissiveMap, fs_in.TexCoord).rgb * u_EmissiveColor * u_EmissiveIntensity;

    o_BaseColorAO      = vec4(albedo, ao);
    o_NormalRoughMetal = vec4(OctEncode(worldNormal), roughness, metallic);
    o_EmissiveShadingID = vec4(emissive, 0.0);  // ShadingModelID = 0 (Default Lit)
    o_EntityID         = fs_in.EntityID;
}
