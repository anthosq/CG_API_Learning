// Skybox Shader
// 支持 LOD (u_Lod) 和强度 (u_Intensity) 控制
// 当 u_Lod > 0 时显示模糊天空 (对应不同粗糙度的预滤波结果)

#type vertex
#version 430 core

layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main() {
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww;  // 确保深度为最大值 1.0
}

#type fragment
#version 430 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out int EntityID;

in vec3 TexCoords;

uniform samplerCube u_EnvironmentMap;
uniform float u_Intensity = 1.0;
uniform float u_Lod = 0.0;

void main() {
    vec3 color = textureLod(u_EnvironmentMap, TexCoords, u_Lod).rgb * u_Intensity;
    FragColor = vec4(color, 1.0);
    EntityID = -1;
}
