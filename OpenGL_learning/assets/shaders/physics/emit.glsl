#version 430 core

// emit.glsl — EmitterFluidSimulation：粒子发射
//
// 每个线程发射一个粒子：
//   1. 原子弹出 free list 栈顶，获取空闲槽位索引
//   2. 在槽位上初始化 position / predicted / velocity / lifetime / color
//   3. 方向按锥形均匀分布（Wang Hash 伪随机，以线程 ID + 帧号为种子）

layout(local_size_x = 64) in;

layout(std430, binding = 0)  buffer Positions  { vec4 positions[]; };
layout(std430, binding = 1)  buffer Predicted  { vec4 predicted[]; };
layout(std430, binding = 2)  buffer Velocities { vec4 velocities[]; };
layout(std430, binding = 14) buffer LifeTime   { float lifetime[]; };
layout(std430, binding = 15) buffer FreeList   { uint freeList[]; };
layout(std430, binding = 16) buffer FreeCount  { uint freeCount; };
layout(std430, binding = 17) buffer Colors     { vec4 color[]; };

uniform uint  u_EmitCount;       // 本次发射粒子数（dispatch 时对齐到 64）
uniform vec3  u_EmitPos;         // 发射点（世界空间）
uniform vec3  u_EmitDir;         // 发射方向（归一化）
uniform float u_EmitSpeed;       // 初始速度（m/s）
uniform float u_Lifetime;        // 粒子存活时间（秒）
uniform float u_ConeAngle;       // 锥形半角（弧度）
uniform vec4  u_EmitColor;       // 粒子颜色
uniform uint  u_FrameSeed;       // 帧级随机种子（CPU 传入，防止每帧重复）

// 低差异伪随机（Wang Hash）
uint wangHash(uint n) {
    n = (n ^ 61u) ^ (n >> 16u);
    n *= 9u;
    n ^= n >> 4u;
    n *= 0x27d4eb2du;
    n ^= n >> 15u;
    return n;
}
float rand01(uint seed) {
    return float(wangHash(seed) & 0x00ffffffu) / float(0x01000000u);
}

// 在以 dir 为轴的锥体内均匀采样一个方向
vec3 sampleCone(vec3 dir, float halfAngle, uint seed) {
    float cosMax = cos(halfAngle);
    float cosA   = mix(cosMax, 1.0, rand01(seed));
    float phi    = rand01(seed + 1u) * 6.2831853;
    float sinA   = sqrt(max(0.0, 1.0 - cosA * cosA));

    vec3 up   = abs(dir.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tang = normalize(cross(up, dir));
    vec3 bitg = cross(dir, tang);

    return cosA * dir + sinA * (cos(phi) * tang + sin(phi) * bitg);
}

void main() {
    uint tid = gl_GlobalInvocationID.x;
    if (tid >= u_EmitCount) return;

    // 无锁 pop：原子减 1，prev 是减前的值
    uint prev = atomicAdd(freeCount, uint(-1));
    if (prev == 0u) {
        atomicAdd(freeCount, 1u);  // 溢出：池已满，还原计数
        return;
    }
    uint idx = freeList[prev - 1u];

    // Wang Hash 伪随机发射方向
    uint seed = wangHash(tid + u_FrameSeed * 65537u);
    vec3 dir  = sampleCone(normalize(u_EmitDir), u_ConeAngle, seed);

    positions[idx]  = vec4(u_EmitPos, 1.0);
    predicted[idx]  = vec4(u_EmitPos, 1.0);
    velocities[idx] = vec4(dir * u_EmitSpeed, 0.0);
    lifetime[idx]   = u_Lifetime;
    color[idx]      = u_EmitColor;
}
