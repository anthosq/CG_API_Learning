// fluid_blur.glsl — Phase 12-E：完整 NRF 深度平滑 + 厚度扩散
//
// direction=0/1  1D NRF 深度平滑（水平/垂直）
// direction=2    M×M 2D 清理 pass（消除分离式近似产生的十字形伪影）
// direction=3/4  厚度图纯空间高斯扩散（水平/垂直）
//
// 所有 pass 的输入纹理均绑定到 slot 0（u_InputTex），由 CPU 侧切换绑定目标。

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

layout(location = 0) out float out_Value;

in vec2 v_UV;

uniform sampler2D u_InputTex;      // 深度（pass 0~2）或厚度（pass 3~4），均绑定 slot 0
uniform vec2      u_TexelSize;
uniform int       u_Direction;     // 0=H深度, 1=V深度, 2=2D清理, 3=H厚度, 4=V厚度
uniform int       u_KernelRadius;
uniform float     u_SigmaS;        // 空间高斯 σ（像素）
uniform float     u_Threshold;     // NRF 深度阈值（pass 0~2 使用；pass 3~4 忽略）

float gaussian(float x, float sigma) {
    return exp(-0.5 * (x * x) / (sigma * sigma));
}

// 完整 1D NRF（水平或垂直深度平滑）
//
// 区分中心像素所属层次（前层 vs 后层），分别用对应层样本平滑：
//   前表面 / 间隙：只取 d ≤ dMin + threshold（前层）
//   后表面       ：只取 d >  dMin + threshold（后层，独立平滑）
// 间隙像素（depth≥0.9999）按前表面处理，由前层流体深度填充。
void nrfDepth1D(vec2 dir) {
    float centerDepth = texture(u_InputTex, v_UV).r;
    bool  isGap       = (centerDepth >= 0.9999);

    float dMin = 1.0;
    for (int i = -u_KernelRadius; i <= u_KernelRadius; i++) {
        float d = texture(u_InputTex, v_UV + dir * float(i) * u_TexelSize).r;
        if (d < 0.9999) dMin = min(dMin, d);
    }
    // 真背景（核内无流体）→ 透传
    if (dMin >= 0.9999) { out_Value = centerDepth; return; }

    float frontBias     = isGap ? 0.0 : (centerDepth - dMin);
    bool  isBackSurface = (!isGap) && (frontBias > u_Threshold);
    float narrowEnd     = dMin + u_Threshold;

    float sum = 0.0, wSum = 0.0;
    for (int i = -u_KernelRadius; i <= u_KernelRadius; i++) {
        float d = texture(u_InputTex, v_UV + dir * float(i) * u_TexelSize).r;
        if (d >= 0.9999) continue;
        if (isBackSurface) { if (d <= narrowEnd) continue; }
        else               { if (d >  narrowEnd) continue; }
        float w = gaussian(float(i), u_SigmaS);
        sum  += d * w;
        wSum += w;
    }
    out_Value = (wSum > 1e-6) ? (sum / wSum) : centerDepth;
}

// 2D 清理（小核双边，修复分离式 1D NRF 的十字形伪影）
void nrfDepth2DCleanup() {
    float centerDepth = texture(u_InputTex, v_UV).r;
    if (centerDepth >= 0.9999) { out_Value = centerDepth; return; }

    float sum = 0.0, wSum = 0.0;
    for (int y = -u_KernelRadius; y <= u_KernelRadius; y++) {
        for (int x = -u_KernelRadius; x <= u_KernelRadius; x++) {
            float d = texture(u_InputTex, v_UV + vec2(float(x), float(y)) * u_TexelSize).r;
            if (d >= 0.9999)                    continue;
            if (abs(d - centerDepth) > u_Threshold) continue;
            float wS = gaussian(length(vec2(float(x), float(y))), u_SigmaS);
            sum  += d * wS;
            wSum += wS;
        }
    }
    out_Value = (wSum > 1e-6) ? (sum / wSum) : centerDepth;
}

// 厚度图纯空间高斯扩散（水平或垂直）
//
// 问题根因：NRF 将间隙像素的深度填充为流体深度，但厚度图中这些像素仍为 0。
// ShadePass 的 Beer-Lambert 用 thickness=0 计算 → 完全透明 → 流体边缘透明光晕。
//
// 修复：对厚度图做纯空间高斯，使厚度向粒子间隙和边界延伸。
//   - 零厚度样本跳过（不往外扩散 0 值），避免稀释有效厚度
//   - 中心厚度=0 且有邻居贡献 → 填充为邻居加权均值（消除透明光晕）
//   - 中心厚度=0 且无邻居（真背景）→ 输出 0（ShadePass 不渲染该像素）
void thicknessBlur1D(vec2 dir) {
    float sum = 0.0, wSum = 0.0;
    for (int i = -u_KernelRadius; i <= u_KernelRadius; i++) {
        float t = texture(u_InputTex, v_UV + dir * float(i) * u_TexelSize).r;
        if (t <= 0.0) continue;  // 零厚度样本跳过（间隙/背景不贡献）
        float w = gaussian(float(i), u_SigmaS);
        sum  += t * w;
        wSum += w;
    }
    out_Value = (wSum > 1e-6) ? (sum / wSum) : 0.0;
}

void main() {
    if      (u_Direction == 0) nrfDepth1D(vec2(1.0, 0.0));
    else if (u_Direction == 1) nrfDepth1D(vec2(0.0, 1.0));
    else if (u_Direction == 2) nrfDepth2DCleanup();
    else if (u_Direction == 3) thicknessBlur1D(vec2(1.0, 0.0));
    else                       thicknessBlur1D(vec2(0.0, 1.0));
}
