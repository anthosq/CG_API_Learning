// particle_render.glsl — 粒子 Billboard 渲染
// 无 VAO：gl_VertexID 从 AliveList + ParticleSSBO 拉取数据生成 Quad
// 每粒子 6 个顶点（2 个三角形，CCW），加法混合到 HDR FBO

#type vertex
#version 430 core

struct GPUParticle {
    vec4 Position;  // xyz=世界坐标, w=当前剩余生命
    vec4 Velocity;
    vec4 Color;     // rgba HDR
    vec4 Params;    // x=大小, y=旋转(rad), z=归一化年龄, w=alive
};

layout(std430, binding = 3) buffer ParticleBuffer { GPUParticle Particles[]; };
layout(std430, binding = 4) buffer AliveList      { uint Alive[]; };

uniform mat4 u_ViewProjection;
uniform mat4 u_View;

out vec4 v_Color;
out vec2 v_UV;

// Quad 顶点偏移（CCW，两个三角形）
// 2--3
// |\ |
// | \|
// 0--1
const vec2 quadPos[6] = vec2[](
    vec2(-0.5, -0.5),
    vec2( 0.5, -0.5),
    vec2(-0.5,  0.5),
    vec2(-0.5,  0.5),
    vec2( 0.5, -0.5),
    vec2( 0.5,  0.5)
);

void main() {
    uint particleIdx = Alive[gl_VertexID / 6];
    GPUParticle p    = Particles[particleIdx];

    vec2 localOffset = quadPos[gl_VertexID % 6];

    // 旋转（绕相机朝向轴）
    float cosR = cos(p.Params.y);
    float sinR = sin(p.Params.y);
    vec2 rotated = vec2(
        cosR * localOffset.x - sinR * localOffset.y,
        sinR * localOffset.x + cosR * localOffset.y
    );

    // Spherical Billboard：View 矩阵列向量即世界空间相机轴
    vec3 right = vec3(u_View[0][0], u_View[1][0], u_View[2][0]);
    vec3 up    = vec3(u_View[0][1], u_View[1][1], u_View[2][1]);

    vec3 worldPos = p.Position.xyz
                  + right * rotated.x * p.Params.x
                  + up    * rotated.y * p.Params.x;

    gl_Position = u_ViewProjection * vec4(worldPos, 1.0);
    v_Color = p.Color;
    v_UV    = localOffset + 0.5;  // [0,1]²
}

// ─────────────────────────────────────────────────────────────────────────────

#type fragment
#version 430 core

in vec4 v_Color;
in vec2 v_UV;

uniform sampler2D u_Texture;    // 粒子纹理（可选）
uniform int       u_HasTexture; // 0 = 纯色，1 = 采样纹理

out vec4 o_Color;

void main() {
    // 圆形软边缘（无纹理时）
    vec2 uv = v_UV * 2.0 - 1.0;  // [-1,1]²
    float dist = dot(uv, uv);

    vec4 color = v_Color;

    if (u_HasTexture == 1) {
        color *= texture(u_Texture, v_UV);
    } else {
        // 高斯软边缘
        float alpha = exp(-dist * 3.0) * v_Color.a;
        color.a = alpha;
    }

    if (color.a < 0.01) discard;

    o_Color = color;
}
