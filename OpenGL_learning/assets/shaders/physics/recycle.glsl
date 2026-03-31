#version 430 core

// recycle.glsl — EmitterFluidSimulation：粒子回收
//
// 每个线程处理一个槽位：
//   - lifetime > 0：递减 dt；若到期（≤ 0）→ 移到哨兵位置，压回 free list
//   - lifetime = 0：已死亡槽位，跳过（避免重复压栈）

layout(local_size_x = 256) in;

layout(std430, binding = 0)  buffer Positions  { vec4 positions[]; };
layout(std430, binding = 1)  buffer Predicted  { vec4 predicted[]; };
layout(std430, binding = 2)  buffer Velocities { vec4 velocities[]; };
layout(std430, binding = 14) buffer LifeTime   { float lifetime[]; };
layout(std430, binding = 15) buffer FreeList   { uint freeList[]; };
layout(std430, binding = 16) buffer FreeCount  { uint freeCount; };

uniform uint  u_MaxParticles;
uniform float u_DeltaTime;

// 域外哨兵位置：死亡粒子移到远离仿真域的区域，不参与 hash/邻居搜索
const vec3 SENTINEL = vec3(-9999.0);

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= u_MaxParticles) return;

    float lt = lifetime[i];
    if (lt <= 0.0) return;   // 已死亡的槽位跳过（lifetime=0 是唯一死亡标志）

    lt -= u_DeltaTime;
    lifetime[i] = lt;

    if (lt <= 0.0) {
        // 粒子到期：移到哨兵位置，清零速度，lifetime 设为 0（死亡标志）
        positions[i]  = vec4(SENTINEL, 1.0);
        predicted[i]  = vec4(SENTINEL, 1.0);
        velocities[i] = vec4(0.0);
        lifetime[i]   = 0.0;

        // 无锁 push
        uint slot = atomicAdd(freeCount, 1u);
        freeList[slot] = i;
    }
}
