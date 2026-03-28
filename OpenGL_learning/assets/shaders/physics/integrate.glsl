#version 430 core

// integrate.glsl — PBF Phase 1：外力积分 + 预测位置
//
// 每个线程处理一个粒子：
//   v += gravity * dt / mass
//   p_pred = p + v * dt
//
// 对应 XPBD 积分步：
//   v* = v + dt * f_ext / m
//   x* = x + dt * v*

layout(local_size_x = 256) in;

layout(std430, binding = 0) buffer ParticlePositions { vec4 positions[]; };
layout(std430, binding = 1) buffer PositionPredict   { vec4 predicted[]; };
layout(std430, binding = 2) buffer Velocities        { vec4 velocities[]; };

uniform vec3  u_Gravity;         // 重力加速度，单位 m/s²，典型值 (0, -9.8, 0)
uniform float u_DeltaTime;       // substep dt
uniform uint  u_ParticleCount;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_ParticleCount) return;

    // v* = v + a_ext * dt   （a = F/m，重力加速度与质量无关）
    velocities[idx].xyz += u_Gravity * u_DeltaTime;
    predicted[idx].xyz   = positions[idx].xyz + velocities[idx].xyz * u_DeltaTime;
}
