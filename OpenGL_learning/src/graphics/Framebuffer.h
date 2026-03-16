#pragma once

#include "utils/GLCommon.h"

namespace GLRenderer {

struct FramebufferSpec {
    uint32_t Width = 1280;
    uint32_t Height = 720;

    // 附件配置
    bool HasColorAttachment = true;
    bool HasDepthAttachment = true;

    // 深度附件是纹理还是渲染缓冲
    // 如果需要采样深度值（如阴影映射），设为 true
    bool DepthAsTexture = false;

    // 采样数（用于 MSAA，1 表示不使用）
    uint32_t Samples = 1;

    // 是否为交换链目标（屏幕帧缓冲）
    bool SwapChainTarget = false;
};

class Framebuffer : public NonCopyable {
public:
    Framebuffer() = default;
    explicit Framebuffer(const FramebufferSpec& spec);
    ~Framebuffer();

    // 移动语义
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    // 绑定/解绑
    void Bind() const;
    void Unbind() const;

    // 调整大小（会重建内部资源）
    void Resize(uint32_t width, uint32_t height);

    // 属性访问
    GLuint GetID() const { return m_ID; }
    GLuint GetColorAttachment() const { return m_ColorAttachment; }
    GLuint GetDepthAttachment() const { return m_DepthAttachment; }

    uint32_t GetWidth() const { return m_Spec.Width; }
    uint32_t GetHeight() const { return m_Spec.Height; }

    const FramebufferSpec& GetSpec() const { return m_Spec; }

    bool IsValid() const { return m_ID != 0; }

    // 静态方法：绑定默认帧缓冲（屏幕）
    static void BindDefault();

private:
    // 根据配置创建/重建帧缓冲
    void Invalidate();

    // 清理资源
    void Cleanup();

    GLuint m_ID = 0;
    GLuint m_ColorAttachment = 0;
    GLuint m_DepthAttachment = 0;  // 可以是纹理或渲染缓冲
    bool m_DepthIsTexture = false;

    FramebufferSpec m_Spec;
};

} // namespace GLRenderer

// 全局命名空间别名
using GLRenderer::Framebuffer;
using GLRenderer::FramebufferSpec;
