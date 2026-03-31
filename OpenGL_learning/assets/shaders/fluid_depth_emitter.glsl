// fluid_depth_emitter.glsl — EmitterFluidSimulation 深度 Pass
//
// 与 fluid_depth.glsl 完全相同，额外在 vertex shader 开头读取 binding 14（lifetime），
// 将死亡粒子（lifetime == 0）送到远裁剪面外剔除。

#type vertex
#version 430 core

layout(std430, binding = 0)  buffer PositionBuffer { vec4 positions[]; };
layout(std430, binding = 14) buffer LifeTimeBuffer { float lifetime[]; };

uniform mat4  u_View;
uniform mat4  u_Proj;
uniform float u_ParticleRadius;
uniform float u_ViewportH;

out vec3  v_ViewPos;

void main() {
    // lifetime == 0：死亡/空闲槽位，送到远裁剪面外裁掉
    if (lifetime[gl_VertexID] == 0.0) {
        gl_Position  = vec4(0.0, 0.0, 2.0, 1.0);
        gl_PointSize = 0.0;
        return;
    }

    vec3  worldPos = positions[gl_VertexID].xyz;
    vec4  viewPos4 = u_View * vec4(worldPos, 1.0);
    v_ViewPos      = viewPos4.xyz;
    gl_Position    = u_Proj * viewPos4;

    float eyeZ     = -viewPos4.z;
    gl_PointSize   = max(u_ParticleRadius * u_Proj[1][1] * u_ViewportH / eyeZ, 1.0);
}

#type fragment
#version 430 core

layout(location = 0) out float out_Thickness;

in vec3 v_ViewPos;

uniform mat4  u_Proj;
uniform float u_ParticleRadius;   // 渲染半径（含 RenderRadiusScale，用于球面重建）
uniform float u_PhysicsRadius;    // 物理半径（仅用于 Beer-Lambert 厚度贡献）

void main() {
    vec2 uv = vec2(gl_PointCoord.x * 2.0 - 1.0,
                   1.0 - gl_PointCoord.y * 2.0);
    float r2 = dot(uv, uv);
    if (r2 > 1.0) discard;

    float dz = sqrt(1.0 - r2);

    // 球面几何用渲染半径（保证深度 impostor 正确）
    vec3 spherePos = v_ViewPos + vec3(uv, dz) * u_ParticleRadius;

    vec4 clip     = u_Proj * vec4(spherePos, 1.0);
    gl_FragDepth  = (clip.z / clip.w) * 0.5 + 0.5;

    // 厚度用物理半径：解耦视觉尺寸与 Beer-Lambert 吸收强度
    out_Thickness = dz * 2.0 * u_PhysicsRadius;
}
