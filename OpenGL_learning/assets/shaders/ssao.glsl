// SSAO - Screen Space Ambient Occlusion
//
// 优化点说明：
//
// [1] 深度线性化 & 位置重建
//     标准做法：u_InvProjection * clipPos / w（矩阵乘 + 透视除法）
//     优化做法：只读投影矩阵的 4 个标量，2 次乘除即可（同 XeGTAO NDCToViewMul 思路）
//
// [2] 交错梯度噪声（Interleaved Gradient Noise, IGN）
//     标准做法：绑定 4×4 noise texture，额外占用纹理槽
//     优化做法：用数学公式在 shader 内直接生成，省去一次纹理采样
//     来源：Jimenez 2014 "Next Generation Post Processing in Call of Duty"
//
// [3] 样本分布幂次（Distribution Power）
//     C++ 生成核时使 r = mix(0.1, 1.0, (i/N)²)，更多样本集中在近处
//     依据：近处几何体对 AO 贡献最大，远处贡献递减
//
// [4] 平滑衰减（Smooth Falloff）
//     用 smoothstep 替代硬截断，消除半径边界的锯齿状圆圈

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

uniform sampler2D u_GNormal;   // Forward: 视图空间法线 [0,1]（来自 ssao_prepass）
                               // Deferred: Oct 编码世界法线 RG（来自 GBuffer att1）
uniform sampler2D u_GDepth;    // 深度纹理（GL_DEPTH24_STENCIL8）
uniform bool      u_UseGBufferNormals;  // true = Deferred 路径，从 GBuffer 解码法线

uniform vec3 u_Samples[64];   // 半球采样核（切线空间，C++ 生成）
uniform int  u_KernelSize;    // 实际使用数量（≤ 64）
uniform float u_Radius;       // 采样半径（视图空间，单位：米）
uniform float u_Bias;         // 防止 self-occlusion 的深度偏移

layout(std140) uniform CameraData {
    mat4 u_ViewProjection;
    mat4 u_View;
    mat4 u_Projection;
    vec4 u_CameraPos;
};

// 优化 [1A]：深度线性化
// GLM column-major 透视矩阵（glm::perspective）中：
//   P[2][2] = -(f+n)/(f-n)，P[3][2] = -2fn/(f-n)
// 推导：NDC_z = (P[2][2]*viewZ + P[3][2]) / (-viewZ)
//       viewZ = P[3][2] / (-NDC_z - P[2][2])
//       linearZ = -viewZ（正值，表示距相机的距离）
float LinearizeDepth(float rawDepth) {
    float ndc_z = rawDepth * 2.0 - 1.0;
    return -u_Projection[3][2] / (-ndc_z - u_Projection[2][2]);
}

// 优化 [1B]：视图空间位置重建
// 等价于 InvProjection * clipPos / w，但避免了矩阵乘法：
//   viewX = NDC_x * (1/P[0][0]) * linearDepth  (= aspect * tan(fov/2) * depth)
//   viewY = NDC_y * (1/P[1][1]) * linearDepth  (= tan(fov/2) * depth)
//   viewZ = -linearDepth  （OpenGL 相机朝 -Z）
// 编译器会将 1/P[i][i] 提到循环外，等价于 Hazel 的 NDCToViewMul 预计算。
vec3 ReconstructViewPos(vec2 uv, float linearDepth) {
    vec2 ndc = uv * 2.0 - 1.0;
    return vec3(
        ndc * vec2(1.0 / u_Projection[0][0], 1.0 / u_Projection[1][1]) * linearDepth,
        -linearDepth
    );
}

// Octahedral 解码（对应 GBuffer 写入的 OctEncode）
vec3 OctDecode(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0);
    n.x += (n.x >= 0.0) ? -t : t;
    n.y += (n.y >= 0.0) ? -t : t;
    return normalize(n);
}

// 优化 [2]：交错梯度噪声（IGN）
// 对不同像素坐标产生不同的随机角度，用于旋转采样核。
// 空间分布质量优于 mod-4 重复的噪声纹理（无 4px 重复条纹）。
float InterleavedGradientNoise(vec2 fragCoord) {
    return fract(52.9829189 * fract(dot(fragCoord, vec2(0.06711056, 0.00583715))));
}

void main() {
    // 解码法线，统一变换到视图空间
    vec3 normal;
    if (u_UseGBufferNormals) {
        // Deferred 路径：GBuffer att1 RG 存储 Oct 编码的世界空间法线
        vec2 oct = texture(u_GNormal, v_TexCoord).rg;
        vec3 worldNormal = OctDecode(oct);
        normal = normalize(vec3(u_View * vec4(worldNormal, 0.0)));
    } else {
        // Forward 路径：ssao_prepass 输出视图空间法线 [0,1]
        normal = normalize(texture(u_GNormal, v_TexCoord).xyz * 2.0 - 1.0);
    }

    float rawDepth    = texture(u_GDepth, v_TexCoord).r;
    float linearDepth = LinearizeDepth(rawDepth);
    vec3  fragPos     = ReconstructViewPos(v_TexCoord, linearDepth);

    // 天空/背景像素（深度接近远平面）无需计算，直接输出无遮蔽
    if (rawDepth >= 0.9999) {
        o_AO = vec4(1.0);
        return;
    }

    // 优化 [2]：用 IGN 构造随机旋转向量，替代 noise texture 绑定
    // 省去一次纹理槽和采样，同时 IGN 的空间分布质量更好
    float noise  = InterleavedGradientNoise(gl_FragCoord.xy);
    float angle  = noise * 6.28318530718;
    vec3 randVec = vec3(cos(angle), sin(angle), 0.0);

    // Gram-Schmidt 正交化：构建以法线为 Z 轴的切线空间
    // 每个像素旋转角不同，使相邻像素采样核不重叠，打破规律性条纹
    vec3 tangent   = normalize(randVec - normal * dot(randVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;

    for (int i = 0; i < u_KernelSize; ++i) {
        // 将核样本从切线空间变换到视图空间
        // u_Samples[i] 由 C++ 生成时已应用幂次分布（优化 [3]），
        // 更多样本集中在近处，无需 shader 内额外处理
        vec3 samplePos = fragPos + TBN * u_Samples[i] * u_Radius;

        // 将采样点投影到屏幕空间，获取对应 UV
        vec4 offset = u_Projection * vec4(samplePos, 1.0);
        offset.xy  /= offset.w;
        vec2 sampleUV = offset.xy * 0.5 + 0.5;

        if (any(lessThan(sampleUV, vec2(0.0))) || any(greaterThan(sampleUV, vec2(1.0))))
            continue;

        float occluderLinear = LinearizeDepth(texture(u_GDepth, sampleUV).r);
        vec3  occluderPos    = ReconstructViewPos(sampleUV, occluderLinear);

        // 优化 [4A]：深度方向范围检查
        // 当 fragPos 与 occluder 的 Z 差超过 radius（如穿透孔洞看到远处），
        // 权重趋于 0，避免不应存在的遮蔽
        float rangeCheck = smoothstep(0.0, 1.0,
            u_Radius / max(abs(fragPos.z - occluderPos.z), 1e-4));

        // 优化 [4B]：半径边界平滑衰减，消除硬截断产生的圆形锯齿边缘
        float dist    = length(samplePos - fragPos);
        float falloff = 1.0 - smoothstep(u_Radius * 0.75, u_Radius, dist);

        // 遮挡判断：view space Z 为负，occluder.z > sample.z 表示 occluder 更靠近相机
        // 即 sample 在 occluder 背后 → 被遮挡
        float occluded = (occluderPos.z >= samplePos.z + u_Bias) ? 1.0 : 0.0;
        occlusion += occluded * rangeCheck * falloff;
    }

    float ao = 1.0 - (occlusion / float(u_KernelSize));
    o_AO = vec4(ao, ao, ao, 1.0);
}
