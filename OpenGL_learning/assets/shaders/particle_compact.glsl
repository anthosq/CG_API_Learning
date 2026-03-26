// particle_compact.glsl — 存活粒子收集 Compute Shader
// 遍历所有槽，将 alive=1 的粒子下标写入 AliveList，同时更新 IndirectBuffer

#type compute
#version 430 core

layout(local_size_x = 64) in;

struct GPUParticle {
    vec4 Position;
    vec4 Velocity;
    vec4 Color;
    vec4 Params;    // w = alive(0/1)
};

struct DrawArraysIndirectCommand {
    uint count;
    uint primCount;
    uint first;
    uint baseInstance;
};

layout(std430, binding = 3) buffer ParticleBuffer { GPUParticle Particles[]; };
layout(std430, binding = 4) buffer AliveList      { uint Alive[]; };
layout(std430, binding = 6) buffer Counters       { uint AliveCount; uint DeadCount; uint EmitCount; uint _pad; };
layout(std430, binding = 7) buffer IndirectBuffer { DrawArraysIndirectCommand Indirect; };

uniform int u_MaxParticles;

void main() {
    uint idx = gl_GlobalInvocationID.x;

    // 第一个线程初始化 Indirect（避免残留上帧数据）
    if (idx == 0u) {
        Indirect.primCount    = 1u;
        Indirect.first        = 0u;
        Indirect.baseInstance = 0u;
    }

    barrier();
    memoryBarrierBuffer();

    if (idx >= uint(u_MaxParticles)) return;

    if (Particles[idx].Params.w > 0.5) {
        uint slot  = atomicAdd(AliveCount, 1u);
        Alive[slot] = idx;
    }

    barrier();
    memoryBarrierBuffer();

    // 最后一个有效线程写 count（workgroup 内所有线程已完成 atomicAdd）
    // 注意：跨 workgroup 的 barrier 不存在，此处仅在每个 workgroup 内保序
    // CPU 端需在 Compact Dispatch 后手动将 aliveCount * 6 写入 Indirect.count
    // 此处写入是 best-effort（单 workgroup 内正确，多 workgroup 场景由 CPU 覆盖）
    if (idx == 0u) {
        Indirect.count = AliveCount * 6u;
    }
}
