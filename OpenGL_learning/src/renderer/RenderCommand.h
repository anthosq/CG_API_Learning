#pragma once

#include "utils/GLCommon.h"
#include "graphics/Buffer.h"

namespace GLRenderer {

// ============================================================================
// RenderCommand - OpenGL 渲染命令封装（静态类）
// ============================================================================
class RenderCommand {
public:
    // ========================================================================
    // 初始化
    // ========================================================================

    static void Init();

    // ========================================================================
    // 清除操作
    // ========================================================================

    static void SetClearColor(const glm::vec4& color);
    static void SetClearColor(float r, float g, float b, float a = 1.0f);
    static void Clear();
    static void ClearColorBuffer();
    static void ClearDepthBuffer();
    static void ClearStencilBuffer();

    // ========================================================================
    // 视口设置
    // ========================================================================

    static void SetViewport(int x, int y, int width, int height);

    // ========================================================================
    // 渲染状态
    // ========================================================================

    // 深度测试
    static void EnableDepthTest();
    static void DisableDepthTest();
    static void SetDepthFunc(GLenum func);  // GL_LESS, GL_LEQUAL, etc.
    static void SetDepthMask(bool enabled);  // 是否写入深度缓冲

    // 混合
    static void EnableBlending();
    static void DisableBlending();
    static void SetBlendFunc(GLenum sfactor, GLenum dfactor);
    static void SetBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB,
                                      GLenum srcAlpha, GLenum dstAlpha);

    // 模板测试
    static void EnableStencilTest();
    static void DisableStencilTest();
    static void SetStencilFunc(GLenum func, int ref, unsigned int mask);
    static void SetStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass);
    static void SetStencilMask(unsigned int mask);

    // 面剔除
    static void EnableFaceCulling();
    static void DisableFaceCulling();
    static void SetCullFace(GLenum face);  // GL_BACK, GL_FRONT, GL_FRONT_AND_BACK
    static void SetFrontFace(GLenum mode);  // GL_CCW, GL_CW

    // 多边形模式
    static void SetPolygonMode(GLenum face, GLenum mode);  // GL_FILL, GL_LINE, GL_POINT

    // ========================================================================
    // 绘制命令
    // ========================================================================

    static void DrawArrays(GLenum mode, int first, int count);
    static void DrawIndexed(const VertexArray& vao, uint32_t indexCount = 0);
    static void DrawIndexed(GLenum mode, uint32_t indexCount, GLenum type, const void* indices);

    // ========================================================================
    // 纹理操作
    // ========================================================================

    static void BindTextureUnit(uint32_t slot, GLuint textureID);

private:
    static glm::vec4 s_ClearColor;
};

} // namespace GLRenderer

using GLRenderer::RenderCommand;
