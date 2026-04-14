// SSSS Separable Gaussian Blur（Compute Shader）
//
// 设计要点：
//   - local_size_x=256，每个 workgroup 处理一行（H pass）或一列（V pass）中的 256 像素
//   - LDS（shared memory）缓存 SSS color：s_Cache[256 + 2*HALF_KERNEL]
//     每像素仅从 VRAM 读取一次 SSSColor，相邻线程从片上内存复用 → 节省 ~60% 带宽
//   - 深度边界保护：线性深度相对阈值 exp(-|Δlinear| / (linearCenter * 0.05))
//     避免 NDC 非线性深度导致近处过激进/远处过宽松的问题
//   - ShadingModelID 检查：中心像素非 SSS 时直接输出 0；邻域非 SSS 时权重置 0
//   - sigma 来自 GBuffer Att2.a 解包的 SubsurfaceRadius，乘以 u_BlurScale 换算到屏幕像素
//
// Dispatch 规则（C++ 侧）：
//   H pass: glDispatchCompute(ceil(width/256), height, 1)
//   V pass: glDispatchCompute(ceil(height/256), width, 1)

#type compute
#version 430 core

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(rgba16f, binding = 0) uniform readonly  image2D u_SrcImage;
layout(rgba16f, binding = 1) uniform writeonly image2D u_DstImage;

uniform sampler2D u_GBufDepth;
uniform sampler2D u_GBufEmissiveShadingID;
uniform bool      u_Horizontal;  // true = H pass，false = V pass
uniform float     u_BlurScale  = 8.0;              // SubsurfaceRadius → 屏幕像素 sigma 的缩放系数
uniform vec3      u_ScatterRGB = vec3(1.0, 0.5, 0.3); // RGB 各通道 sigma 倍率（皮肤：红>绿>蓝）
uniform float     u_Near = 0.1;
uniform float     u_Far  = 100.0;

float LinearizeDepth(float ndcDepth) {
    return u_Near * u_Far / (u_Far - ndcDepth * (u_Far - u_Near));
}

#define HALF_KERNEL 32
#define CACHE_SIZE  (256 + 2 * HALF_KERNEL)  // 320 × 16B = 5.12KB shared memory（GPU 最低 48KB，安全）

shared vec4 s_Cache[CACHE_SIZE];

void main() {
    ivec2 imgSize  = imageSize(u_SrcImage);
    int   localID  = int(gl_LocalInvocationID.x);

    // primary  = 沿模糊方向的坐标（H pass: x，V pass: y）
    // secondary = 垂直方向固定坐标（H pass: y 行号，V pass: x 列号）
    int primary   = int(gl_WorkGroupID.x) * 256 + localID;
    int secondary = int(gl_WorkGroupID.y);
    int dimPrimary = u_Horizontal ? imgSize.x : imgSize.y;

    // 填充 LDS（含两侧 HALF_KERNEL 宽的 halo，边界 clamp）
    // 每个线程可能填充多个 cache slot（CACHE_SIZE > 256）
    for (int i = localID; i < CACHE_SIZE; i += 256) {
        int src = clamp(int(gl_WorkGroupID.x) * 256 - HALF_KERNEL + i, 0, dimPrimary - 1);
        ivec2 srcCoord = u_Horizontal ? ivec2(src, secondary) : ivec2(secondary, src);
        s_Cache[i] = imageLoad(u_SrcImage, srcCoord);
    }
    barrier();

    if (primary >= dimPrimary) return;

    ivec2 coord = u_Horizontal ? ivec2(primary, secondary) : ivec2(secondary, primary);
    vec2  uv    = (vec2(coord) + 0.5) / vec2(imgSize);

    // 读取中心像素的深度与 ShadingModelID
    float centerDepth = texture(u_GBufDepth, uv).r;
    int   packedA     = int(texture(u_GBufEmissiveShadingID, uv).a * 255.0 + 0.5);
    int   centerID    = packedA & 0x3;

    // 非 SSS 像素直接输出 0（在 Composite Pass 中此像素无 SSS 贡献）
    if (centerID != 1) {
        imageStore(u_DstImage, coord, vec4(0.0));
        return;
    }

    // SubsurfaceRadius → per-channel sigma（RGB 散射距离不同，红 > 绿 > 蓝）
    float radius     = float((packedA >> 2) & 63) / 63.0 * 5.0;
    vec3  sigmaRGB   = max(vec3(radius * u_BlurScale) * u_ScatterRGB, vec3(1.0));
    vec3  inv2s2RGB  = 1.0 / (2.0 * sigmaRGB * sigmaRGB);

    // 中心线性深度：每像素只算一次，循环内复用
    float linearCenter   = LinearizeDepth(centerDepth);
    float depthThreshold = linearCenter * 0.05;

    vec3  sum  = vec3(0.0);
    vec3  wSum = vec3(0.0);

    for (int k = -HALF_KERNEL; k <= HALF_KERNEL; k++) {
        // 从 LDS 读取 SSS color（无额外 VRAM 访问）
        vec3 sampleColor = s_Cache[localID + HALF_KERNEL + k].rgb;

        // 邻域像素坐标（用于深度 / ShadingModelID 纹理采样）
        int   sp     = clamp(primary + k, 0, dimPrimary - 1);
        ivec2 sCoord = u_Horizontal ? ivec2(sp, secondary) : ivec2(secondary, sp);
        vec2  sUV    = (vec2(sCoord) + 0.5) / vec2(imgSize);

        // 深度边界权重（线性深度相对阈值，避免 NDC 非线性导致近处过激进/远处过宽松）
        float sDepth       = texture(u_GBufDepth, sUV).r;
        float linearSample = LinearizeDepth(sDepth);
        float depthW       = exp(-abs(linearSample - linearCenter) / depthThreshold);

        // ShadingModelID 遮罩：邻域非 SSS 时权重归零，防止非 SSS 颜色渗入
        int   sPacked = int(texture(u_GBufEmissiveShadingID, sUV).a * 255.0 + 0.5);
        float sssW    = float((sPacked & 0x3) == 1);

        float base   = depthW * sssW;
        // per-channel Gaussian（RGB 各自 sigma 不同）
        vec3  gaussW = exp(-float(k * k) * inv2s2RGB);
        vec3  w      = gaussW * base;

        sum  += sampleColor * w;
        wSum += w;
    }

    imageStore(u_DstImage, coord, vec4(sum / max(wSum, vec3(1e-5)), 1.0));
}
