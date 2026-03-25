#include "GBuffer.h"
#include "utils/GLCommon.h"
#include <cassert>

namespace GLRenderer {

// static factory
Ref<GBuffer> GBuffer::Create(const GBufferSpec& spec) {
    return Ref<GBuffer>(new GBuffer(spec));
}

GBuffer::GBuffer(const GBufferSpec& spec)
    : m_Spec(spec)
{
    Create_();
}

GBuffer::~GBuffer() {
    Destroy();
}

void GBuffer::Bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Spec.Width, m_Spec.Height);
}

void GBuffer::Unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBuffer::ClearEntityID(int value) const {
    glClearBufferiv(GL_COLOR, 3, &value);  // attachment index 3 = EntityID
}

int GBuffer::ReadEntityID(int x, int y) const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);
    glReadBuffer(GL_COLOR_ATTACHMENT3);
    int id = -1;
    glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &id);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    return id;
}

void GBuffer::Resize(uint32_t width, uint32_t height) {
    if (m_Spec.Width == width && m_Spec.Height == height)
        return;
    m_Spec.Width  = width;
    m_Spec.Height = height;
    Destroy();
    Create_();
}

// 创建一张 GL_TEXTURE_2D 并设置滤波/边缘（无 mip）
static GLuint CreateTex2D(GLenum internalFmt, GLenum fmt, GLenum type,
                           uint32_t w, uint32_t h)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void GBuffer::Create_() {
    const uint32_t w = m_Spec.Width;
    const uint32_t h = m_Spec.Height;

    // Att0: BaseColor.rgb + AO.a
    m_TexBaseColorAO = CreateTex2D(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, w, h);
    // Att1: Oct(Normal).rg + Roughness.b + Metallic.a
    m_TexNormalRoughMetal = CreateTex2D(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, w, h);
    // Att2: Emissive.rgb + ShadingModelID.a
    m_TexEmissiveShadingID = CreateTex2D(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, w, h);
    // Att3: EntityID（整数格式，不做线性插值）
    m_TexEntityID = CreateTex2D(GL_R32I, GL_RED_INTEGER, GL_INT, w, h);
    // Depth: DEPTH24_STENCIL8（纹理，供 TiledLightCull / SkyboxPass blit 采样）
    m_TexDepth = CreateTex2D(GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, w, h);

    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_TexBaseColorAO,       0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_TexNormalRoughMetal,  0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_TexEmissiveShadingID, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, m_TexEntityID,          0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_TexDepth,      0);

    // 指定所有 color attachment 均参与写入
    constexpr GLenum drawBuffers[4] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3
    };
    glDrawBuffers(4, drawBuffers);

    // 验证完整性
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    assert(status == GL_FRAMEBUFFER_COMPLETE && "GBuffer FBO incomplete!");
    (void)status;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBuffer::Destroy() {
    if (m_FBO) { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }

    GLuint textures[] = {
        m_TexBaseColorAO, m_TexNormalRoughMetal,
        m_TexEmissiveShadingID, m_TexEntityID, m_TexDepth
    };
    glDeleteTextures(5, textures);

    m_TexBaseColorAO       = 0;
    m_TexNormalRoughMetal  = 0;
    m_TexEmissiveShadingID = 0;
    m_TexEntityID          = 0;
    m_TexDepth             = 0;
}

} // namespace GLRenderer
