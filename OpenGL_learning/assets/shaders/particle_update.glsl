// particle_update.glsl — 粒子更新 Compute Shader
// 遍历所有槽，更新存活粒子的物理状态；死亡粒子压回 DeadList

#type compute
#version 430 core

layout(local_size_x = 64) in;

struct GPUParticle {
    vec4 Position;  // xyz=世界坐标, w=当前剩余生命(秒)
    vec4 Velocity;  // xyz=速度,     w=总生命(秒)
    vec4 Color;     // rgba HDR
    vec4 Params;    // x=大小, y=旋转(rad), z=归一化年龄[0,1], w=alive(0/1)
};

layout(std430, binding = 3) buffer ParticleBuffer { GPUParticle Particles[]; };
layout(std430, binding = 5) buffer DeadList        { uint Dead[]; };
layout(std430, binding = 6) buffer Counters        { uint AliveCount; uint DeadCount; uint EmitCount; uint _pad; };

uniform float u_DeltaTime;
uniform int   u_MaxParticles;
uniform vec4  u_Gravity;
uniform vec4  u_ColorBegin;
uniform vec4  u_ColorEnd;
uniform float u_SizeBegin;
uniform float u_SizeEnd;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= uint(u_MaxParticles)) return;

    GPUParticle p = Particles[idx];
    if (p.Params.w < 0.5) return;  // 死亡槽，跳过

    float life      = p.Position.w - u_DeltaTime;
    float totalLife = p.Velocity.w;

    if (life <= 0.0) {
        // 标记死亡，压回 DeadList
        p.Params.w = 0.0;
        Particles[idx] = p;

        uint deadSlot = atomicAdd(DeadCount, 1u);
        Dead[deadSlot] = idx;
        return;
    }

    float age = 1.0 - clamp(life / totalLife, 0.0, 1.0);  // [0=新生, 1=即将消亡]

    // Euler 积分
    p.Velocity.xyz += u_Gravity.xyz * u_DeltaTime;
    p.Position.xyz += p.Velocity.xyz * u_DeltaTime;
    p.Position.w    = life;

    // 插值 Color / Size
    p.Color    = mix(u_ColorBegin, u_ColorEnd, age);
    p.Params.x = mix(u_SizeBegin,  u_SizeEnd,  age);
    p.Params.z = age;

    Particles[idx] = p;
}
