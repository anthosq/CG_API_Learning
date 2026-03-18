// Billboard Shader - 始终面向相机的 2D 图标
// 用于渲染光源、相机等编辑器图标

#type vertex
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

uniform mat4 u_View;
uniform mat4 u_Projection;
uniform vec3 u_Position;      // Billboard 世界位置
uniform float u_Size;         // Billboard 大小

void main()
{
    // 提取相机的右向量和上向量（从视图矩阵）
    vec3 cameraRight = vec3(u_View[0][0], u_View[1][0], u_View[2][0]);
    vec3 cameraUp = vec3(u_View[0][1], u_View[1][1], u_View[2][1]);

    // 计算 Billboard 顶点位置
    vec3 worldPos = u_Position
                  + cameraRight * aPos.x * u_Size
                  + cameraUp * aPos.y * u_Size;

    gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);
    TexCoords = aTexCoords;
}

#type fragment
#version 330 core

// 多渲染目标输出
layout(location = 0) out vec4 FragColor;
layout(location = 1) out int EntityID;

in vec2 TexCoords;

uniform sampler2D u_Texture;
uniform vec3 u_Color;         // 图标颜色叠加
uniform int u_EntityID;       // 实体 ID（用于拾取）
uniform float u_Alpha;        // 透明度

void main()
{
    vec4 texColor = texture(u_Texture, TexCoords);

    // 丢弃完全透明的像素
    if (texColor.a < 0.1)
        discard;

    // 颜色叠加
    vec3 finalColor = texColor.rgb * u_Color;
    FragColor = vec4(finalColor, texColor.a * u_Alpha);
    EntityID = u_EntityID;
}
