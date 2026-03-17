#version 330 core

// 多渲染目标输出
layout(location = 0) out vec4 FragColor;
layout(location = 1) out int EntityID;

uniform vec3 lightColor;

// Entity ID for mouse picking
uniform int u_EntityID;

void main()
{
    FragColor = vec4(lightColor, 1.0);
    EntityID = u_EntityID;
}
