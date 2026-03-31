#version 430 core

// emitter_integrate.glsl — EmitterFluidSimulation Phase 1：外力积分 + 预测位置
//
// 与 integrate.glsl 完全相同，但增加 binding 14 lifetime 检查：
//   lifetime == 0 的死亡/空闲槽位跳过积分，保持哨兵位置不变。

layout(local_size_x = 256) in;

layout(std430, binding = 0)  buffer ParticlePositions { vec4 positions[]; };
layout(std430, binding = 1)  buffer PositionPredict   { vec4 predicted[]; };
layout(std430, binding = 2)  buffer Velocities        { vec4 velocities[]; };
layout(std430, binding = 14) buffer LifeTime          { float lifetime[]; };

uniform vec3  u_Gravity;
uniform float u_DeltaTime;
uniform uint  u_ParticleCount;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_ParticleCount) return;
    if (lifetime[idx] == 0.0) return;   // 死亡槽位：保持哨兵位置，不参与积分

    velocities[idx].xyz += u_Gravity * u_DeltaTime;
    predicted[idx].xyz   = positions[idx].xyz + velocities[idx].xyz * u_DeltaTime;
}
