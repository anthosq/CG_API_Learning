// fluid_particle_debug.glsl — 流体粒子调试视图
//
// 以 GL_POINTS 渲染流体粒子，按速度大小着色（蓝→绿→黄→红），
// 供调试时直观观察粒子运动状态。
//
// 绑定：
//   binding=0  positionSSBO  vec4[N]  粒子当前位置
//   binding=2  velocitySSBO  vec4[N]  粒子当前速度

#type vertex
#version 430 core

layout(std430, binding = 0) buffer PositionBuffer { vec4 positions[]; };
layout(std430, binding = 2) buffer VelocityBuffer { vec4 velocities[]; };

uniform mat4  u_ViewProjection;
uniform float u_ParticleRadius;
uniform float u_ViewportH;

out vec3  v_Color;
out float v_Speed;

// 速度 → 热力图颜色（蓝 0 → 青 → 绿 → 黄 → 红 高）
vec3 speedToColor(float t) {
    t = clamp(t, 0.0, 1.0);
    vec3 c0 = vec3(0.0, 0.0, 1.0);  // 蓝
    vec3 c1 = vec3(0.0, 1.0, 1.0);  // 青
    vec3 c2 = vec3(0.0, 1.0, 0.0);  // 绿
    vec3 c3 = vec3(1.0, 1.0, 0.0);  // 黄
    vec3 c4 = vec3(1.0, 0.0, 0.0);  // 红
    if (t < 0.25) return mix(c0, c1, t * 4.0);
    if (t < 0.50) return mix(c1, c2, (t - 0.25) * 4.0);
    if (t < 0.75) return mix(c2, c3, (t - 0.50) * 4.0);
    return mix(c3, c4, (t - 0.75) * 4.0);
}

void main() {
    gl_Position = u_ViewProjection * vec4(positions[gl_VertexID].xyz, 1.0);

    float speed  = length(velocities[gl_VertexID].xyz);
    float speedN = clamp(speed / 5.0, 0.0, 1.0);  // 5 m/s 映射为最大红色
    v_Color  = speedToColor(speedN);
    v_Speed  = speedN;

    // clip.w == -view.z，即正值的视空间深度
    float eyeZ   = max(gl_Position.w, 0.01);
    gl_PointSize = max((u_ParticleRadius / eyeZ) * u_ViewportH * 0.5, 2.0);
}

#type fragment
#version 430 core

layout(location = 0) out vec4 out_Color;

in vec3  v_Color;
in float v_Speed;

void main() {
    // 圆形裁剪
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(uv, uv);
    if (r2 > 1.0) discard;

    // 边缘软化 + 速度影响透明度
    float alpha = (1.0 - sqrt(r2)) * mix(0.6, 1.0, v_Speed);
    out_Color = vec4(v_Color, alpha);
}
