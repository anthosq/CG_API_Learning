#version 430 core

// translate.glsl — 平移所有粒子并施加惯性冲量
//
// 当流体 Entity 发生纯平移时（Domain 大小不变，只有位置改变），
// 对 positions / predicted 施加相同偏移（容器随 Entity 移动），
// 同时对 velocity 施加 u_InertialImpulse = -delta / frame_dt，
// 模拟流体惯性：容器移动时流体"落后"，在容器参考系内获得反向初速度，产生晃动。
// BoundaryMin/Max 由 CPU 端 TranslateDomain() 在调用前已更新。

layout(local_size_x = 256) in;

layout(std430, binding = 0) buffer Positions  { vec4 positions[]; };
layout(std430, binding = 1) buffer Predicted  { vec4 predicted[]; };
layout(std430, binding = 2) buffer Velocity   { vec4 velocity[]; };

uniform vec3 u_Delta;
uniform vec3 u_InertialImpulse;  // = -u_Delta / frame_dt，惯性反向冲量
uniform uint u_ParticleCount;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= u_ParticleCount) return;

    positions[i].xyz += u_Delta;
    predicted[i].xyz += u_Delta;
    velocity[i].xyz  += u_InertialImpulse;
}
