// scene_collision.glsl — 屏幕空间场景碰撞（Phase 12-G）
//
// 在 integrate 之后、空间哈希重建之前执行：
//   - 将 predicted 位置投影到屏幕
//   - 采样 G-Buffer 深度与法线
//   - 若穿入场景几何 → 沿法线推出 + 反射速度

#version 430 core
layout(local_size_x = 256) in;

layout(std430, binding = 1)  buffer PredBuffer     { vec4  predicted[]; };
layout(std430, binding = 2)  buffer VelBuffer      { vec4  velocity[];  };
layout(std430, binding = 14) buffer LifetimeBuffer { float lifetime[];  };  // emitter 专用

uniform mat4      u_ViewProj;
uniform mat4      u_InvViewProj;
uniform sampler2D u_GDepth;         // texture unit 0：场景深度 [0,1]
uniform sampler2D u_GNormal;        // texture unit 1：GBuffer NormalRoughMetal（Oct in .rg）
uniform float     u_ParticleRadius;
uniform uint      u_ParticleCount;
uniform float     u_Restitution;
uniform int       u_HasLifetime;    // 1 = emitter 粒子，lifetime==0 的跳过

vec3 decodeOct(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f, 1.0 - abs(f.x) - abs(f.y));
    if (n.z < 0.0) n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    return normalize(n);
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= u_ParticleCount) return;

    // 死亡 emitter 粒子在哨兵位置，不参与碰撞
    if (u_HasLifetime == 1 && lifetime[i] == 0.0) return;

    vec3 pred = predicted[i].xyz;

    // 投影到裁剪空间
    vec4 clip = u_ViewProj * vec4(pred, 1.0);
    if (clip.w <= 0.0) return;   // 粒子在摄像机背后

    vec3 ndc = clip.xyz / clip.w;
    // 屏幕外粒子跳过（AABB 边界约束兜底）
    if (any(greaterThan(abs(ndc.xy), vec2(1.02)))) return;

    vec2 uv = ndc.xy * 0.5 + 0.5;
    uv = clamp(uv, vec2(0.001), vec2(0.999));

    // 采样场景深度
    float gbufD = texture(u_GDepth, uv).r;
    if (gbufD >= 0.9999) return;   // 天空/背景，无场景几何

    // 深度比较（NDC 空间）
    // ndc.z 范围 [-1, 1]，gbufD 范围 [0, 1] → 转换为同一空间
    float sceneNDCz = gbufD * 2.0 - 1.0;
    if (ndc.z <= sceneNDCz) return;    // 粒子在场景表面前方，无碰撞

    // 重建场景表面世界坐标
    vec4 surfClip  = vec4(ndc.xy, sceneNDCz, 1.0);
    vec4 surfWorld = u_InvViewProj * surfClip;
    vec3 surfPos   = surfWorld.xyz / surfWorld.w;

    // 解码 GBuffer 法线（世界空间 Octahedral）
    vec3 N = decodeOct(texture(u_GNormal, uv).rg);

    // 确保法线朝向摄像机（剔除背面法线）
    vec3 camPos = (u_InvViewProj * vec4(0.0, 0.0, -1.0, 1.0)).xyz;
    vec3 toCam  = normalize(camPos - surfPos);
    if (dot(N, toCam) < 0.0) N = -N;

    // 推出：球心放到表面上方 radius + 安全距离
    predicted[i].xyz = surfPos + N * (u_ParticleRadius + 1e-3);

    // 速度法向分量反射（仅向表面运动时反射）
    vec3  v  = velocity[i].xyz;
    float vn = dot(v, N);
    if (vn < 0.0)
        velocity[i].xyz = v - (1.0 + u_Restitution) * vn * N;
}
