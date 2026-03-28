#version 430 core

// velocity_update.glsl — Phase 11-D：约束迭代结束后更新速度和位置
//
// vᵢ = (pᵢ* - xᵢ) / dt
// 碰墙时法向速度镜像反射（RESTITUTION = 1.0，与参考实现一致）
// xᵢ = pᵢ*

layout(local_size_x = 256) in;

layout(std430, binding = 0) buffer Position     { vec4 position[]; };
layout(std430, binding = 1) buffer PredictedPos { vec4 predicted[]; };
layout(std430, binding = 2) buffer Velocity     { vec4 velocity[]; };

uniform float u_DeltaTime;
uniform uint  u_ParticleCount;
uniform vec3  u_BoundaryMin;
uniform vec3  u_BoundaryMax;
uniform float u_BoundaryPadding;  // = 2.5 * kernelRadius，与 apply_delta 一致

// 参考实现：RESTITUTION=1.0（完全弹性），FRICTION=1.0（切向无摩擦）
uniform float u_Restitution;   // 法向速度反弹系数，默认 1.0

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= u_ParticleCount) return;

    vec3 pos  = position[i].xyz;
    vec3 pred = predicted[i].xyz;
    vec3 v    = (pred - pos) / u_DeltaTime;

    // 碰撞反射：紧贴边界时仅反转法向分量，不衰减
    float pad = u_BoundaryPadding;
    if (pred.x <= u_BoundaryMin.x + pad && v.x < 0.0f) v.x *= -u_Restitution;
    if (pred.x >= u_BoundaryMax.x - pad && v.x > 0.0f) v.x *= -u_Restitution;
    if (pred.y <= u_BoundaryMin.y + pad && v.y < 0.0f) v.y *= -u_Restitution;
    if (pred.y >= u_BoundaryMax.y - pad && v.y > 0.0f) v.y *= -u_Restitution;
    if (pred.z <= u_BoundaryMin.z + pad && v.z < 0.0f) v.z *= -u_Restitution;
    if (pred.z >= u_BoundaryMax.z - pad && v.z > 0.0f) v.z *= -u_Restitution;

    velocity[i] = vec4(v, 0.0f);
    position[i] = vec4(pred, 0.0f);
}
