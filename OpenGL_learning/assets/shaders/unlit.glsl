// Unlit Shader - 无光照着色器
// 用于光源可视化、调试等

#type vertex
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoords;

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
