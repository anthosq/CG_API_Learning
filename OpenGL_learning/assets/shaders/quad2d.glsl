// Quad2D Shader - 2D 批处理渲染
// 支持纹理数组批处理、颜色叠加、实体 ID 输出

#type vertex
#version 330 core

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec4 a_Color;
layout (location = 2) in vec2 a_TexCoord;
layout (location = 3) in float a_TexIndex;
layout (location = 4) in int a_EntityID;

out vec4 v_Color;
out vec2 v_TexCoord;
out float v_TexIndex;
flat out int v_EntityID;

uniform mat4 u_ViewProjection;

void main()
{
    v_Color = a_Color;
    v_TexCoord = a_TexCoord;
    v_TexIndex = a_TexIndex;
    v_EntityID = a_EntityID;

    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 330 core

// 多渲染目标输出
layout(location = 0) out vec4 FragColor;
layout(location = 1) out int EntityID;

in vec4 v_Color;
in vec2 v_TexCoord;
in float v_TexIndex;
flat in int v_EntityID;

uniform sampler2D u_Textures[16];

void main()
{
    vec4 texColor = v_Color;

    // 根据纹理索引采样（添加 clamp 确保索引有效）
    int index = clamp(int(v_TexIndex + 0.5), 0, 15);

    // 使用 if-else 替代 switch，避免某些驱动的 switch 问题
    if (index == 0)       texColor *= texture(u_Textures[0],  v_TexCoord);
    else if (index == 1)  texColor *= texture(u_Textures[1],  v_TexCoord);
    else if (index == 2)  texColor *= texture(u_Textures[2],  v_TexCoord);
    else if (index == 3)  texColor *= texture(u_Textures[3],  v_TexCoord);
    else if (index == 4)  texColor *= texture(u_Textures[4],  v_TexCoord);
    else if (index == 5)  texColor *= texture(u_Textures[5],  v_TexCoord);
    else if (index == 6)  texColor *= texture(u_Textures[6],  v_TexCoord);
    else if (index == 7)  texColor *= texture(u_Textures[7],  v_TexCoord);
    else if (index == 8)  texColor *= texture(u_Textures[8],  v_TexCoord);
    else if (index == 9)  texColor *= texture(u_Textures[9],  v_TexCoord);
    else if (index == 10) texColor *= texture(u_Textures[10], v_TexCoord);
    else if (index == 11) texColor *= texture(u_Textures[11], v_TexCoord);
    else if (index == 12) texColor *= texture(u_Textures[12], v_TexCoord);
    else if (index == 13) texColor *= texture(u_Textures[13], v_TexCoord);
    else if (index == 14) texColor *= texture(u_Textures[14], v_TexCoord);
    else                  texColor *= texture(u_Textures[15], v_TexCoord);

    // 丢弃透明像素
    if (texColor.a < 0.1)
        discard;

    FragColor = texColor;
    EntityID = v_EntityID;
}
