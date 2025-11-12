#version 330 core
layout (location = 0) in vec3 aPos;

out vec2 TexCoords;

void main()
{
    // 直接使用 NDC 坐标（-1 到 1）
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    
    // 将 NDC 坐标转换为纹理坐标（0 到 1）
    TexCoords = aPos.xy * 0.5 + 0.5;
}