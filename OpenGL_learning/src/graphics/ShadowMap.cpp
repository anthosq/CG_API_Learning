#include "ShadowMap.h"
#include <iostream>

namespace GLRenderer {

Ref<ShadowMap> ShadowMap::Create(const Config& config) {
    return Ref<ShadowMap>(new ShadowMap(config));
}

ShadowMap::ShadowMap(const Config& config) : m_Config(config) {
    Create_();
}

ShadowMap::~ShadowMap() {
    for (GLuint fbo : m_FBOs) {
        if (fbo) glDeleteFramebuffers(1, &fbo);
    }
    if (m_TextureArray) glDeleteTextures(1, &m_TextureArray);
}

void ShadowMap::Create_() {
    const uint32_t res    = m_Config.Resolution;
    const uint32_t layers = m_Config.CascadeCount;

    // 创建深度纹理数组
    glGenTextures(1, &m_TextureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_TextureArray);

    // 分配 layers 层，每层 res×res，格式 DEPTH_COMPONENT32F
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F,
                 res, res, static_cast<GLsizei>(layers),
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    // 过滤：GL_LINEAR 在 sampler2DShadow 下触发硬件 2×2 PCF（免费 bilinear 比较）
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 边界钳制：UV 超出 [0,1] 时返回边界颜色（1.0 = 最大深度 = 无阴影）
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);

    // 不启用硬件深度比较（sampler2DArray，由 shader 手动 step() 比较）
    // PCSS Blocker Search 需要读取原始深度值，与硬件比较模式不兼容
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    // 为每个 cascade 创建独立 FBO
    m_FBOs.resize(layers, 0);
    for (uint32_t i = 0; i < layers; i++) {
        glGenFramebuffers(1, &m_FBOs[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBOs[i]);

        // 附加纹理数组的第 i 层作为深度附件
        // 参数: target, attachment, texture, mipLevel, layer
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  m_TextureArray, 0, static_cast<GLint>(i));

        // 不需要颜色附件（仅深度写入）
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        // 验证 FBO 完整性
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[ShadowMap] FBO " << i << " incomplete: 0x"
                      << std::hex << status << std::dec << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    std::cout << "[ShadowMap] Created " << layers << " cascades @ "
              << res << "x" << res << std::endl;
}

void ShadowMap::BindForWriting(uint32_t cascadeIndex) {
    if (cascadeIndex >= m_FBOs.size()) return;
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBOs[cascadeIndex]);
}

void ShadowMap::BindForReading(uint32_t slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_TextureArray);
}

void ShadowMap::Unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace GLRenderer
