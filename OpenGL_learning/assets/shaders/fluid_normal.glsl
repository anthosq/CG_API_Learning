// fluid_normal.glsl — Phase 12-D：法线重建
//
// 前提：双边滤波已多次迭代（sigma_d=1.0，iters=5），深度图足够平滑。
// 策略（与 PositionBasedFluids/computeFluidNormal.comp 对齐）：
//   - 对每个方向比较前向/后向的 Z 分量差（深度差），取深度差更小的一侧
//   - 用原始切向量叉积后归一化，NaN 由邻域修复处理

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

bool isValidNormal(vec3 n) {
    return !any(isnan(n)) && dot(n, n) > 1e-6;
}

vec3 computeNormal(vec2 uv) {
    float d = texture(u_FluidDepthSmooth, uv).r;
    if (d >= 0.9999) return vec3(0.0);

    vec3 p  = reconstructViewPos(uv, d);

    float dR = texture(u_FluidDepthSmooth, uv + vec2( u_TexelSize.x, 0.0)).r;
    float dL = texture(u_FluidDepthSmooth, uv + vec2(-u_TexelSize.x, 0.0)).r;
    float dU = texture(u_FluidDepthSmooth, uv + vec2(0.0,  u_TexelSize.y)).r;
    float dD = texture(u_FluidDepthSmooth, uv + vec2(0.0, -u_TexelSize.y)).r;

    vec3 pR = reconstructViewPos(uv + vec2( u_TexelSize.x, 0.0), dR);
    vec3 pL = reconstructViewPos(uv + vec2(-u_TexelSize.x, 0.0), dL);
    vec3 pU = reconstructViewPos(uv + vec2(0.0,  u_TexelSize.y), dU);
    vec3 pD = reconstructViewPos(uv + vec2(0.0, -u_TexelSize.y), dD);

    // 按 Z 分量差（深度差）选择切向量，深度差更小的一侧更贴近本粒子表面
    vec3 px = (abs(pR.z - p.z) < abs(pL.z - p.z)) ? (pR - p) : (p - pL);
    vec3 py = (abs(pU.z - p.z) < abs(pD.z - p.z)) ? (pU - p) : (p - pD);

    return normalize(cross(px, py));
}

void main() {
    float d = texture(u_FluidDepthSmooth, v_UV).r;

    if (d >= 0.9999) {
        out_Normal = vec4(0.0);
        return;
    }

    vec3 normal = computeNormal(v_UV);

    // 无效法线修复：3×3 邻域平均有效法线
    if (!isValidNormal(normal)) {
        vec3 sum = vec3(0.0);
        int  cnt = 0;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                vec2 nuv = v_UV + vec2(float(dx) * u_TexelSize.x,
                                       float(dy) * u_TexelSize.y);
                vec3 nn = computeNormal(nuv);
                if (isValidNormal(nn)) { sum += normalize(nn); cnt++; }
            }
        }
        normal = (cnt > 0) ? sum / float(cnt) : vec3(0.0, 0.0, 1.0);
    }

    normal = normalize(normal);
    if (normal.z < 0.0) normal = -normal;

    out_Normal = vec4(normal, 1.0);
}
