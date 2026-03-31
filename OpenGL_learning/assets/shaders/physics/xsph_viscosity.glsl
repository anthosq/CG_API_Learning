#version 430 core

// xsph_viscosity.glsl — Phase 11-E：XSPH 粘性修正
//
// 对每个粒子的速度做 SPH 邻域加权平均：
//   vᵢ += c · Σⱼ (vⱼ - vᵢ) W_poly6(|pᵢ - pⱼ|, h)
//
// c 为粘性系数（典型值 0.01~0.1），值越大粒子速度越趋同。
// 结果写入 velocitySSBO，供下一帧 integrate 使用。

layout(local_size_x = 256) in;

layout(std430, binding = 0) buffer Position     { vec4 position[]; };
layout(std430, binding = 2) buffer Velocity     { vec4 velocity[]; };
layout(std430, binding = 7) buffer NeighborCount{ uint neighborCount[]; };
layout(std430, binding = 8) buffer NeighborIdx  { uint neighborIdx[]; };

uniform float u_KernelRadius;
uniform float u_Viscosity;        // c，典型值 0.01（已含 1/ρ₀ 归一化）
uniform float u_RestDensity;      // ρ₀，用于 XSPH 归一化
uniform uint  u_ParticleCount;
uniform uint  u_MaxNeighbors;

const float PI = 3.14159265358979f;

float W_poly6(float r2, float h2, float coeff) {
    if (r2 >= h2) return 0.0f;
    float d = h2 - r2;
    return coeff * d * d * d;
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= u_ParticleCount) return;

    float h  = u_KernelRadius;
    float h2 = h * h;
    float h4 = h2 * h2;
    float h8 = h4 * h4;
    float h9 = h8 * h;
    float poly6Coeff = 315.0f / (64.0f * PI * h9);

    vec3  pi = position[i].xyz;
    vec3  vi = velocity[i].xyz;
    uint  nc = neighborCount[i];
    vec3  dv = vec3(0.0f);

    for (uint k = 0u; k < nc; k++) {
        uint  j   = neighborIdx[i * u_MaxNeighbors + k];
        vec3  rij = pi - position[j].xyz;
        float r2  = dot(rij, rij);
        float w   = W_poly6(r2, h2, poly6Coeff);
        dv += (velocity[j].xyz - vi) * w;
    }

    // 参考公式：Δv = c * (1/ρ₀) * Σⱼ (vⱼ - vᵢ) W
    float invRho0 = 1.0f / u_RestDensity;
    velocity[i] = vec4(vi + u_Viscosity * invRho0 * dv, 0.0f);
}
