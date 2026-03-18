#pragma once

// IBLProcessor - Image-Based Lighting 处理器
//
// 用于生成 PBR 所需的 IBL 纹理:
// 1. Irradiance Map (漫反射 IBL)
// 2. Prefiltered Environment Map (镜面反射 IBL)
// 3. BRDF LUT (Split-Sum 近似)
//
// 参考: LearnOpenGL PBR IBL, Hazel Engine

#include "graphics/Shader.h"
#include "graphics/Texture.h"
#include "graphics/Framebuffer.h"
#include "graphics/MeshFactory.h"

#include <memory>
#include <filesystem>

namespace GLRenderer {

// 开发者批注: 不需要在运行时计算 BRDF LUT, 直接拿图查表, 请你参考Hazel的实现


// IBL 数据结构
struct IBLData {
    std::unique_ptr<TextureCube> EnvironmentMap;   // 原始环境贴图
    std::unique_ptr<TextureCube> IrradianceMap;    // 漫反射辐照度图
    std::unique_ptr<TextureCube> PrefilterMap;     // 预过滤环境图
    std::unique_ptr<Texture> BrdfLUT;              // BRDF 查找表

    bool IsValid() const {
        return IrradianceMap && PrefilterMap && BrdfLUT;
    }
};

class IBLProcessor {
public:
    IBLProcessor();
    ~IBLProcessor();

    // 禁用拷贝
    IBLProcessor(const IBLProcessor&) = delete;
    IBLProcessor& operator=(const IBLProcessor&) = delete;

    // 初始化/关闭
    void Initialize(const std::filesystem::path& shaderDir);
    void Shutdown();
    bool IsInitialized() const { return m_Initialized; }

    // 从 HDR 环境贴图生成 IBL 数据
    // @param hdrPath: HDR 环境贴图路径 (equirectangular format)
    // @param envMapSize: 环境 cubemap 分辨率 (默认 512)
    // @param irradianceSize: 辐照度图分辨率 (默认 32)
    // @param prefilterSize: 预过滤图分辨率 (默认 128)
    std::unique_ptr<IBLData> ProcessHDRI(
        const std::filesystem::path& hdrPath,
        int envMapSize = 512,
        int irradianceSize = 32,
        int prefilterSize = 128);

    // 从已有 Cubemap 生成 IBL 数据
    std::unique_ptr<IBLData> ProcessCubemap(
        TextureCube* envMap,
        int irradianceSize = 32,
        int prefilterSize = 128);

    // 获取共享的 BRDF LUT (可复用)
    Texture* GetBrdfLUT();

    // 分辨率设置
    void SetBrdfLUTSize(int size) { m_BrdfLUTSize = size; }
    int GetBrdfLUTSize() const { return m_BrdfLUTSize; }

private:
    // 加载 HDR 纹理并转换为 Cubemap
    std::unique_ptr<TextureCube> EquirectangularToCubemap(
        const std::filesystem::path& hdrPath, int size);

    // 生成辐照度图
    std::unique_ptr<TextureCube> GenerateIrradianceMap(
        TextureCube* envMap, int size);

    // 生成预过滤环境图
    std::unique_ptr<TextureCube> GeneratePrefilterMap(
        TextureCube* envMap, int size);

    // 生成 BRDF LUT
    std::unique_ptr<Texture> GenerateBrdfLUT(int size);

    // 渲染 cubemap 的一个面
    void RenderCubemapFace(GLuint fbo, GLuint cubemap, int face,
                           int mipLevel, int size, Shader* shader);

private:
    bool m_Initialized = false;

    // 着色器
    std::unique_ptr<Shader> m_EquirectToCubemapShader;
    std::unique_ptr<Shader> m_IrradianceShader;
    std::unique_ptr<Shader> m_PrefilterShader;
    std::unique_ptr<Shader> m_BrdfLUTShader;

    // 共享的 BRDF LUT (所有 IBL 共用)
    std::unique_ptr<Texture> m_SharedBrdfLUT;
    int m_BrdfLUTSize = 512;

    // 投影矩阵 (用于渲染到 cubemap)
    glm::mat4 m_CaptureProjection;
    glm::mat4 m_CaptureViews[6];
};

} // namespace GLRenderer
