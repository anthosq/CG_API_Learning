#version 430 core

// vorticity.glsl — Phase 11-E：涡旋约束（Vorticity Confinement）
//
// 补偿数值阻尼导致的涡旋能量损失。
//
// 算法（Macklin & Müller 2013, Section 4）：
//   ωᵢ = Σⱼ (vⱼ - vᵢ) × ∇W_spiky(pᵢ - pⱼ)   涡度向量
//   ηᵢ = Σⱼ |ωⱼ| 方向的梯度（位置梯度）
//   fᵢ = ε · (η̂ᵢ × ωᵢ)                       涡旋修正力
//   vᵢ += fᵢ · dt
//
// ε 为涡旋强度（u_VorticityEps），设为 0 则禁用此 pass。

layout(local_size_x = 256) in;

layout(std430, binding = 0) buffer Position     { vec4 position[]; };
layout(std430, binding = 2) buffer Velocity     { vec4 velocity[]; };
layout(std430, binding = 7) buffer NeighborCount{ uint neighborCount[]; };
layout(std430, binding = 8) buffer NeighborIdx  { uint neighborIdx[]; };

uniform float u_KernelRadius;
uniform float u_VorticityEps;    // 涡旋强度，0 = 禁用
uniform float u_DeltaTime;
uniform uint  u_ParticleCount;
uniform uint  u_MaxNeighbors;

const float PI = 3.14159265358979f;

vec3 gradW_spiky(vec3 rij, float r, float h, float gradCoeff) {
    if (r <= 1e-5f || r >= h) return vec3(0.0f);
    float d = h - r;
    return gradCoeff * d * d * (rij / r);
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= u_ParticleCount) return;

    float h  = u_KernelRadius;
    float h2 = h * h;
    float h4 = h2 * h2;
    float h6 = h4 * h2;
    float spikyCoeff = -45.0f / (PI * h6);

    vec3  pi = position[i].xyz;
    vec3  vi = velocity[i].xyz;
    uint  nc = neighborCount[i];

    // ① 计算局部涡度 ωᵢ = Σⱼ (vⱼ - vᵢ) × ∇W(pᵢ - pⱼ)
    vec3 omega = vec3(0.0f);
    for (uint k = 0u; k < nc; k++) {
        uint  j   = neighborIdx[i * u_MaxNeighbors + k];
        vec3  rij = pi - position[j].xyz;
        float r   = length(rij);
        vec3  dv  = velocity[j].xyz - vi;
        vec3  gw  = gradW_spiky(rij, r, h, spikyCoeff);
        omega += cross(dv, gw);
    }

    float omegaLen = length(omega);
    if (omegaLen < 1e-6f) {
        return;  // 无涡度，跳过
    }

    // ② 计算 η = ∇|ω|（用邻居的 |ω| 梯度近似）
    // 由于 |ω| 需要单独缓冲区，这里用邻居速度叉积估算
    // 简化实现：η = Σⱼ |ωⱼ_approx| * ∇W(rᵢⱼ)，ωⱼ ≈ (vⱼ-vᵢ)×∇W
    vec3 eta = vec3(0.0f);
    for (uint k = 0u; k < nc; k++) {
        uint  j    = neighborIdx[i * u_MaxNeighbors + k];
        vec3  rij  = pi - position[j].xyz;
        float r    = length(rij);
        vec3  dv   = velocity[j].xyz - vi;
        vec3  gw   = gradW_spiky(rij, r, h, spikyCoeff);
        vec3  omgJ = cross(dv, gw);
        eta += length(omgJ) * gw;
    }

    // ③ 涡旋力 f = ε · (η̂ × ω)
    float etaLen = length(eta);
    if (etaLen < 1e-6f) return;

    vec3 N = eta / etaLen;                       // η̂

    // SPH 离散求和缺少体积元 h³（连续积分的 dV），补偿后 omega 量纲为 [1/s]，
    // ε 在 0~5 范围内具有直观意义，与 XSPH 乘 1/ρ₀ 的归一化思路一致。
    float h3 = h * h2;
    vec3 fVort = u_VorticityEps * cross(N, omega * h3);

    velocity[i] = vec4(vi + fVort * u_DeltaTime, 0.0f);
}
