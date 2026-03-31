// fluid_particle_debug.glsl — 流体粒子调试视图（Sphere Impostor）
//
// 以 GL_POINTS 渲染流体粒子，球面 impostor + 真实深度写入 + Phong 光照。
// 速度着色（FluidComponent）/ 生命周期着色（EmitterFluidSimulation）。
//
// 绑定：
//   binding=0   positionSSBO  vec4[N]   粒子位置
//   binding=2   velocitySSBO  vec4[N]   粒子速度（u_HasVelocity=1 时有效）
//   binding=14  lifetimeSSBO  float[N]  粒子生命（u_HasLifetime=1 时有效）

// ─── Vertex ───────────────────────────────────────────────────────────────
#type vertex
#version 430 core

layout(std430, binding = 0)  buffer PositionBuffer { vec4 positions[];  };
layout(std430, binding = 2)  buffer VelocityBuffer { vec4 velocities[]; };
layout(std430, binding = 14) buffer LifetimeBuffer { float lifetime[];   };

uniform mat4  u_View;
uniform mat4  u_Projection;
uniform float u_ParticleRadius;
uniform float u_ViewportH;
uniform int   u_HasLifetime;  // 1 = emitter 粒子，按 lifetime 剔除/着色
uniform int   u_HasVelocity;  // 1 = 有速度 SSBO，按速度着色

out vec3  v_Color;
out float v_Alpha;
out vec3  v_ViewCenter;   // 视空间球心
out float v_Radius;

// 速度热力图：蓝 0 → 青 → 绿 → 黄 → 红 高
vec3 speedToColor(float t) {
    t = clamp(t, 0.0, 1.0);
    if (t < 0.25) return mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 1.0), t * 4.0);
    if (t < 0.50) return mix(vec3(0.0, 1.0, 1.0), vec3(0.0, 1.0, 0.0), (t - 0.25) * 4.0);
    if (t < 0.75) return mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 1.0, 0.0), (t - 0.50) * 4.0);
    return           mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (t - 0.75) * 4.0);
}

void main() {
    // 死亡粒子（emitter 专用）→ 剔除
    if (u_HasLifetime == 1 && lifetime[gl_VertexID] == 0.0) {
        gl_Position  = vec4(0.0, 0.0, 2.0, 1.0);
        gl_PointSize = 0.0;
        v_Alpha      = 0.0;
        v_Color      = vec3(0.0);
        v_ViewCenter = vec3(0.0);
        v_Radius     = 0.0;
        return;
    }

    vec4 viewPos = u_View * vec4(positions[gl_VertexID].xyz, 1.0);
    gl_Position  = u_Projection * viewPos;

    // 投影点尺寸：projH = cot(fov/2) = Projection[1][1]
    float eyeZ   = max(-viewPos.z, 0.01);
    float projH  = abs(u_Projection[1][1]);
    gl_PointSize = max((u_ParticleRadius / eyeZ) * projH * u_ViewportH * 0.5, 2.0);

    v_ViewCenter = viewPos.xyz;
    v_Radius     = u_ParticleRadius;

    if (u_HasVelocity == 1) {
        float speed = length(velocities[gl_VertexID].xyz);
        v_Color     = speedToColor(clamp(speed / 5.0, 0.0, 1.0));
    } else {
        // 无速度数据 → 按生命周期着色：新生=蓝，衰老=橙
        float lt = u_HasLifetime == 1 ? clamp(lifetime[gl_VertexID] / 3.0, 0.0, 1.0) : 0.5;
        v_Color  = mix(vec3(1.0, 0.3, 0.0), vec3(0.2, 0.5, 1.0), lt);
    }
    v_Alpha = 1.0;
}

// ─── Fragment ─────────────────────────────────────────────────────────────
#type fragment
#version 430 core

layout(location = 0) out vec4 out_Color;

in vec3  v_Color;
in float v_Alpha;
in vec3  v_ViewCenter;
in float v_Radius;

uniform mat4 u_Projection;

void main() {
    if (v_Alpha == 0.0) discard;

    // 将 PointCoord [0,1] 映射到 [-1,1]
    vec2  uv = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(uv, uv);
    if (r2 > 1.0) discard;

    // 球面法线（视空间，z 轴朝向摄像机）
    float z      = sqrt(1.0 - r2);
    vec3  normal = vec3(uv, z);

    // 真实击中点（视空间）
    vec3 viewHit = v_ViewCenter + normal * v_Radius;

    // 写入准确深度
    vec4  clip      = u_Projection * vec4(viewHit, 1.0);
    gl_FragDepth    = (clip.z / clip.w) * 0.5 + 0.5;

    // 简单 Phong（视空间光方向，类似从相机右上方照射）
    vec3  L    = normalize(vec3(0.4, 0.6, 1.0));
    float diff = max(dot(normal, L), 0.0);
    float spec = pow(max(normal.z, 0.0), 48.0);

    vec3 color = v_Color * (0.15 + 0.75 * diff) + vec3(0.6) * spec;
    out_Color  = vec4(color, v_Alpha);
}
