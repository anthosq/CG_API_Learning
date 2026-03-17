#version 330 core

// 多渲染目标输出
layout(location = 0) out vec4 FragColor;
layout(location = 1) out int EntityID;

in vec3 TexCoords;

uniform samplerCube skybox;

void main() {
    FragColor = texture(skybox, TexCoords);
    EntityID = -1;  // 天空盒不是可选实体
}