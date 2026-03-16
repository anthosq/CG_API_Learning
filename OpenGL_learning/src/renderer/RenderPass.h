#pragma once

#include "RenderCommand.h"
#include "Renderer.h"
#include "graphics/Framebuffer.h"
#include "graphics/Shader.h"
#include <vector>
#include <memory>
#include <functional>

namespace GLRenderer {

// RenderPass - 渲染通道抽象基类
class RenderPass : public NonCopyable {
public:
    virtual ~RenderPass() = default;

    // 执行渲染通道
    virtual void Execute() = 0;

    // 通道名称（用于调试）
    virtual const char* GetName() const = 0;

    // 是否启用
    bool IsEnabled() const { return m_Enabled; }
    void SetEnabled(bool enabled) { m_Enabled = enabled; }

protected:
    bool m_Enabled = true;
};

// ClearPass - 清除缓冲区通道
class ClearPass : public RenderPass {
public:
    enum ClearFlags {
        Color   = 1 << 0,
        Depth   = 1 << 1,
        Stencil = 1 << 2,
        All     = Color | Depth | Stencil
    };

    explicit ClearPass(uint32_t flags = All, const glm::vec4& clearColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f))
        : m_Flags(flags), m_ClearColor(clearColor) {}

    void Execute() override {
        if (!m_Enabled) return;

        if (m_Flags & Color) {
            RenderCommand::SetClearColor(m_ClearColor);
        }

        if (m_Flags == All) {
            RenderCommand::Clear();
        } else {
            if (m_Flags & Color)   RenderCommand::ClearColorBuffer();
            if (m_Flags & Depth)   RenderCommand::ClearDepthBuffer();
            if (m_Flags & Stencil) RenderCommand::ClearStencilBuffer();
        }
    }

    const char* GetName() const override { return "ClearPass"; }

    void SetClearColor(const glm::vec4& color) { m_ClearColor = color; }
    void SetFlags(uint32_t flags) { m_Flags = flags; }

private:
    uint32_t m_Flags;
    glm::vec4 m_ClearColor;
};

// FramebufferPass - 帧缓冲通道基类
class FramebufferPass : public RenderPass {
public:
    explicit FramebufferPass(Framebuffer* framebuffer = nullptr)
        : m_Framebuffer(framebuffer) {}

    void SetFramebuffer(Framebuffer* fb) { m_Framebuffer = fb; }
    Framebuffer* GetFramebuffer() const { return m_Framebuffer; }

protected:
    void BindFramebuffer() {
        if (m_Framebuffer) {
            m_Framebuffer->Bind();
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }

    void UnbindFramebuffer() {
        if (m_Framebuffer) {
            m_Framebuffer->Unbind();
        }
    }

protected:
    Framebuffer* m_Framebuffer = nullptr;  // nullptr 表示默认帧缓冲
};

// GeometryPass - 几何渲染通道（使用回调）
class GeometryPass : public FramebufferPass {
public:
    using RenderCallback = std::function<void()>;

    GeometryPass(const char* name = "GeometryPass", Framebuffer* fb = nullptr)
        : FramebufferPass(fb), m_Name(name) {}

    void Execute() override {
        if (!m_Enabled) return;

        BindFramebuffer();

        if (m_RenderCallback) {
            m_RenderCallback();
        }

        UnbindFramebuffer();
    }

    const char* GetName() const override { return m_Name; }

    void SetRenderCallback(RenderCallback callback) { m_RenderCallback = std::move(callback); }

private:
    const char* m_Name;
    RenderCallback m_RenderCallback;
};

// FullscreenPass - 全屏后处理通道
class FullscreenPass : public FramebufferPass {
public:
    FullscreenPass(Shader* shader = nullptr, const char* name = "FullscreenPass")
        : FramebufferPass(nullptr), m_Shader(shader), m_Name(name) {}

    void Execute() override {
        if (!m_Enabled || !m_Shader) return;

        BindFramebuffer();

        m_Shader->Bind();

        // 绑定输入纹理
        for (size_t i = 0; i < m_InputTextures.size(); ++i) {
            RenderCommand::BindTextureUnit(static_cast<uint32_t>(i), m_InputTextures[i].textureID);
            m_Shader->SetInt(m_InputTextures[i].uniformName, static_cast<int>(i));
        }

        // 设置自定义 uniform 回调
        if (m_SetupCallback) {
            m_SetupCallback(*m_Shader);
        }

        // 绘制全屏四边形
        Renderer::DrawFullscreenQuad();

        UnbindFramebuffer();
    }

    const char* GetName() const override { return m_Name; }

    void SetShader(Shader* shader) { m_Shader = shader; }
    Shader* GetShader() const { return m_Shader; }

    // 添加输入纹理
    void AddInputTexture(GLuint textureID, const std::string& uniformName) {
        m_InputTextures.push_back({ textureID, uniformName });
    }

    void ClearInputTextures() { m_InputTextures.clear(); }

    // 设置自定义 uniform 回调
    using SetupCallback = std::function<void(Shader&)>;
    void SetSetupCallback(SetupCallback callback) { m_SetupCallback = std::move(callback); }

private:
    struct TextureBinding {
        GLuint textureID;
        std::string uniformName;
    };

    Shader* m_Shader = nullptr;
    const char* m_Name;
    std::vector<TextureBinding> m_InputTextures;
    SetupCallback m_SetupCallback;
};

// StencilPass - 模板测试通道
class StencilPass : public RenderPass {
public:
    struct StencilConfig {
        GLenum func = GL_ALWAYS;
        int ref = 1;
        unsigned int mask = 0xFF;
        GLenum sfail = GL_KEEP;
        GLenum dpfail = GL_KEEP;
        GLenum dppass = GL_REPLACE;
        unsigned int writeMask = 0xFF;
    };

    using RenderCallback = std::function<void()>;

    StencilPass(const char* name = "StencilPass")
        : m_Name(name) {}

    void Execute() override {
        if (!m_Enabled) return;

        // 启用模板测试
        RenderCommand::EnableStencilTest();
        RenderCommand::SetStencilMask(m_Config.writeMask);
        RenderCommand::SetStencilFunc(m_Config.func, m_Config.ref, m_Config.mask);
        RenderCommand::SetStencilOp(m_Config.sfail, m_Config.dpfail, m_Config.dppass);

        if (m_RenderCallback) {
            m_RenderCallback();
        }
    }

    const char* GetName() const override { return m_Name; }

    void SetConfig(const StencilConfig& config) { m_Config = config; }
    StencilConfig& GetConfig() { return m_Config; }

    void SetRenderCallback(RenderCallback callback) { m_RenderCallback = std::move(callback); }

private:
    const char* m_Name;
    StencilConfig m_Config;
    RenderCallback m_RenderCallback;
};

// RenderPipeline - 渲染管线（组合多个通道）
class RenderPipeline : public NonCopyable {
public:
    RenderPipeline() = default;

    // 添加渲染通道
    void AddPass(std::unique_ptr<RenderPass> pass) {
        m_Passes.push_back(std::move(pass));
    }

    // 执行所有通道
    void Execute() {
        for (auto& pass : m_Passes) {
            if (pass->IsEnabled()) {
                pass->Execute();
            }
        }
    }

    // 获取通道（按索引）
    RenderPass* GetPass(size_t index) {
        return index < m_Passes.size() ? m_Passes[index].get() : nullptr;
    }

    // 获取通道数量
    size_t GetPassCount() const { return m_Passes.size(); }

    // 清空所有通道
    void Clear() { m_Passes.clear(); }

    // 启用/禁用所有通道
    void SetAllEnabled(bool enabled) {
        for (auto& pass : m_Passes) {
            pass->SetEnabled(enabled);
        }
    }

private:
    std::vector<std::unique_ptr<RenderPass>> m_Passes;
};

// 渲染状态辅助结构
struct RenderState {
    // 深度测试
    bool depthTestEnabled = true;
    GLenum depthFunc = GL_LESS;
    bool depthWriteEnabled = true;

    // 混合
    bool blendEnabled = false;
    GLenum blendSrcFactor = GL_SRC_ALPHA;
    GLenum blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;

    // 面剔除
    bool cullFaceEnabled = true;
    GLenum cullFace = GL_BACK;
    GLenum frontFace = GL_CCW;

    // 多边形模式
    GLenum polygonMode = GL_FILL;

    // 应用状态
    void Apply() const {
        // 深度测试
        if (depthTestEnabled) {
            RenderCommand::EnableDepthTest();
            RenderCommand::SetDepthFunc(depthFunc);
        } else {
            RenderCommand::DisableDepthTest();
        }
        RenderCommand::SetDepthMask(depthWriteEnabled);

        // 混合
        if (blendEnabled) {
            RenderCommand::EnableBlending();
            RenderCommand::SetBlendFunc(blendSrcFactor, blendDstFactor);
        } else {
            RenderCommand::DisableBlending();
        }

        // 面剔除
        if (cullFaceEnabled) {
            RenderCommand::EnableFaceCulling();
            RenderCommand::SetCullFace(cullFace);
            RenderCommand::SetFrontFace(frontFace);
        } else {
            RenderCommand::DisableFaceCulling();
        }

        // 多边形模式
        RenderCommand::SetPolygonMode(GL_FRONT_AND_BACK, polygonMode);
    }
};

// 预定义的渲染状态
namespace RenderStates {
    // 默认不透明渲染状态
    inline RenderState Opaque() {
        RenderState state;
        state.depthTestEnabled = true;
        state.depthWriteEnabled = true;
        state.blendEnabled = false;
        state.cullFaceEnabled = true;
        return state;
    }

    // 透明物体渲染状态
    inline RenderState Transparent() {
        RenderState state;
        state.depthTestEnabled = true;
        state.depthWriteEnabled = false;  // 透明物体不写入深度
        state.blendEnabled = true;
        state.blendSrcFactor = GL_SRC_ALPHA;
        state.blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
        state.cullFaceEnabled = false;    // 双面渲染
        return state;
    }

    // 后处理渲染状态
    inline RenderState PostProcess() {
        RenderState state;
        state.depthTestEnabled = false;
        state.depthWriteEnabled = false;
        state.blendEnabled = false;
        state.cullFaceEnabled = false;
        return state;
    }

    // 线框渲染状态
    inline RenderState Wireframe() {
        RenderState state;
        state.depthTestEnabled = true;
        state.polygonMode = GL_LINE;
        state.cullFaceEnabled = false;
        return state;
    }
}

} // namespace GLRenderer

using GLRenderer::RenderPass;
using GLRenderer::ClearPass;
using GLRenderer::FramebufferPass;
using GLRenderer::GeometryPass;
using GLRenderer::FullscreenPass;
using GLRenderer::StencilPass;
using GLRenderer::RenderPipeline;
using GLRenderer::RenderState;
