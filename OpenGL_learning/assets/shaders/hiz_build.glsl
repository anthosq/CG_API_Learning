// Hi-Z (Hierarchical-Z) 深度 Mip 链构建
// 每次 Dispatch 写入 HiZ 的一个 mip 级别。
//
// 第一轮（u_IsFirstPass=1）：从场景深度纹理拷贝到 HiZ mip 0（1:1 拷贝）。
// 后续轮：对 HiZ 上一级 mip 做 2×2 MIN 下采样（标准深度 [0,1]，0=近，1=远，
//         取 MIN = 最近遮挡体，用于 SSR 射线是否穿越 tile 的判断）。
//
// 工程注意：
// - C++ 端为每级 Mip 单独调用 BindImageTexture(unit, tex, mip, WRITE_ONLY, R32F)
// - 每轮 Dispatch 之后必须插入 GL_SHADER_IMAGE_ACCESS_BARRIER_BIT，
//   确保下一轮采样时看到已写入的数据

#type compute
#version 430 core

layout(local_size_x = 8, local_size_y = 8) in;

// 写入目标 Mip（由 C++ 通过 glBindImageTexture 指定 level）
layout(binding = 0, r32f) restrict writeonly uniform image2D o_HiZ;

// 输入深度：第一轮为 GBuffer 的 DEPTH24_STENCIL8 深度纹理，
// 后续轮为上一级 HiZ R32F 纹理（绑定到相同 uniform 名）
uniform sampler2D u_InputDepth;

uniform int   u_SrcMip;       // 从 u_InputDepth 的哪一级 mip 采样（第一轮=0）
uniform ivec2 u_SrcSize;      // 源 mip 的像素分辨率（用于边界 clamp）
uniform int   u_IsFirstPass;  // 1=直接拷贝（1:1），0=2×2 MIN 下采样

void main() {
    ivec2 dstCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dstSize  = imageSize(o_HiZ);

    if (dstCoord.x >= dstSize.x || dstCoord.y >= dstSize.y)
        return;

    float result;

    if (u_IsFirstPass != 0) {
        // 第一轮：1:1 拷贝场景深度到 HiZ mip 0
        // DEPTH24_STENCIL8 在 GL_DEPTH_STENCIL_TEXTURE_MODE=GL_DEPTH_COMPONENT（默认）
        // 时可通过 sampler2D + texelFetch 读取深度值（返回 .r 分量）
        ivec2 srcCoord = clamp(dstCoord, ivec2(0), u_SrcSize - 1);
        result = texelFetch(u_InputDepth, srcCoord, 0).r;
    } else {
        // 后续轮：2×2 MIN 下采样
        // 标准深度 [0,1]（0=近，1=远）取 MIN = tile 内最近遮挡体
        // SSR：ray.z < HiZ[tile] → 射线仍在最近表面之前 → 可跳过此 tile
        ivec2 srcBase = dstCoord * 2;
        float d0 = texelFetch(u_InputDepth, clamp(srcBase + ivec2(0,0), ivec2(0), u_SrcSize-1), u_SrcMip).r;
        float d1 = texelFetch(u_InputDepth, clamp(srcBase + ivec2(1,0), ivec2(0), u_SrcSize-1), u_SrcMip).r;
        float d2 = texelFetch(u_InputDepth, clamp(srcBase + ivec2(0,1), ivec2(0), u_SrcSize-1), u_SrcMip).r;
        float d3 = texelFetch(u_InputDepth, clamp(srcBase + ivec2(1,1), ivec2(0), u_SrcSize-1), u_SrcMip).r;
        result = min(min(d0, d1), min(d2, d3));
    }

    imageStore(o_HiZ, dstCoord, vec4(result));
}
