// fluid_blur.glsl — Phase 12-B：双边滤波平滑流体深度图
//
// 空间高斯 × 深度差高斯，仅在深度连续区域平滑，保留边界锐利度。
// 水平（u_Direction=0）和垂直（u_Direction=1）各一 pass，分离计算降低复杂度。

#type vertex
#version 430 core

out vec2 v_UV;

void main() {
    vec2 uv     = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    v_UV        = uv;
}

#type fragment
#version 430 core

layout(location = 0) out float out_Depth;

in vec2 v_UV;

uniform sampler2D u_FluidDepth;
uniform vec2      u_TexelSize;
uniform int       u_Direction;     // 0 = 水平，1 = 垂直
uniform float     u_SigmaS;        // 空间高斯标准差（像素，典型 5~10）
uniform float     u_SigmaD;        // 深度差高斯标准差（典型 0.05~0.2）
uniform int       u_KernelRadius;  // 卷积半径（像素，典型 5~15）

float gaussian(float x, float sigma) {
    return exp(-0.5 * (x * x) / (sigma * sigma));
}

void main() {
    float centerDepth = texture(u_FluidDepth, v_UV).r;

    // 背景深度（1.0 = 最远）不参与平滑，直接透传
    if (centerDepth >= 0.9999) {
        out_Depth = centerDepth;
        return;
    }

    vec2  dir     = (u_Direction == 0) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    float sum     = 0.0;
    float wSum    = 0.0;

    for (int i = -u_KernelRadius; i <= u_KernelRadius; i++) {
        vec2  sampleUV    = v_UV + dir * float(i) * u_TexelSize;
        float sampleDepth = texture(u_FluidDepth, sampleUV).r;

        if (sampleDepth >= 0.9999) continue;  // 跳过背景样本

        float wS = gaussian(float(i),                     u_SigmaS);
        float wD = gaussian(sampleDepth - centerDepth,    u_SigmaD);
        float w  = wS * wD;

        sum  += sampleDepth * w;
        wSum += w;
    }

    out_Depth = (wSum > 1e-6) ? (sum / wSum) : centerDepth;
}
