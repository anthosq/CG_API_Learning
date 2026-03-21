#include "IBLProcessor.h"
#include <glm/glm.hpp>
#include <iostream>

namespace GLRenderer {

// 静态成员定义
Ref<ComputePipeline> IBLProcessor::s_EquirectPipeline;
Ref<ComputePipeline> IBLProcessor::s_IrradiancePipeline;
Ref<ComputePipeline> IBLProcessor::s_PrefilterPipeline;
Ref<Texture2D>       IBLProcessor::s_BRDFLut;

IBLProcessor::IBLProcessor(const Config& config)
    : m_Config(config) {}

Ref<IBLProcessor> IBLProcessor::Create(const Config& config) {
    return Ref<IBLProcessor>(new IBLProcessor(config));
}

void IBLProcessor::EnsurePipelinesLoaded() {
    if (!s_EquirectPipeline)
        s_EquirectPipeline  = ComputePipeline::Create("assets/shaders/ibl/equirect_to_cubemap.glsl");
    if (!s_IrradiancePipeline)
        s_IrradiancePipeline = ComputePipeline::Create("assets/shaders/ibl/irradiance.glsl");
    if (!s_PrefilterPipeline)
        s_PrefilterPipeline  = ComputePipeline::Create("assets/shaders/ibl/prefilter.glsl");
}

Ref<Texture2D> IBLProcessor::GetBRDFLUT() {
    if (!s_BRDFLut) {
        TextureSpec spec;
        spec.FlipVertically  = false;
        spec.GenerateMipmaps = false;
        spec.WrapS = GL_CLAMP_TO_EDGE;
        spec.WrapT = GL_CLAMP_TO_EDGE;
        spec.MinFilter = GL_LINEAR;   // 无 mip chain，必须用非 mipmap 过滤器，否则纹理不完整返回黑
        spec.MagFilter = GL_LINEAR;
        s_BRDFLut = Texture2D::Create("assets/render/BRDF_LUT.png", spec);
        std::cout << "[IBLProcessor] BRDF LUT 加载完成" << std::endl;
    }
    return s_BRDFLut;
}

bool IBLProcessor::Load(const std::filesystem::path& hdrPath) {
    m_Ready = false;

    EnsurePipelinesLoaded();
    if (!s_EquirectPipeline || !s_IrradiancePipeline || !s_PrefilterPipeline) {
        std::cerr << "[IBLProcessor] Compute pipeline 加载失败" << std::endl;
        return false;
    }

    // 加载 HDR 等距矩形图 (Hazel 使用 RGBA32F, 我们用 RGB16F 节省显存)
    TextureSpec hdrSpec;
    hdrSpec.FlipVertically  = false;  // y-flip已在GetCubeMapTexCoord shader中处理 (1.0 - st.y)
    hdrSpec.GenerateMipmaps = false;
    hdrSpec.WrapS = GL_CLAMP_TO_EDGE;
    hdrSpec.WrapT = GL_CLAMP_TO_EDGE;
    hdrSpec.MinFilter = GL_LINEAR;
    hdrSpec.MagFilter = GL_LINEAR;

    auto hdrTexture = Texture2D::Create(hdrPath, hdrSpec);
    if (!hdrTexture || !hdrTexture->IsValid() || !hdrTexture->IsHDR()) {
        std::cerr << "[IBLProcessor] 加载 HDR 失败或不是 HDR 格式: " << hdrPath << std::endl;
        return false;
    }
    std::cout << "[IBLProcessor] 开始处理: " << hdrPath.filename().string() << std::endl;

    // 阶段 1: equirect → envUnfiltered + 生成 mip
    if (!ProcessEquirectToCubemap(hdrTexture)) return false;

    // 阶段 2: envUnfiltered → RadianceMap (按粗糙度预滤波各 mip)
    ProcessPrefilter();

    // 阶段 3: RadianceMap → IrradianceMap (漫反射卷积)
    ProcessIrradiance();

    m_Ready = true;
    std::cout << "[IBLProcessor] IBL 处理完成: "
              << hdrPath.filename().string() << std::endl;
    return true;
}

bool IBLProcessor::ProcessEquirectToCubemap(const Ref<Texture2D>& hdrTexture) {
    const uint32_t res = m_Config.EnvMapResolution;

    // 计算完整 mip 链所需级别数
    uint32_t mipLevels = 1;
    uint32_t r = res;
    while (r > 1) { r >>= 1; mipLevels++; }

    // 创建 unfiltered cubemap (glTexStorage2D 分配不可变存储)
    m_EnvUnfiltered = TextureCube::CreateEmpty(res, GL_RGBA16F, mipLevels);
    if (!m_EnvUnfiltered || !m_EnvUnfiltered->IsValid()) {
        std::cerr << "[IBLProcessor] 创建 EnvUnfiltered 失败" << std::endl;
        return false;
    }

    glTextureParameteri(m_EnvUnfiltered->GetID(), GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(m_EnvUnfiltered->GetID(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);


    // 绑定 equirect 纹理到 sampler binding = 1
    glActiveTexture(GL_TEXTURE1);
    hdrTexture->Bind(1);

    // 绑定 mip 0 的 cubemap 作为输出 (image binding = 0, layered)
    s_EquirectPipeline->Bind();
    s_EquirectPipeline->BindImageTextureCube(0, m_EnvUnfiltered->GetID(), 0,
                                              GL_WRITE_ONLY, GL_RGBA16F);
    s_EquirectPipeline->SetInt("u_EquirectangularMap", 1);

    // dispatch: 每个工作组处理 32x32 像素, z = 6 个面
    uint32_t groups = (res + 31) / 32;
    s_EquirectPipeline->DispatchAndWait(groups, groups, 6);
    s_EquirectPipeline->Unbind();

    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // 生成 mip 链 (prefilter 阶段需要 mip 做 MFIS)
    m_EnvUnfiltered->GenerateMipmaps();

    // GenerateMipmaps 写入 mip 1+, 需要此 barrier 确保 prefilter compute 采样时可见
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    std::cout << "[IBLProcessor] EquirectToCubemap 完成 (" << res << "x" << res << ")" << std::endl;
    return true;
}

void IBLProcessor::ProcessPrefilter() {
    const uint32_t res = m_Config.PrefilterResolution;

    // mip 数 = log2(res) + 1
    uint32_t mipLevels = 1;
    uint32_t r = res;
    while (r > 1) { r >>= 1; mipLevels++; }

    // RadianceMap: prefilter 结果
    m_RadianceMap = TextureCube::CreateEmpty(res, GL_RGBA16F, mipLevels);
    glTextureParameteri(m_RadianceMap->GetID(), GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(m_RadianceMap->GetID(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 绑定 unfiltered 作为输入纹理 (sampler binding = 1)
    m_EnvUnfiltered->Bind(1);

    s_PrefilterPipeline->Bind();
    s_PrefilterPipeline->SetInt("u_InputMap", 1);

    // 每个 mip 对应一个粗糙度, 分别 dispatch (参考 Hazel 逐 mip 循环)
    const float deltaRoughness = 1.0f / glm::max(float(mipLevels) - 1.0f, 1.0f);

    for (uint32_t mip = 0; mip < mipLevels; mip++) {
        uint32_t mipRes = glm::max(1u, res >> mip);
        uint32_t groups = glm::max(1u, (mipRes + 31) / 32);

        float roughness = mip * deltaRoughness;
        s_PrefilterPipeline->SetFloat("u_Roughness", roughness);

        // 绑定当前 mip 作为写入目标
        s_PrefilterPipeline->BindImageTextureCube(0, m_RadianceMap->GetID(), mip,
                                                   GL_WRITE_ONLY, GL_RGBA16F);

        s_PrefilterPipeline->Dispatch(groups, groups, 6);
        ComputePipeline::Wait(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    s_PrefilterPipeline->Unbind();

    // 确保 prefilter 写入对后续纹理采样 (irradiance 用 sampler 读 RadianceMap) 可见
    ComputePipeline::Wait(GL_TEXTURE_FETCH_BARRIER_BIT);

    std::cout << "[IBLProcessor] PrefilterEnvMap 完成 ("
              << res << "x" << res << ", " << mipLevels << " mips)" << std::endl;
}

void IBLProcessor::ProcessIrradiance() {
    const uint32_t res = m_Config.IrradianceResolution;

    // IrradianceMap: 低分辨率即可 (32x32), 无需 mip
    m_IrradianceMap = TextureCube::CreateEmpty(res, GL_RGBA16F, 1);

    // 输入: RadianceMap (预滤波后的, 对应 Hazel 传 envFiltered)
    m_EnvUnfiltered->Bind(1);

    s_IrradiancePipeline->Bind();
    s_IrradiancePipeline->SetInt("u_RadianceMap", 1);
    s_IrradiancePipeline->SetInt("u_Samples", m_Config.IrradianceSamples);

    s_IrradiancePipeline->BindImageTextureCube(0, m_IrradianceMap->GetID(), 0,
                                                GL_WRITE_ONLY, GL_RGBA16F);

    uint32_t groups = glm::max(1u, (res + 31) / 32);
    s_IrradiancePipeline->DispatchAndWait(groups, groups, 6);
    s_IrradiancePipeline->Unbind();

    std::cout << "[IBLProcessor] IrradianceMap 完成 ("
              << res << "x" << res << ", samples=" << m_Config.IrradianceSamples << ")" << std::endl;
}

} // namespace GLRenderer
