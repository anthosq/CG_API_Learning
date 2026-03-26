// particle_emit.glsl — 粒子发射 Compute Shader
// 从 DeadList 中分配空闲槽，写入初始粒子状态

#type compute
#version 430 core

layout(local_size_x = 64) in;

// GPUParticle (std430, 64 bytes)
struct GPUParticle {
    vec4 Position;  // xyz=世界坐标, w=当前生命(秒)
    vec4 Velocity;  // xyz=速度,     w=总生命(秒)
    vec4 Color;     // rgba HDR
    vec4 Params;    // x=大小, y=旋转(rad), z=归一化年龄[0,1], w=alive(0/1)
};

layout(std430, binding = 3) buffer ParticleBuffer { GPUParticle Particles[]; };
layout(std430, binding = 5) buffer DeadList        { uint Dead[]; };
layout(std430, binding = 6) buffer Counters        { uint AliveCount; uint DeadCount; uint EmitCount; uint _pad; };

uniform vec4  u_EmitPos;
uniform vec4  u_EmitDir;
uniform vec4  u_ColorBegin;
uniform vec4  u_ColorEnd;
uniform vec4  u_Gravity;
uniform float u_EmitSpread;
uniform float u_LifetimeMin;
uniform float u_LifetimeMax;
uniform float u_SpeedMin;
uniform float u_SpeedMax;
uniform float u_SizeBegin;
uniform float u_SizeEnd;
uniform int   u_EmitCount;
uniform int   u_MaxParticles;

// PCG 伪随机
uint pcg(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}
float rand(inout uint seed) {
    seed = pcg(seed);
    return float(seed) / float(0xFFFFFFFFu);
}

// 在半锥角 spread 内随机方向（围绕 dir）
vec3 RandomConeDir(vec3 dir, float spread, inout uint seed) {
    float cosA = cos(spread);
    float z    = cosA + rand(seed) * (1.0 - cosA);
    float r    = sqrt(max(0.0, 1.0 - z * z));
    float phi  = rand(seed) * 6.283185307;

    // 构建局部坐标系
    vec3 up    = abs(dir.y) < 0.999 ? vec3(0,1,0) : vec3(1,0,0);
    vec3 right = normalize(cross(up, dir));
    vec3 fwd   = cross(dir, right);
    return normalize(right * (r * cos(phi)) + fwd * (r * sin(phi)) + dir * z);
}

void main() {
    uint tid = gl_GlobalInvocationID.x;
    if (tid >= uint(u_EmitCount)) return;

    // 从 DeadList 末尾取槽（原子递减）
    uint deadIdx = atomicAdd(DeadCount, 0xFFFFFFFFu); // atomicAdd(x, -1) via wraparound
    if (deadIdx == 0u) return; // dead list 为空
    uint slot = Dead[deadIdx - 1u];

    if (slot >= uint(u_MaxParticles)) return;

    // 随机种子：tid + slot 混合
    uint seed = pcg(tid * 1013u + slot * 3571u + 1u);

    float lifetime = u_LifetimeMin + rand(seed) * (u_LifetimeMax - u_LifetimeMin);
    float speed    = u_SpeedMin    + rand(seed) * (u_SpeedMax    - u_SpeedMin);

    vec3 dir = RandomConeDir(normalize(u_EmitDir.xyz), u_EmitSpread, seed);
    vec3 vel = dir * speed;

    GPUParticle p;
    p.Position = vec4(u_EmitPos.xyz, lifetime);  // w = 当前剩余生命
    p.Velocity = vec4(vel, lifetime);             // w = 总生命（不变）
    p.Color    = u_ColorBegin;
    p.Params   = vec4(u_SizeBegin, rand(seed) * 6.283185307, 0.0, 1.0); // alive=1

    Particles[slot] = p;
}
