// Pre-Integrated Skin Rendering LUT Compute Shader
// 预计算皮肤漫反射散射积分查找表（PISR）
//
// 输出: RG16F 纹理 512x512
//   U 轴 (X): (NdotL + 1) / 2, 范围 [0,1] 对应 NdotL ∈ [-1, 1]
//   V 轴 (Y): curvature ∈ [0, 1]（曲率，越大散射越宽）
//   R 通道: 红色散射积分（皮肤红色通道散射最宽）
//   G 通道: 绿色散射积分
//
// 算法参考: Penner 2010 "Pre-Integrated Skin Shading"
// 对球面上光照进行一圈积分，权重来自皮肤漫散射剖面（6高斯近似）

#version 430 core

layout(binding = 0, rg16f) restrict writeonly uniform image2D o_SkinLUT;

const float PI    = 3.14159265359;
const float TwoPI = 2.0 * PI;

// 皮肤漫散射剖面：6 个高斯之和（Red / Green 分量）
// 来源: d'Eon & Luebke 2007, "Advanced Techniques for Realistic Real-Time Skin Rendering"
// 方差单位：mm²，在使用时通过 curvature 缩放到世界空间
const int NUM_GAUSSIANS = 6;

const float weights_R[6] = float[](0.233, 0.100, 0.118, 0.113, 0.358, 0.078);
const float sigma_R[6]   = float[](0.0064, 0.0484, 0.187, 0.567, 1.99, 7.41);

const float weights_G[6] = float[](0.455, 0.336, 0.198, 0.007, 0.004, 0.000);
const float sigma_G[6]   = float[](0.0064, 0.0484, 0.187, 0.567, 1.99, 7.41);

// 单个高斯散射剖面评估
float Gaussian(float r2, float sigma2) {
    return exp(-r2 / (2.0 * sigma2)) / (TwoPI * sigma2);
}

// 皮肤漫反射剖面 D(r)，r 为散射距离（mm）
float DiffuseProfile_R(float r) {
    float r2  = r * r;
    float sum = 0.0;
    for (int i = 0; i < NUM_GAUSSIANS; i++)
        sum += weights_R[i] * Gaussian(r2, sigma_R[i]);
    return sum;
}

float DiffuseProfile_G(float r) {
    float r2  = r * r;
    float sum = 0.0;
    for (int i = 0; i < NUM_GAUSSIANS; i++)
        sum += weights_G[i] * Gaussian(r2, sigma_G[i]);
    return sum;
}

const int NUM_SAMPLES = 256;

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
void main() {
    ivec2 coord      = ivec2(gl_GlobalInvocationID.xy);
    ivec2 outputSize = imageSize(o_SkinLUT);
    if (coord.x >= outputSize.x || coord.y >= outputSize.y) return;

    // UV: x→NdotL, y→curvature
    vec2 uv        = (vec2(coord) + 0.5) / vec2(outputSize);
    float NdotL    = uv.x * 2.0 - 1.0;        // [-1, 1]
    float curvature = uv.y;                     // [0, 1]

    // curvature 非常接近 0 时退化为标准 Lambert
    if (curvature < 0.0001) {
        float lambert = max(0.0, NdotL);
        imageStore(o_SkinLUT, coord, vec4(lambert, lambert, 0.0, 1.0));
        return;
    }

    float theta_L = acos(clamp(NdotL, -1.0, 1.0));

    float sumR = 0.0, sumG = 0.0;
    float totalR = 0.0, totalG = 0.0;

    // 在光方向附近对半圆积分（-PI 到 PI）
    for (int i = 0; i < NUM_SAMPLES; i++) {
        float phi = -PI + TwoPI * (float(i) + 0.5) / float(NUM_SAMPLES);

        // 该采样点的 NdotL
        float NdotL_i = cos(theta_L + phi);

        // 在单位球上，弦长 = 2*sin(|phi|/2)
        // 除以曲率 = 1/r_sphere → 实际散射距离（mm）
        float chord = 2.0 * sin(abs(phi) * 0.5);
        float dist  = chord / curvature;

        float wR = DiffuseProfile_R(dist);
        float wG = DiffuseProfile_G(dist);

        sumR   += max(0.0, NdotL_i) * wR;
        sumG   += max(0.0, NdotL_i) * wG;
        totalR += wR;
        totalG += wG;
    }

    float lutR = (totalR > 0.0) ? sumR / totalR : max(0.0, NdotL);
    float lutG = (totalG > 0.0) ? sumG / totalG : max(0.0, NdotL);

    imageStore(o_SkinLUT, coord, vec4(lutR, lutG, 0.0, 1.0));
}
