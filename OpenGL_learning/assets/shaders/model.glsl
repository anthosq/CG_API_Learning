// Model Shader - 模型着色器
// 支持 Assimp 加载的模型纹理命名约定
// 统一格式版本

#type vertex
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat3 u_NormalMatrix;

void main()
{
    FragPos = vec3(u_Model * vec4(aPos, 1.0));
    Normal = u_NormalMatrix * aNormal;
    TexCoords = aTexCoords;
    gl_Position = u_Projection * u_View * vec4(FragPos, 1.0);
}

#type fragment
#version 330 core

// 多渲染目标输出
layout(location = 0) out vec4 FragColor;
layout(location = 1) out int EntityID;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

// 模型纹理 (Assimp 命名约定)
uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform sampler2D texture_normal1;

// 是否有纹理
uniform bool u_HasDiffuse = true;
uniform bool u_HasSpecular = false;

// 相机位置
uniform vec3 u_ViewPos;

// 实体 ID
uniform int u_EntityID;

// 方向光
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight dirLight;

// 点光源
struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};
#define NR_POINT_LIGHTS 4
uniform PointLight pointLight[NR_POINT_LIGHTS];  // 注意: 与 Renderer 一致使用单数
uniform int u_PointLightCount;

// 聚光灯
struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};
uniform SpotLight spotLight;
uniform bool u_SpotlightEnabled;

// 材质属性
uniform float u_Shininess = 32.0;

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 diffuseColor, vec3 specularColor);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor, vec3 specularColor);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor, vec3 specularColor);

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(u_ViewPos - FragPos);

    // 获取纹理颜色
    vec3 diffuseColor = u_HasDiffuse ? texture(texture_diffuse1, TexCoords).rgb : vec3(0.8);
    vec3 specularColor = u_HasSpecular ? texture(texture_specular1, TexCoords).rgb : vec3(0.5);

    // 计算光照
    vec3 result = vec3(0.0);

    // 方向光
    result += CalcDirLight(dirLight, norm, viewDir, diffuseColor, specularColor);

    // 点光源
    int lightCount = min(u_PointLightCount, NR_POINT_LIGHTS);
    for (int i = 0; i < lightCount; i++) {
        result += CalcPointLight(pointLight[i], norm, FragPos, viewDir, diffuseColor, specularColor);
    }

    // 聚光灯
    if (u_SpotlightEnabled && spotLight.cutOff > 0.0) {
        result += CalcSpotLight(spotLight, norm, FragPos, viewDir, diffuseColor, specularColor);
    }

    FragColor = vec4(result, 1.0);
    EntityID = u_EntityID;
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 diffuseColor, vec3 specularColor)
{
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Shininess);

    vec3 ambient = light.ambient * diffuseColor;
    vec3 diffuse = light.diffuse * diff * diffuseColor;
    vec3 specular = light.specular * spec * specularColor;

    return ambient + diffuse + specular;
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor, vec3 specularColor)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Shininess);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    vec3 ambient = light.ambient * diffuseColor * attenuation;
    vec3 diffuse = light.diffuse * diff * diffuseColor * attenuation;
    vec3 specular = light.specular * spec * specularColor * attenuation;

    return ambient + diffuse + specular;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor, vec3 specularColor)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Shininess);

    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    vec3 ambient = light.ambient * diffuseColor * attenuation;
    vec3 diffuse = light.diffuse * diff * diffuseColor * attenuation * intensity;
    vec3 specular = light.specular * spec * specularColor * attenuation * intensity;

    return ambient + diffuse + specular;
}
