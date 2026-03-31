// SSAO Blur - 深度感知双边模糊（Depth-Aware Bilateral Blur）
//
//  简单均值模糊会跨越物体边界传播 AO 值，在墙角产生不正确的暗晕。
//  使用深度连续性权重：
//   weight = exp(-(|centerDepth - sampleDepth| / centerDepth) * sharpness)
//   归一化深度差 → sharpness 参数与场景尺度无关，远近物体行为一致
//   深度差小（同一表面）→ weight ≈ 1  → 正常参与模糊
//   深度差大（跨边缘）  → weight ≈ 0  → 几乎不参与，保留 AO 边界
//
// u_BlurSharpness 控制灵敏度：
//   小值 → 宽松，更平滑（可能少量跨边漏光）
//   大值 → 严格，更锐利（接近不模糊）

#type vertex
#version 430 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main() {
    v_TexCoord  = a_TexCoord;
    gl_Position = vec4(a_Position.xy, 0.0, 1.0);
}

#type fragment
#version 430 core

layout(location = 0) out vec4 o_AO;

in vec2 v_TexCoord;

uniform sampler2D u_AOInput;        // 待模糊的原始 AO（来自 ssao.glsl）
uniform sampler2D u_Depth;          // 深度纹理（与 SSAO pass 共用）
uniform float     u_BlurSharpness;  // 深度敏感度（默认 1.0）

// CameraUBO（用于深度线性化，与 ssao.glsl 相同）
layout(std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_View;
    mat4 u_Projection;
    vec4 u_CameraPos;
};

float LinearizeDepth(float rawDepth) {
    float ndc_z = rawDepth * 2.0 - 1.0;
    return -u_Projection[3][2] / (-ndc_z - u_Projection[2][2]);
}

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(u_AOInput, 0));

    // 中心像素深度（线性化，用于边界检测）
    float centerDepth = LinearizeDepth(texture(u_Depth, v_TexCoord).r);

    float aoSum     = 0.0;
    float weightSum = 0.0;

    // 4×4 kernel（以当前像素为中心，[-1.5, 1.5] texel 范围）
    // 共 16 个采样，比 box blur 多一倍，但增加了深度权重过滤
    for (int x = -1; x <= 2; ++x) {
        for (int y = -1; y <= 2; ++y) {
            vec2 offset    = vec2(float(x) - 0.5, float(y) - 0.5) * texelSize;
            vec2 sampleUV  = v_TexCoord + offset;

            float sampleDepth  = LinearizeDepth(texture(u_Depth, sampleUV).r);
            float sampleAO     = texture(u_AOInput, sampleUV).r;

            // 深度感知权重：归一化深度差使 sharpness 参数与场景尺度无关
            // 不归一化时：远处物体深度差(米)远大于近处，导致远处描边异常噪点
            float depthDiff = abs(centerDepth - sampleDepth) / max(centerDepth, 0.1);
            float weight    = exp(-depthDiff * u_BlurSharpness);

            aoSum     += sampleAO * weight;
            weightSum += weight;
        }
    }

    float ao = aoSum / max(weightSum, 1e-6);  // 避免除零
    o_AO = vec4(ao, ao, ao, 1.0);
}
