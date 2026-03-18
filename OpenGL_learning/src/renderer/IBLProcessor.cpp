#include "IBLProcessor.h"
#include "RenderCommand.h"
#include "utils/GLCommon.h"

#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace GLRenderer {

IBLProcessor::IBLProcessor() {
    // 设置 cubemap 投影矩阵 (90度 FOV)
    m_CaptureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

    // 设置 6 个面的视图矩阵
    m_CaptureViews[0] = glm::lookAt(glm::vec3(0), glm::vec3( 1,  0,  0), glm::vec3(0, -1,  0)); // +X
    m_CaptureViews[1] = glm::lookAt(glm::vec3(0), glm::vec3(-1,  0,  0), glm::vec3(0, -1,  0)); // -X
    m_CaptureViews[2] = glm::lookAt(glm::vec3(0), glm::vec3( 0,  1,  0), glm::vec3(0,  0,  1)); // +Y
    m_CaptureViews[3] = glm::lookAt(glm::vec3(0), glm::vec3( 0, -1,  0), glm::vec3(0,  0, -1)); // -Y
    m_CaptureViews[4] = glm::lookAt(glm::vec3(0), glm::vec3( 0,  0,  1), glm::vec3(0, -1,  0)); // +Z
    m_CaptureViews[5] = glm::lookAt(glm::vec3(0), glm::vec3( 0,  0, -1), glm::vec3(0, -1,  0)); // -Z
}

IBLProcessor::~IBLProcessor() {
    Shutdown();
}

void IBLProcessor::Initialize(const std::filesystem::path& shaderDir) {
    if (m_Initialized) return;

    // 加载着色器
    auto iblDir = shaderDir / "ibl";

    m_EquirectToCubemapShader = std::make_unique<Shader>(iblDir / "equirect_to_cubemap.glsl");
    m_IrradianceShader = std::make_unique<Shader>(iblDir / "irradiance.glsl");
    m_PrefilterShader = std::make_unique<Shader>(iblDir / "prefilter.glsl");
    m_BrdfLUTShader = std::make_unique<Shader>(iblDir / "brdf_lut.glsl");

    if (!m_EquirectToCubemapShader->IsValid() ||
        !m_IrradianceShader->IsValid() ||
        !m_PrefilterShader->IsValid() ||
        !m_BrdfLUTShader->IsValid()) {
        std::cerr << "[IBLProcessor] Failed to load IBL shaders" << std::endl;
        return;
    }

    m_Initialized = true;
    std::cout << "[IBLProcessor] Initialized successfully" << std::endl;
}

void IBLProcessor::Shutdown() {
    m_EquirectToCubemapShader.reset();
    m_IrradianceShader.reset();
    m_PrefilterShader.reset();
    m_BrdfLUTShader.reset();
    m_SharedBrdfLUT.reset();
    m_Initialized = false;
}

std::unique_ptr<IBLData> IBLProcessor::ProcessHDRI(
    const std::filesystem::path& hdrPath,
    int envMapSize, int irradianceSize, int prefilterSize) {

    if (!m_Initialized) {
        std::cerr << "[IBLProcessor] Not initialized" << std::endl;
        return nullptr;
    }

    auto data = std::make_unique<IBLData>();

    // Step 1: Convert HDR equirectangular to cubemap
    data->EnvironmentMap = EquirectangularToCubemap(hdrPath, envMapSize);
    if (!data->EnvironmentMap) {
        std::cerr << "[IBLProcessor] Failed to create environment cubemap" << std::endl;
        return nullptr;
    }

    // Step 2: Generate irradiance map
    data->IrradianceMap = GenerateIrradianceMap(data->EnvironmentMap.get(), irradianceSize);

    // Step 3: Generate prefiltered environment map
    data->PrefilterMap = GeneratePrefilterMap(data->EnvironmentMap.get(), prefilterSize);

    // Step 4: Get/Generate BRDF LUT
    data->BrdfLUT.reset(GetBrdfLUT());
    // Note: BrdfLUT ownership is shared, we copy the pointer but don't own it
    // Actually, let's create a new one for this IBLData
    data->BrdfLUT = GenerateBrdfLUT(m_BrdfLUTSize);

    std::cout << "[IBLProcessor] Processed HDRI: " << hdrPath << std::endl;

    return data;
}

std::unique_ptr<IBLData> IBLProcessor::ProcessCubemap(
    TextureCube* envMap, int irradianceSize, int prefilterSize) {

    if (!m_Initialized || !envMap) {
        return nullptr;
    }

    auto data = std::make_unique<IBLData>();

    // Step 1: Generate irradiance map
    data->IrradianceMap = GenerateIrradianceMap(envMap, irradianceSize);

    // Step 2: Generate prefiltered environment map
    data->PrefilterMap = GeneratePrefilterMap(envMap, prefilterSize);

    // Step 3: Generate BRDF LUT
    data->BrdfLUT = GenerateBrdfLUT(m_BrdfLUTSize);

    return data;
}

Texture* IBLProcessor::GetBrdfLUT() {
    if (!m_SharedBrdfLUT) {
        m_SharedBrdfLUT = GenerateBrdfLUT(m_BrdfLUTSize);
    }
    return m_SharedBrdfLUT.get();
}

std::unique_ptr<TextureCube> IBLProcessor::EquirectangularToCubemap(
    const std::filesystem::path& hdrPath, int size) {

    // Load HDR image
    stbi_set_flip_vertically_on_load(true);
    int width, height, nrComponents;
    float* data = stbi_loadf(hdrPath.string().c_str(), &width, &height, &nrComponents, 0);

    if (!data) {
        std::cerr << "[IBLProcessor] Failed to load HDR: " << hdrPath << std::endl;
        return nullptr;
    }

    // Create HDR texture
    GLuint hdrTexture;
    glGenTextures(1, &hdrTexture);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);

    // Create cubemap texture
    auto cubemap = std::make_unique<TextureCube>();
    GLuint cubemapID;
    glGenTextures(1, &cubemapID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapID);

    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                     size, size, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Create FBO for rendering
    GLuint captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    // Render each face
    m_EquirectToCubemapShader->Bind();
    m_EquirectToCubemapShader->SetInt("u_EquirectangularMap", 0);
    m_EquirectToCubemapShader->SetMat4("u_Projection", m_CaptureProjection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);

    glViewport(0, 0, size, size);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    for (int i = 0; i < 6; ++i) {
        m_EquirectToCubemapShader->SetMat4("u_View", m_CaptureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemapID, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render cube
        MeshFactory::Get().GetCube()->Draw();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Generate mipmaps
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapID);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // Cleanup
    glDeleteTextures(1, &hdrTexture);
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);

    // Set cubemap ID (need to expose this in TextureCube)
    // For now, create a new TextureCube that wraps this ID
    cubemap->SetID(cubemapID);

    return cubemap;
}

std::unique_ptr<TextureCube> IBLProcessor::GenerateIrradianceMap(
    TextureCube* envMap, int size) {

    // Create irradiance cubemap
    GLuint irradianceMap;
    glGenTextures(1, &irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);

    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                     size, size, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Create FBO
    GLuint captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    // Render each face
    m_IrradianceShader->Bind();
    m_IrradianceShader->SetInt("u_EnvironmentMap", 0);
    m_IrradianceShader->SetMat4("u_Projection", m_CaptureProjection);

    glActiveTexture(GL_TEXTURE0);
    envMap->Bind();

    glViewport(0, 0, size, size);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    for (int i = 0; i < 6; ++i) {
        m_IrradianceShader->SetMat4("u_View", m_CaptureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        MeshFactory::Get().GetCube()->Draw();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Cleanup
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);

    auto result = std::make_unique<TextureCube>();
    result->SetID(irradianceMap);

    return result;
}

std::unique_ptr<TextureCube> IBLProcessor::GeneratePrefilterMap(
    TextureCube* envMap, int size) {

    // Create prefilter cubemap with mipmaps
    GLuint prefilterMap;
    glGenTextures(1, &prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);

    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                     size, size, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // Create FBO
    GLuint captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    // Render each mip level
    m_PrefilterShader->Bind();
    m_PrefilterShader->SetInt("u_EnvironmentMap", 0);
    m_PrefilterShader->SetMat4("u_Projection", m_CaptureProjection);
    m_PrefilterShader->SetFloat("u_Resolution", static_cast<float>(size));

    glActiveTexture(GL_TEXTURE0);
    envMap->Bind();

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    const int maxMipLevels = 5;
    for (int mip = 0; mip < maxMipLevels; ++mip) {
        // Resize framebuffer according to mip-level size
        int mipWidth = static_cast<int>(size * std::pow(0.5f, mip));
        int mipHeight = mipWidth;

        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
        m_PrefilterShader->SetFloat("u_Roughness", roughness);

        for (int i = 0; i < 6; ++i) {
            m_PrefilterShader->SetMat4("u_View", m_CaptureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            MeshFactory::Get().GetCube()->Draw();
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Cleanup
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);

    auto result = std::make_unique<TextureCube>();
    result->SetID(prefilterMap);

    return result;
}

std::unique_ptr<Texture> IBLProcessor::GenerateBrdfLUT(int size) {
    // Create 2D LUT texture
    GLuint brdfLUTTexture;
    glGenTextures(1, &brdfLUTTexture);
    glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, size, size, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Create FBO
    GLuint captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);

    glViewport(0, 0, size, size);
    m_BrdfLUTShader->Bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render full-screen quad
    MeshFactory::Get().GetQuad()->Draw();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Cleanup
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);

    auto result = std::make_unique<Texture>();
    result->SetID(brdfLUTTexture);

    return result;
}

void IBLProcessor::RenderCubemapFace(GLuint fbo, GLuint cubemap, int face,
                                      int mipLevel, int size, Shader* shader) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cubemap, mipLevel);
    glViewport(0, 0, size, size);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader->SetMat4("u_View", m_CaptureViews[face]);
    MeshFactory::Get().GetCube()->Draw();
}

} // namespace GLRenderer
