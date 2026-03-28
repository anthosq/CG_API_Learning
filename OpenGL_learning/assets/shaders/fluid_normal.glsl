// fluid_normal.glsl — Phase 12-B：从平滑深度图重建视空间法线

#type vertex
#version 430 core

out vec2 v_UV;

void main() {
    vec2 uv     = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    v_UV        = uv;
}

#type fragment
#version 430 core

layout(location = 0) out vec4 out_Normal;  // xyz=视空间法线（已归一化），w=1（有流体）

in vec2 v_UV;

uniform sampler2D u_FluidDepthSmooth;
uniform mat4      u_InvProj;
uniform vec2      u_TexelSize;

vec3 reconstructViewPos(vec2 uv, float depth) {
    vec4 ndc  = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = u_InvProj * ndc;
    return view.xyz / view.w;
}

void main() {
    float d = texture(u_FluidDepthSmooth, v_UV).r;

    if (d >= 0.9999) {
        out_Normal = vec4(0.0);  // 背景，无流体
        return;
    }

    // 采样相邻像素，避免跨越流体边界
    float dR = texture(u_FluidDepthSmooth, v_UV + vec2(u_TexelSize.x, 0.0)).r;
    float dU = texture(u_FluidDepthSmooth, v_UV + vec2(0.0, u_TexelSize.y)).r;

    // 深度跳变过大时回退到单侧差分
    if (abs(dR - d) > 0.01) dR = d;
    if (abs(dU - d) > 0.01) dU = d;

    vec3 p  = reconstructViewPos(v_UV,                                    d);
    vec3 px = reconstructViewPos(v_UV + vec2(u_TexelSize.x, 0.0),         dR);
    vec3 py = reconstructViewPos(v_UV + vec2(0.0,           u_TexelSize.y), dU);

    vec3 normal = normalize(cross(px - p, py - p));

    // 确保法线朝向摄像机（视空间 +Z 方向）
    if (normal.z < 0.0) normal = -normal;

    out_Normal = vec4(normal, 1.0);
}
