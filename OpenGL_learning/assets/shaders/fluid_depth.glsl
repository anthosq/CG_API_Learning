// fluid_depth.glsl — Phase 12-A：流体粒子深度 Pass
//
// 以 GL_POINTS 渲染粒子，将每个粒子投影为球形 billboard（Point Sprite），
// 修正 gl_FragDepth 为球面真实深度，并输出厚度贡献（additive 叠加）。
//
// Draw call：glDrawArrays(GL_POINTS, 0, particleCount)
// positionSSBO 绑定在 binding=0，vertex shader 通过 gl_VertexID 读取。

#type vertex
#version 430 core

layout(std430, binding = 0) buffer PositionBuffer { vec4 positions[]; };

uniform mat4  u_View;
uniform mat4  u_Proj;
uniform float u_ParticleRadius;
uniform float u_ViewportH;

out vec3  v_ViewPos;

void main() {
    vec3  worldPos = positions[gl_VertexID].xyz;
    vec4  viewPos4 = u_View * vec4(worldPos, 1.0);
    v_ViewPos      = viewPos4.xyz;
    gl_Position    = u_Proj * viewPos4;

    // 正确投影：point_diameter = 2r / |z| * proj[1][1] * (H/2)
    // gl_PointSize 单位为像素直径，化简得：r / |z| * proj[1][1] * H
    float eyeZ     = -viewPos4.z;
    gl_PointSize   = max(u_ParticleRadius * u_Proj[1][1] * u_ViewportH / eyeZ, 1.0);
}

#type fragment
#version 430 core

layout(location = 0) out float out_Thickness;

in vec3 v_ViewPos;

uniform mat4  u_Proj;
uniform float u_ParticleRadius;

void main() {
    // gl_PointCoord.y：窗口坐标原点在左上角，Y 向下，需翻转
    vec2 uv = vec2(gl_PointCoord.x * 2.0 - 1.0,
                   1.0 - gl_PointCoord.y * 2.0);
    float r2 = dot(uv, uv);
    if (r2 > 1.0) discard;

    float dz = sqrt(1.0 - r2);

    // 球面点（视空间）：从粒子中心沿球面法向偏移 radius
    // 视空间 +Z 朝向摄像机，所以前表面的 Z 更大（更小的负值）
    vec3 spherePos = v_ViewPos + vec3(uv, dz) * u_ParticleRadius;

    // 重投影到裁剪空间，写入精确球面深度
    vec4 clip     = u_Proj * vec4(spherePos, 1.0);
    gl_FragDepth  = (clip.z / clip.w) * 0.5 + 0.5;

    // 厚度：光线穿过球体的估算路径长（各粒子 additive 叠加）
    out_Thickness = dz * 2.0 * u_ParticleRadius;
}
