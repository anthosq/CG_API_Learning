// Wireframe Shader - 线框着色器
// 用于碰撞体可视化、AABB 调试渲染等

#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

void main()
{
    gl_Position = u_Projection * u_View * u_Model * vec4(a_Position, 1.0);
}

#type fragment
#version 330 core

// 多渲染目标输出
layout(location = 0) out vec4 FragColor;
layout(location = 1) out int EntityID;

uniform vec3 u_Color;
uniform int u_EntityID;

void main()
{
    FragColor = vec4(u_Color, 1.0);
    EntityID = u_EntityID;
}
