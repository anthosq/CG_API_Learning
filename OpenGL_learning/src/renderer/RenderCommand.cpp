#include "RenderCommand.h"

namespace GLRenderer {

glm::vec4 RenderCommand::s_ClearColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);

void RenderCommand::Init() {
    // 启用深度测试
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // 启用混合
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // CubeMap 无缝过滤: 采样跨面边界时插值相邻面, 消除接缝闪烁
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    // 面剔除（默认关闭，避免影响双面渲染如四边形）
    // 需要时可手动调用 RenderCommand::EnableFaceCulling()
    glDisable(GL_CULL_FACE);
    // glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // 设置默认清除颜色
    glClearColor(s_ClearColor.r, s_ClearColor.g, s_ClearColor.b, s_ClearColor.a);

    GL_CHECK_ERROR();
}

// 清除操作

void RenderCommand::SetClearColor(const glm::vec4& color) {
    s_ClearColor = color;
    glClearColor(color.r, color.g, color.b, color.a);
}

void RenderCommand::SetClearColor(float r, float g, float b, float a) {
    SetClearColor(glm::vec4(r, g, b, a));
}

void RenderCommand::Clear() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void RenderCommand::ClearColorBuffer() {
    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderCommand::ClearDepthBuffer() {
    glClear(GL_DEPTH_BUFFER_BIT);
}

void RenderCommand::ClearStencilBuffer() {
    glClear(GL_STENCIL_BUFFER_BIT);
}

// 视口设置
void RenderCommand::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

// 深度测试
void RenderCommand::EnableDepthTest() {
    glEnable(GL_DEPTH_TEST);
}

void RenderCommand::DisableDepthTest() {
    glDisable(GL_DEPTH_TEST);
}

void RenderCommand::SetDepthFunc(GLenum func) {
    glDepthFunc(func);
}

void RenderCommand::SetDepthMask(bool enabled) {
    glDepthMask(enabled ? GL_TRUE : GL_FALSE);
}

// 混合
void RenderCommand::EnableBlending() {
    glEnable(GL_BLEND);
}

void RenderCommand::DisableBlending() {
    glDisable(GL_BLEND);
}

void RenderCommand::SetBlendFunc(GLenum sfactor, GLenum dfactor) {
    glBlendFunc(sfactor, dfactor);
}

void RenderCommand::SetBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB,
                                          GLenum srcAlpha, GLenum dstAlpha) {
    glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

// 模板测试
void RenderCommand::EnableStencilTest() {
    glEnable(GL_STENCIL_TEST);
}

void RenderCommand::DisableStencilTest() {
    glDisable(GL_STENCIL_TEST);
}

void RenderCommand::SetStencilFunc(GLenum func, int ref, unsigned int mask) {
    glStencilFunc(func, ref, mask);
}

void RenderCommand::SetStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass) {
    glStencilOp(sfail, dpfail, dppass);
}

void RenderCommand::SetStencilMask(unsigned int mask) {
    glStencilMask(mask);
}

// 面剔除
void RenderCommand::EnableFaceCulling() {
    glEnable(GL_CULL_FACE);
}

void RenderCommand::DisableFaceCulling() {
    glDisable(GL_CULL_FACE);
}

void RenderCommand::SetCullFace(GLenum face) {
    glCullFace(face);
}

void RenderCommand::SetFrontFace(GLenum mode) {
    glFrontFace(mode);
}

// 多边形模式
void RenderCommand::SetPolygonMode(GLenum face, GLenum mode) {
    glPolygonMode(face, mode);
}

// 绘制命令
void RenderCommand::DrawArrays(GLenum mode, int first, int count) {
    glDrawArrays(mode, first, count);
}

void RenderCommand::DrawIndexed(const VertexArray& vao, uint32_t indexCount) {
    vao.Bind();
    uint32_t count = indexCount;
    if (count == 0 && vao.GetIndexBuffer()) {
        count = vao.GetIndexBuffer()->GetCount();
    }
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
}

void RenderCommand::DrawIndexed(GLenum mode, uint32_t indexCount, GLenum type, const void* indices) {
    glDrawElements(mode, indexCount, type, indices);
}

// 纹理操作
void RenderCommand::BindTextureUnit(uint32_t slot, GLuint textureID) {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, textureID);
}

}
