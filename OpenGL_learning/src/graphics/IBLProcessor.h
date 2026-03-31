#pragma once

#include "ComputePipeline.h"
#include "Texture.h"
#include "core/Ref.h"
#include <filesystem>

namespace GLRenderer {

// IBLProcessor - Image-Based Lighting 预处理器
//
// 将 HDR 等距矩形环境图 (.hdr) 转换为 IBL 所需贴图:
//
//   HDR (equirect)
//       ↓  [EquirectToCubemap compute]
//   EnvUnfiltered (带 mip)          ← 中间产物, 不对外暴露
//       ↓  [PrefilterEnvMap compute, 按 roughness 逐 mip]
//   EnvFiltered (RadianceMap)        ← specular IBL
//       ↓  [EnvironmentIrradiance compute, 采样 EnvFiltered]
//   IrradianceMap (32x32)            ← diffuse IBL
//
// BRDF LUT 使用预烘焙文件 assets/render/BRDF_LUT.png, 全局共享
//
// 在 PBR shader 中绑定:
//   ibl->GetIrradianceMap()->Bind(5);    // u_EnvIrradianceTex
//   ibl->GetRadianceMap()->Bind(6);      // u_EnvRadianceTex
//   IBLProcessor::GetBRDFLUT()->Bind(7); // u_BRDFLUTTexture

class IBLProcessor : public RefCounter {
public:
    struct Config {
        uint32_t EnvMapResolution     = 1024;
        uint32_t IrradianceResolution = 32;
        uint32_t PrefilterResolution  = 512;
        int      IrradianceSamples    = 512;
    };

    static Ref<IBLProcessor> Create(const Config& config = Config());

    // 加载 HDR 环境图并执行所有预处理步骤
    // 返回 false 表示加载失败
    bool Load(const std::filesystem::path& hdrPath);

    // 对外接口
    Ref<TextureCube> GetEnvMap()        const { return m_EnvUnfiltered; }  // 天空盒用 (1024x1024, 清晰)
    Ref<TextureCube> GetRadianceMap()   const { return m_RadianceMap; }    // specular IBL (256x256, 预过滤)
    Ref<TextureCube> GetIrradianceMap() const { return m_IrradianceMap; }  // diffuse IBL (32x32)

    bool IsReady() const { return m_Ready; }

    // 全局共享 BRDF LUT (从 assets/render/BRDF_LUT.png 懒加载, 与场景无关)
    static Ref<Texture2D> GetBRDFLUT();

private:
    explicit IBLProcessor(const Config& config);

    // 处理步骤
    bool ProcessEquirectToCubemap(const Ref<Texture2D>& hdrTexture);  // → m_EnvUnfiltered
    void ProcessPrefilter();                                            // → m_RadianceMap
    void ProcessIrradiance();                                           // → m_IrradianceMap

    Config           m_Config;
    bool             m_Ready = false;

    Ref<TextureCube> m_EnvUnfiltered;  // 中间产物: equirect 转换结果 + mip
    Ref<TextureCube> m_RadianceMap;    // 最终输出: 预滤波 specular IBL
    Ref<TextureCube> m_IrradianceMap;  // 最终输出: 辐照度 diffuse IBL

    // 共享 compute pipelines (懒加载, 所有 IBLProcessor 实例共用)
    static Ref<ComputePipeline> s_EquirectPipeline;
    static Ref<ComputePipeline> s_IrradiancePipeline;
    static Ref<ComputePipeline> s_PrefilterPipeline;
    static Ref<Texture2D>       s_BRDFLut;

    static void EnsurePipelinesLoaded();
};

} // namespace GLRenderer
