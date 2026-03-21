// BRDF Integration LUT Compute Shader
// 预计算 PBR Split-Sum 近似所需的 BRDF 积分查找表
// 输出: RG16F 纹理 (R = scale, G = bias)
// 参考: Hazel PBR.glslh + Epic Games "Real Shading in Unreal Engine 4"
#version 430 core

layout(binding = 0, rg16f) restrict writeonly uniform image2D o_BRDFLut;

const float PI     = 3.141592;
const float TwoPI  = 2.0 * PI;
const float Epsilon = 0.00001;

float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 SampleHammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// GGX 重要性采样半向量
vec3 ImportanceSampleGGX(vec2 Xi, float roughness, vec3 N) {
    float a = roughness * roughness;
    float phi = TwoPI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H;
    H.x = sinTheta * cos(phi);
    H.y = sinTheta * sin(phi);
    H.z = cosTheta;

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangentX = normalize(cross(up, N));
    vec3 tangentY = cross(N, tangentX);
    return tangentX * H.x + tangentY * H.y + N * H.z;
}

// Schlick-GGX 几何函数 (IBL 版本, k = a²/2)
// 注意: 直接光照用 k = (r+1)²/8, IBL 用 k = r²/2
float GeometrySchlickGGX_IBL(float NdotV, float roughness) {
    float a = roughness;
    float k = (a * a) * 0.5;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith_IBL(float NdotV, float NdotL, float roughness) {
    return GeometrySchlickGGX_IBL(NdotV, roughness) *
           GeometrySchlickGGX_IBL(NdotL, roughness);
}

// 积分 BRDF (Split-Sum 第二项)
// 输入: NdotV (观察角余弦), roughness
// 输出: vec2(scale, bias) → F_approx = F0 * scale + bias
vec2 IntegrateBRDF(float NdotV, float roughness) {
    vec3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);  // sin(theta)
    V.y = 0.0;
    V.z = NdotV;                        // cos(theta)

    float A = 0.0;
    float B = 0.0;
    vec3 N = vec3(0.0, 0.0, 1.0);

    const uint NumSamples = 1024u;
    for (uint i = 0u; i < NumSamples; i++) {
        vec2 Xi = SampleHammersley(i, NumSamples);
        vec3 H  = ImportanceSampleGGX(Xi, roughness, N);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0) {
            float G     = GeometrySmith_IBL(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV + Epsilon);
            float Fc    = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    return vec2(A, B) / float(NumSamples);
}

// ----------- 主函数 -----------

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
void main() {
    vec2 outputSize = vec2(imageSize(o_BRDFLut));
    if (gl_GlobalInvocationID.x >= uint(outputSize.x) ||
        gl_GlobalInvocationID.y >= uint(outputSize.y)) return;

    // uv.x = NdotV (0~1), uv.y = roughness (0~1)
    vec2 uv = (vec2(gl_GlobalInvocationID.xy) + 0.5) / outputSize;

    vec2 brdf = IntegrateBRDF(uv.x, uv.y);
    imageStore(o_BRDFLut, ivec2(gl_GlobalInvocationID.xy), vec4(brdf, 0.0, 1.0));
}
