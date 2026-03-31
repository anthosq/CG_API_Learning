#version 430 core

// apply_delta.glsl — Phase 11-D：应用位置修正量 + 边界硬约束
//
// predicted[i] += deltaPos[i]
// clamp 到 [BoundaryMin + pad, BoundaryMax - pad]
//
// u_BoundaryPadding = 2.5 * h，确保边界粒子有完整邻居支持，避免密度塌缩

layout(local_size_x = 256) in;

layout(std430, binding = 1)  buffer PredictedPos { vec4 predicted[]; };
layout(std430, binding = 13) buffer DeltaPos     { vec4 deltaPos[]; };

uniform vec3  u_BoundaryMin;
uniform vec3  u_BoundaryMax;
uniform float u_BoundaryPadding;  // = 2.5 * kernelRadius，典型 0.1
uniform uint  u_ParticleCount;

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= u_ParticleCount) return;

    vec3 pred = predicted[i].xyz + deltaPos[i].xyz;

    pred = clamp(pred, u_BoundaryMin + u_BoundaryPadding, u_BoundaryMax - u_BoundaryPadding);

    predicted[i] = vec4(pred, predicted[i].w);
}
