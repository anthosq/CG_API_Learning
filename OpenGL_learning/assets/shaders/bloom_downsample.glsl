// Bloom Downsample Pass
// 13-tap 降采样滤波（无阈值，纯下采样）
// 参考: COD Advanced Warfare / Hazel Bloom.glsl

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

uniform sampler2D u_Texture;  // 上一级 bloom mip

// 13-tap 降采样：4 个 2×2 block（内圈）+ 5 个点（角+中心）
// 输入 texelSize 为源纹理的 texel 大小
vec3 Downsample13Tap(sampler2D tex, vec2 uv, vec2 texelSize) {
    vec3 a = texture(tex, uv + texelSize * vec2(-2.0,  2.0)).rgb;
    vec3 b = texture(tex, uv + texelSize * vec2( 0.0,  2.0)).rgb;
    vec3 c = texture(tex, uv + texelSize * vec2( 2.0,  2.0)).rgb;

    vec3 d = texture(tex, uv + texelSize * vec2(-2.0,  0.0)).rgb;
    vec3 e = texture(tex, uv + texelSize * vec2( 0.0,  0.0)).rgb;
    vec3 f = texture(tex, uv + texelSize * vec2( 2.0,  0.0)).rgb;

    vec3 g = texture(tex, uv + texelSize * vec2(-2.0, -2.0)).rgb;
    vec3 h = texture(tex, uv + texelSize * vec2( 0.0, -2.0)).rgb;
    vec3 i = texture(tex, uv + texelSize * vec2( 2.0, -2.0)).rgb;

    vec3 j = texture(tex, uv + texelSize * vec2(-1.0,  1.0)).rgb;
    vec3 k = texture(tex, uv + texelSize * vec2( 1.0,  1.0)).rgb;
    vec3 l = texture(tex, uv + texelSize * vec2(-1.0, -1.0)).rgb;
    vec3 m = texture(tex, uv + texelSize * vec2( 1.0, -1.0)).rgb;

    vec3 result  = e           * 0.125;
    result += (a + c + g + i)  * 0.03125;
    result += (b + d + f + h)  * 0.0625;
    result += (j + k + l + m)  * 0.125;
    return result;
}

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(u_Texture, 0));
    o_Color = vec4(Downsample13Tap(u_Texture, v_TexCoord, texelSize), 1.0);
}
