// Bloom Prefilter Pass
// 从 HDR 颜色提取亮度超过阈值的像素，输出到半分辨率纹理
// 使用 Quadratic Threshold（Knee Curve），参考 Hazel Bloom.glsl
//
// threshold: 亮度低于此值的像素完全黑化
// knee:      过渡区宽度，0 = 硬阈值，1 = 完全软过渡

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

uniform sampler2D u_Texture;    // HDR 颜色（linear, GL_RGBA16F）
uniform float     u_Threshold;  // 亮度阈值（默认 1.0）
uniform float     u_Knee;       // 软过渡宽度（0~1，默认 0.1）

// Quadratic Threshold（来自 Hazel）
// curve.x = threshold - knee
// curve.y = 2 * knee
// curve.z = 0.25 / knee
vec3 QuadraticThreshold(vec3 color, float threshold, vec3 curve) {
    float brightness = max(color.r, max(color.g, color.b));
    // 软过渡区 [threshold-knee, threshold+knee]
    float rq = clamp(brightness - curve.x, 0.0, curve.y);
    rq = curve.z * rq * rq;
    // 硬截断与软过渡取最大，再归一化到原始颜色比例
    color *= max(rq, brightness - threshold) / max(brightness, 1e-4);
    return color;
}

// 13-tap 降采样（COD Advanced Warfare 方案）
// 注意：与 bloom_downsample.glsl 中的 Downsample13Tap 保持相同权重
vec3 Downsample13Tap(sampler2D tex, vec2 uv, vec2 texelSize) {
    // 中心 4 个 2x2 block（各加权 0.5）+ 4 个角（各加权 0.125）+ 4 个边中（各加权 0.125）
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

    // 加权和（归一化到 1.0）
    vec3 result = e             * 0.125;
    result += (a + c + g + i)   * 0.03125;  // 4 个角：每个 1/32
    result += (b + d + f + h)   * 0.0625;   // 4 条边：每个 1/16
    result += (j + k + l + m)   * 0.125;    // 内圈 4 个：每个 1/8
    return result;
}

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(u_Texture, 0));
    vec3 color = Downsample13Tap(u_Texture, v_TexCoord, texelSize);

    // Knee Curve 阈值过滤
    float knee   = u_Threshold * u_Knee;
    vec3  curve  = vec3(u_Threshold - knee, knee * 2.0, 0.25 / max(knee, 1e-5));
    color = QuadraticThreshold(color, u_Threshold, curve);

    o_Color = vec4(color, 1.0);
}
