// Prefiltered Environment Map Compute Shader
// 使用 GGX NDF 重要性采样 + Mipmap Filtered Importance Sampling (MFIS) 预滤波环境贴图
// 参考: Hazel EnvironmentMipFilter.glsl + EnvironmentMapping.glslh + PBR.glslh
// 每个 mip 级别单独 dispatch 一次
#version 430 core

// 每次 dispatch 写入一个 mip 级别
layout(binding = 0, rgba16f) restrict writeonly uniform imageCube o_OutputMap;
layout(binding = 1) uniform samplerCube u_InputMap;

uniform float u_Roughness;  // 当前 mip 对应的粗糙度 [0, 1]

// ----------- Common -----------
const float PI     = 3.141592;
const float TwoPI  = 2.0 * PI;
const float Epsilon = 0.00001;

// ----------- EnvironmentMapping -----------

vec3 GetCubeMapTexCoord(vec2 imageSize) {
    vec2 st = gl_GlobalInvocationID.xy / imageSize;
    vec2 uv = 2.0 * vec2(st.x, 1.0 - st.y) - vec2(1.0);

    vec3 ret;
    if      (gl_GlobalInvocationID.z == 0u) ret = vec3( 1.0, uv.y, -uv.x);
    else if (gl_GlobalInvocationID.z == 1u) ret = vec3(-1.0, uv.y,  uv.x);
    else if (gl_GlobalInvocationID.z == 2u) ret = vec3( uv.x, 1.0, -uv.y);
    else if (gl_GlobalInvocationID.z == 3u) ret = vec3( uv.x,-1.0,  uv.y);
    else if (gl_GlobalInvocationID.z == 4u) ret = vec3( uv.x, uv.y,  1.0);
    else                                    ret = vec3(-uv.x, uv.y, -1.0);
    return normalize(ret);
}

void ComputeBasisVectors(const vec3 N, out vec3 S, out vec3 T) {
    T = cross(N, vec3(0.0, 1.0, 0.0));
    T = mix(cross(N, vec3(1.0, 0.0, 0.0)), T, step(Epsilon, dot(T, T)));
    T = normalize(T);
    S = normalize(cross(N, T));
}

vec3 TangentToWorld(const vec3 v, const vec3 N, const vec3 S, const vec3 T) {
    return S * v.x + T * v.y + N * v.z;
}

float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 SampleHammersley(uint i, uint N) {
    float invN = 1.0 / float(N);
    return vec2(i * invN, RadicalInverse_VdC(i));
}

// GGX 重要性采样半向量 (来自 Hazel SampleGGX)
vec3 SampleGGX(float u1, float u2, float roughness) {
    float alpha = roughness * roughness;
    float cosTheta = sqrt((1.0 - u2) / (1.0 + (alpha * alpha - 1.0) * u2));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    float phi = TwoPI * u1;
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// GGX 法线分布函数 (来自 Hazel NdfGGX)
float NdfGGX(float cosLh, float roughness) {
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;
    float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
    return alphaSq / (PI * denom * denom);
}

// ----------- 主函数 -----------

const uint NumSamples = 1024u;

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
void main() {
    ivec2 outputSize = imageSize(o_OutputMap);
    if (gl_GlobalInvocationID.x >= uint(outputSize.x) ||
        gl_GlobalInvocationID.y >= uint(outputSize.y)) return;

    // 假设 V = N (各向同性反射近似, 来自 Hazel)
    vec3 N = GetCubeMapTexCoord(vec2(outputSize));
    vec3 Lo = N;

    // roughness=0 时 NdfGGX 分母为 0 → NaN, 直接从 mip0 采样
    if (u_Roughness < 0.001) {
        imageStore(o_OutputMap, ivec3(gl_GlobalInvocationID),
                   vec4(min(textureLod(u_InputMap, N, 0).rgb, vec3(100.0)), 1.0));
        return;
    }

    vec3 S, T;
    ComputeBasisVectors(N, S, T);

    // 输入贴图的单个 texel 对应的立体角 (用于 MFIS mip 级别选择)
    vec2 inputSize = vec2(textureSize(u_InputMap, 0));
    float wt = 4.0 * PI / (6.0 * inputSize.x * inputSize.y);

    vec3 color  = vec3(0.0);
    float weight = 0.0;

    // 用 GGX NDF 重要性采样卷积环境贴图
    for (uint i = 0u; i < NumSamples; i++) {
        vec2 u  = SampleHammersley(i, NumSamples);
        // Lh: 切线空间 GGX 半向量 → 世界空间
        vec3 Lh = TangentToWorld(SampleGGX(u.x, u.y, u_Roughness), N, S, T);

        // Li: 通过 Lo 关于 Lh 的反射得到入射方向
        vec3 Li = 2.0 * dot(Lo, Lh) * Lh - Lo;

        float cosLi = dot(N, Li);
        if (cosLi > 0.0) {
            float cosLh = max(dot(N, Lh), 0.0);

            // GGX pdf = D(Lh) * cosLh / (4 * dot(Lo, Lh))
            // 因为 dot(Lo, Lh) 近似为 cosLh (V=N 时), 所以 pdf ≈ D * 0.25
            float pdf = NdfGGX(cosLh, u_Roughness) * 0.25;

            // 此采样对应的立体角
            float ws = 1.0 / (float(NumSamples) * pdf + 0.0001);

            // MFIS: 根据立体角比选择合适的 mip 级别, 减少方差
            float mipLevel = max(0.5 * log2(ws / wt) + 1.0, 0.0);

            vec3 sampleColor = textureLod(u_InputMap, Li, mipLevel).rgb;
            sampleColor = min(sampleColor, vec3(100.0));

            color  += sampleColor * cosLi;
            weight += cosLi;
        }
    }
    if (weight > 0.0)
        color /= weight;

    imageStore(o_OutputMap, ivec3(gl_GlobalInvocationID), vec4(color, 1.0));
}
