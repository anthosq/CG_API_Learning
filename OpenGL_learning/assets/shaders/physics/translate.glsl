#version 430 core

// translate.glsl — 平移所有粒子
//
// 当流体 Entity 发生纯平移时（Domain 大小不变，只有位置改变），
// 对 positions 和 predicted 同步施加相同偏移，保留速度场不变。
// BoundaryMin/Max 由 CPU 端 SetBoundary() 在调用前已更新。

layout(local_size_x = 256) in;

layout(std430, binding = 0) buffer Positions  { vec4 positions[]; };
layout(std430, binding = 1) buffer Predicted  { vec4 predicted[]; };

uniform vec3 u_Delta;
uniform uint u_ParticleCount;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= u_ParticleCount) return;

    positions[i].xyz += u_Delta;
    predicted[i].xyz += u_Delta;
}
