#version 430 core

// delta_pos.glsl — Phase 11-D：计算粒子位置修正量 Δp
//
// Δpᵢ = (1/ρ₀) Σⱼ (λᵢ + λⱼ + s_corr) ∇W_spiky(pᵢ - pⱼ, h)
//
// 人工压力（s_corr）防止粒子聚集，使表面张力趋势受控：
//   s_corr = -k * ( W_poly6(r, h) / W_poly6(Δq, h) )^n
//   典型参数：k = 0.1, n = 4, Δq = 0.3h

layout(local_size_x = 256) in;

layout(std430, binding = 1)  buffer PredictedPos  { vec4  predicted[]; };
layout(std430, binding = 7)  buffer NeighborCount { uint  neighborCount[]; };
layout(std430, binding = 8)  buffer NeighborIdx   { uint  neighborIdx[]; };
layout(std430, binding = 12) buffer Lambda        { float lambda_[]; };
layout(std430, binding = 13) buffer DeltaPos      { vec4  deltaPos[]; };

uniform float u_KernelRadius;
uniform float u_RestDensity;
uniform float u_Mass;         // 粒子质量
uniform float u_ScorrK;       // 默认 0.1
uniform float u_ScorrDeltaQ;  // Δq/h，默认 0.3
uniform uint  u_ParticleCount;
uniform uint  u_MaxNeighbors;

const float PI    = 3.14159265358979f;
const int   SCORR_N = 4;

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

    // 人工压力分母（常量，只计算一次）
    float dq  = u_ScorrDeltaQ * h;
    float Wdq = W_poly6(dq * dq, h2, poly6Coeff);
    bool  useScorr = (Wdq > 1e-12f);

    vec3  pi    = predicted[i].xyz;
    float lam_i = lambda_[i];
    uint  nc    = neighborCount[i];
    vec3  dp    = vec3(0.0f);

    for (uint k = 0u; k < nc; k++) {
        uint  j   = neighborIdx[i * u_MaxNeighbors + k];
        vec3  rij = pi - predicted[j].xyz;
        float r   = length(rij);

        float scorr = 0.0f;
        if (useScorr) {
            float ratio = W_poly6(r * r, h2, poly6Coeff) / Wdq;
            float r2    = ratio * ratio;
            float r4    = r2 * r2;
            scorr = -u_ScorrK * r4;
        }

        vec3 gw = gradW_spiky(rij, r, h, spikyCoeff);
        dp += (lam_i + lambda_[j] + scorr) * gw;
    }

    deltaPos[i] = vec4(u_Mass * invRho0 * dp, 0.0f);
}
