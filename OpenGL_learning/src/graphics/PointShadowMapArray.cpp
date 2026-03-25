#include "PointShadowMapArray.h"
#include <iostream>

namespace GLRenderer {

Ref<PointShadowMapArray> PointShadowMapArray::Create(const Config& config) {
    return Ref<PointShadowMapArray>(new PointShadowMapArray(config));
}

PointShadowMapArray::PointShadowMapArray(const Config& config) : m_Config(config) {
    Create_();
}

PointShadowMapArray::~PointShadowMapArray() {
    for (GLuint fbo : m_FBOs) {
        if (fbo) glDeleteFramebuffers(1, &fbo);
    }
    if (m_CubemapArray) glDeleteTextures(1, &m_CubemapArray);
}

void PointShadowMapArray::Create_() {
    const uint32_t res   = m_Config.Resolution;
    const uint32_t count = m_Config.Count;

    // GL_TEXTURE_CUBE_MAP_ARRAY: depth 参数 = numCubemaps * 6 (layer-faces)
    glGenTextures(1, &m_CubemapArray);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_CubemapArray);

    glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT32F,
                 res, res, static_cast<GLsizei>(count * 6),
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    // 手动比较（samplerCubeArray），不需要 GL_COMPARE_REF_TO_TEXTURE
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    // 每个 (lightSlot, face) 对应一个 FBO
    // layer-face 索引 = lightSlot * 6 + faceIndex
    m_FBOs.resize(count * 6, 0);
    for (uint32_t slot = 0; slot < count; slot++) {
        for (uint32_t face = 0; face < 6; face++) {
            const uint32_t fboIdx = slot * 6 + face;
            glGenFramebuffers(1, &m_FBOs[fboIdx]);
            glBindFramebuffer(GL_FRAMEBUFFER, m_FBOs[fboIdx]);

            // glFramebufferTextureLayer: layer = lightSlot * 6 + faceIndex
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                      m_CubemapArray, 0,
                                      static_cast<GLint>(fboIdx));

            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE) {
                std::cerr << "[PointShadowMapArray] FBO [slot=" << slot
                          << " face=" << face << "] incomplete: 0x"
                          << std::hex << status << std::dec << std::endl;
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    std::cout << "[PointShadowMapArray] Created " << count
              << " cubemaps @ " << res << "x" << res << std::endl;
}

void PointShadowMapArray::BindFaceForWriting(uint32_t lightSlot, uint32_t faceIndex) {
    if (lightSlot >= m_Config.Count || faceIndex >= 6) return;
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBOs[lightSlot * 6 + faceIndex]);
}

void PointShadowMapArray::BindForReading(uint32_t slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, m_CubemapArray);
}

void PointShadowMapArray::Unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace GLRenderer
