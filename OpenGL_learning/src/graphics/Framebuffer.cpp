#include "Framebuffer.h"

namespace GLRenderer {

Framebuffer::Framebuffer(const FramebufferSpec& spec)
    : m_Spec(spec) {
    Invalidate();
}

Framebuffer::~Framebuffer() {
    Cleanup();
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : m_ID(other.m_ID),
      m_ColorAttachment(other.m_ColorAttachment),
      m_EntityIDAttachment(other.m_EntityIDAttachment),
      m_DepthAttachment(other.m_DepthAttachment),
      m_DepthIsTexture(other.m_DepthIsTexture),
      m_Spec(other.m_Spec) {
    other.m_ID = 0;
    other.m_ColorAttachment = 0;
    other.m_EntityIDAttachment = 0;
    other.m_DepthAttachment = 0;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept {
    if (this != &other) {
        Cleanup();

        m_ID = other.m_ID;
        m_ColorAttachment = other.m_ColorAttachment;
        m_EntityIDAttachment = other.m_EntityIDAttachment;
        m_DepthAttachment = other.m_DepthAttachment;
        m_DepthIsTexture = other.m_DepthIsTexture;
        m_Spec = other.m_Spec;

        other.m_ID = 0;
        other.m_ColorAttachment = 0;
        other.m_EntityIDAttachment = 0;
        other.m_DepthAttachment = 0;
    }
    return *this;
}

void Framebuffer::Bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_ID);
    glViewport(0, 0, m_Spec.Width, m_Spec.Height);
}

void Framebuffer::Unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::BindDefault() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        std::cerr << "[Framebuffer] 无效的尺寸: " << width << "x" << height << std::endl;
        return;
    }

    m_Spec.Width = width;
    m_Spec.Height = height;
    Invalidate();
}

void Framebuffer::Invalidate() {
    // 清理旧资源
    Cleanup();

    // 创建帧缓冲
    glGenFramebuffers(1, &m_ID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_ID);

    std::vector<GLenum> drawBuffers;

    // 创建颜色附件
    if (m_Spec.HasColorAttachment) {
        glGenTextures(1, &m_ColorAttachment);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     m_Spec.Width, m_Spec.Height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, m_ColorAttachment, 0);
        drawBuffers.push_back(GL_COLOR_ATTACHMENT0);
    }

    // 创建实体 ID 附件（用于鼠标拾取）
    if (m_Spec.HasEntityIDAttachment) {
        glGenTextures(1, &m_EntityIDAttachment);
        glBindTexture(GL_TEXTURE_2D, m_EntityIDAttachment);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I,
                     m_Spec.Width, m_Spec.Height, 0,
                     GL_RED_INTEGER, GL_INT, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                               GL_TEXTURE_2D, m_EntityIDAttachment, 0);
        drawBuffers.push_back(GL_COLOR_ATTACHMENT1);
    }

    // 设置绘制缓冲
    if (!drawBuffers.empty()) {
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
    }

    // 创建深度/模板附件
    if (m_Spec.HasDepthAttachment) {
        m_DepthIsTexture = m_Spec.DepthAsTexture;

        if (m_DepthIsTexture) {
            // 创建深度纹理（可采样）
            glGenTextures(1, &m_DepthAttachment);
            glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8,
                         m_Spec.Width, m_Spec.Height, 0,
                         GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

            float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D, m_DepthAttachment, 0);
        } else {
            // 创建渲染缓冲（不可采样，但更高效）
            glGenRenderbuffers(1, &m_DepthAttachment);
            glBindRenderbuffer(GL_RENDERBUFFER, m_DepthAttachment);

            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                                  m_Spec.Width, m_Spec.Height);

            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                      GL_RENDERBUFFER, m_DepthAttachment);
        }
    }

    // 检查帧缓冲完整性
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[Framebuffer] 帧缓冲不完整! 状态: 0x"
                  << std::hex << status << std::dec << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GL_CHECK_ERROR();
}

void Framebuffer::Cleanup() {
    if (m_ID != 0) {
        glDeleteFramebuffers(1, &m_ID);
        m_ID = 0;
    }

    if (m_ColorAttachment != 0) {
        glDeleteTextures(1, &m_ColorAttachment);
        m_ColorAttachment = 0;
    }

    if (m_EntityIDAttachment != 0) {
        glDeleteTextures(1, &m_EntityIDAttachment);
        m_EntityIDAttachment = 0;
    }

    if (m_DepthAttachment != 0) {
        if (m_DepthIsTexture) {
            glDeleteTextures(1, &m_DepthAttachment);
        } else {
            glDeleteRenderbuffers(1, &m_DepthAttachment);
        }
        m_DepthAttachment = 0;
    }
}

int Framebuffer::ReadPixel(int x, int y) const {
    if (!m_Spec.HasEntityIDAttachment || m_EntityIDAttachment == 0) {
        return -1;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_ID);
    glReadBuffer(GL_COLOR_ATTACHMENT1);

    int pixelData = -1;
    glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return pixelData;
}

void Framebuffer::ClearEntityID(int value) {
    if (!m_Spec.HasEntityIDAttachment || m_EntityIDAttachment == 0) {
        return;
    }

    // 注意：假设 FBO 已经绑定，不要在这里解绑
    // 直接清除第二个颜色附件
    GLint clearValue = value;
    glClearBufferiv(GL_COLOR, 1, &clearValue);
}

} // namespace GLRenderer
