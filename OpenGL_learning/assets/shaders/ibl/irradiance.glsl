// Irradiance Map Compute Shader
// 通过半球面蒙特卡罗积分计算漫反射辐照度图
#version 430 core

layout(binding = 0, rgba16f) restrict writeonly uniform imageCube o_IrradianceMap;
layout(binding = 1) uniform samplerCube u_RadianceMap;

uniform int u_Samples;  // 采样数 (建议 64~512)

const float PI     = 3.141592;
const float TwoPI  = 2.0 * PI;
const float Epsilon = 0.00001;

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

// 计算 TBN 基向量 (无分支版本)
void ComputeBasisVectors(const vec3 N, out vec3 S, out vec3 T) {
    T = cross(N, vec3(0.0, 1.0, 0.0));
    T = mix(cross(N, vec3(1.0, 0.0, 0.0)), T, step(Epsilon, dot(T, T)));
    T = normalize(T);
    S = normalize(cross(N, T));
}

// 切线空间 → 世界空间
vec3 TangentToWorld(const vec3 v, const vec3 N, const vec3 S, const vec3 T) {
    return S * v.x + T * v.y + N * v.z;
}

// Hammersley 低差异序列
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 SampleHammersley(uint i, uint samples) {
    float invSamples = 1.0 / float(samples);
    return vec2(i * invSamples, RadicalInverse_VdC(i));
}

// 均匀半球采样
// u1 → z 轴分量, u2 → 方位角
vec3 SampleHemisphere(float u1, float u2) {
    const float u1p = sqrt(max(0.0, 1.0 - u1 * u1));
    return vec3(cos(TwoPI * u2) * u1p, sin(TwoPI * u2) * u1p, u1);
}

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
void main() {
    ivec2 outputSize = imageSize(o_IrradianceMap);
    if (gl_GlobalInvocationID.x >= uint(outputSize.x) ||
        gl_GlobalInvocationID.y >= uint(outputSize.y)) return;

    vec3 N = GetCubeMapTexCoord(vec2(outputSize));

    vec3 S, T;
    ComputeBasisVectors(N, S, T);

    uint samples = uint(u_Samples);
    vec3 irradiance = vec3(0.0);

    for (uint i = 0u; i < samples; i++) {
        vec2 u  = SampleHammersley(i, samples);
        vec3 Li = TangentToWorld(SampleHemisphere(u.x, u.y), N, S, T);
        float cosTheta = max(0.0, dot(Li, N));

        // 2.0 来自 pdf 约分: pdf = 1/(2PI), dΩ·cosθ 积分后除以 PI 抵消
        // 使用 mip 1 并截断极亮值，防止 HDR 太阳等强光源产生萤火虫白斑
        vec3 sampleColor = min(textureLod(u_RadianceMap, Li, 1).rgb, vec3(10.0));
        irradiance += 2.0 * sampleColor * cosTheta;
    }
    irradiance /= float(samples);

    imageStore(o_IrradianceMap, ivec3(gl_GlobalInvocationID), vec4(irradiance, 1.0));
}
