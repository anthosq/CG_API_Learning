// SSSS Composite
//
// 全屏 Quad Pass：将模糊后的 SSS diffuse irradiance 乘以 SubsurfaceColor 后叠加回场景。
// ssss_blur 输出的是纯 irradiance（scatter/PI × radiance），不含任何材质颜色。
// 此 Pass 只乘 subsurfaceColor（介质内部散射颜色），不乘 albedo：
//   - albedo = 表面颜色（用于 specular F0、IBL specular）
//   - subsurfaceColor = 次表面介质颜色（定义散射光的出射颜色）
//   两者语义不同；乘 albedo 会导致深色表面把 SSS 散射压到近零
// C++ 侧启用 Additive Blend（GL_ONE + GL_ONE），直接写入 m_HDRFramebuffer。
// 使用 VAO 实际顶点属性（而非 gl_VertexID），配合 DrawFullscreenQuad 的 6-vertex quad。

#type vertex
#version 430 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main() {
    gl_Position = vec4(a_Position, 0.0, 1.0);
    v_TexCoord  = a_TexCoord;
}

#type fragment
#version 430 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_SSSColor;              // 模糊后 SSS diffuse irradiance（scatter/PI × radiance）
uniform sampler2D u_GBufEmissiveShadingID; // GBuffer Att2：subsurfaceColor (rgb) for SSS pixels

void main() {
    vec3 irradiance      = texture(u_SSSColor,             v_TexCoord).rgb;
    vec3 subsurfaceColor = texture(u_GBufEmissiveShadingID, v_TexCoord).rgb;

    o_Color = vec4(irradiance * subsurfaceColor, 1.0);
}
