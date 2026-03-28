#version 430 core

// density_lambda.glsl — Phase 11-D：密度计算 + 约束乘子 λ 求解
//
// ρᵢ = W_poly6(0) + Σⱼ W_poly6(|pᵢ - pⱼ|, h)   （j 为邻居列表，含自身）
// Cᵢ = ρᵢ/ρ₀ - 1
// λᵢ = -Cᵢ / ( Σₖ |∇_pk Cᵢ|² + ε )
//
// 密度核：Poly6；梯度核：Spiky（参考 Macklin & Müller 2013）

layout(local_size_x = 256) in;

layout(std430, binding = 1)  buffer PredictedPos  { vec4  predicted[]; };
layout(std430, binding = 7)  buffer NeighborCount { uint  neighborCount[]; };
layout(std430, binding = 8)  buffer NeighborIdx   { uint  neighborIdx[]; };
layout(std430, binding = 9)  buffer Density       { float density[]; };
layout(std430, binding = 12) buffer Lambda        { float lambda_[]; };

uniform float u_KernelRadius;
uniform float u_RestDensity;
uniform float u_RelaxationEps;   // 典型值 1e-6（参考 Macklin & Müller）
uniform float u_Mass;            // 粒子质量 = 6.4 * r³ * ρ₀
uniform uint  u_ParticleCount;
uniform uint  u_MaxNeighbors;

const float PI = 3.14159265358979f;

float W_poly6(float r2, float h2, float coeff) {
    if (r2 >= h2) return 0.0f;
    float d = h2 - r2;
    return coeff * d * d * d;
}

vec3 gradW_spiky(vec3 rij, float r, float h, float gradCoeff) {
    if (r <= 1e-5f || r >= h) return vec3(0.0f);
    float d = h - r;
    return gradCoeff * d * d * (rij / r);
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= u_ParticleCount) return;

    float h   = u_KernelRadius;
    float h2  = h * h;
    float h4  = h2 * h2;
    float h8  = h4 * h4;
    float h9  = h8 * h;
    float h6  = h4 * h2;

    float poly6Coeff = 315.0f / (64.0f * PI * h9);
    float spikyCoeff = -45.0f / (PI * h6);
    float invRho0    = 1.0f / u_RestDensity;

    vec3  pi = predicted[i].xyz;
    uint  nc = neighborCount[i];

    float rho    = W_poly6(0.0f, h2, poly6Coeff);  // 自身贡献
    vec3  grad_i = vec3(0.0f);                       // ∇_pi Ci
    float sum_sq = 0.0f;                             // Σⱼ |∇_pj Ci|²

    for (uint k = 0u; k < nc; k++) {
        uint  j   = neighborIdx[i * u_MaxNeighbors + k];
        vec3  rij = pi - predicted[j].xyz;
        float r   = length(rij);

        rho += W_poly6(r * r, h2, poly6Coeff);

        // ∇_pi W(|pi - pj|) 的 spiky 近似
        vec3 gw = gradW_spiky(rij, r, h, spikyCoeff);

        // |∇_pj Ci|² = (m * invRho0)² * |gw|²
        vec3 gj = u_Mass * invRho0 * gw;
        sum_sq += dot(gj, gj);
        grad_i += gj;
    }

    // ρᵢ = m * Σⱼ W(rᵢⱼ)
    rho *= u_Mass;
    density[i] = rho;

    // Cᵢ = max(ρᵢ/ρ₀ - 1, 0)，截断负值防止拉力不稳定
    float Ci   = max(rho * invRho0 - 1.0f, 0.0f);
    sum_sq    += dot(grad_i, grad_i);       // 加入 |∇_pi Ci|²
    lambda_[i] = -Ci / (sum_sq + u_RelaxationEps);
}
