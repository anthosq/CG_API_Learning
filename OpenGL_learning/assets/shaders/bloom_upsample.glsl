// Bloom Upsample Pass
// 3×3 Tent Filter 上采样
// 渲染时开启 GL_ONE + GL_ONE 加法混合，将结果叠加到目标 mip 已有内容上

#type vertex
#version 430 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main() {
    v_TexCoord  = a_TexCoord;
    gl_Position = vec4(a_Position.xy, 0.0, 1.0);
}

#type fragment
#version 430 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_Texture;     // 当前（较小）mip
uniform float     u_FilterRadius; // 采样半径（以 texel 为单位，默认 1.0）

// 3×3 Tent Filter（9 次采样，权重和为 1）
//   1 2 1
//   2 4 2  × (1/16)
//   1 2 1
vec3 UpsampleTent(sampler2D tex, vec2 uv, vec2 texelSize) {
    vec2 o = texelSize * u_FilterRadius;

    vec3 s  = texture(tex, uv + vec2(-o.x,  o.y)).rgb;      // TL
    s += 2.0 * texture(tex, uv + vec2( 0.0,  o.y)).rgb;     // TC
    s += texture(tex, uv + vec2( o.x,  o.y)).rgb;           // TR

    s += 2.0 * texture(tex, uv + vec2(-o.x,  0.0)).rgb;     // ML
    s += 4.0 * texture(tex, uv).rgb;                         // MC
    s += 2.0 * texture(tex, uv + vec2( o.x,  0.0)).rgb;     // MR

    s += texture(tex, uv + vec2(-o.x, -o.y)).rgb;           // BL
    s += 2.0 * texture(tex, uv + vec2( 0.0, -o.y)).rgb;     // BC
    s += texture(tex, uv + vec2( o.x, -o.y)).rgb;           // BR

    return s * (1.0 / 16.0);
}

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(u_Texture, 0));
    o_Color = vec4(UpsampleTent(u_Texture, v_TexCoord, texelSize), 1.0);
}
