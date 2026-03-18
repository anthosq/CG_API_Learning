#pragma once

// Renderer2D - 2D 渲染器
//
// 参考 Hazel Engine 的设计:
// - 批处理渲染 (Quad Batching)
// - 纹理槽位批处理
// - Billboard 渲染支持
//
// 使用方式:
//   Renderer2D::BeginScene(camera, aspectRatio);
//   Renderer2D::DrawQuad(...);
//   Renderer2D::DrawBillboard(...);
//   Renderer2D::EndScene();  // 提交并渲染

#include "graphics/Shader.h"
#include "graphics/Texture.h"
#include "graphics/Buffer.h"
#include "graphics/Camera.h"

#include <glm/glm.hpp>
#include <array>
#include <memory>

namespace GLRenderer {

// 2D 渲染统计
struct Renderer2DStats {
    uint32_t DrawCalls = 0;
    uint32_t QuadCount = 0;

    uint32_t GetTotalVertexCount() const { return QuadCount * 4; }
    uint32_t GetTotalIndexCount() const { return QuadCount * 6; }
};

class Renderer2D {
public:
    // 初始化/关闭
    static void Init();
    static void Shutdown();

    // 场景渲染
    static void BeginScene(const Camera& camera, float aspectRatio);
    static void BeginScene(const glm::mat4& viewProjection);
    static void EndScene();
    static void Flush();

    // === 绘制 API ===

    // 绘制纯色 Quad
    static void DrawQuad(const glm::vec2& position, const glm::vec2& size,
                         const glm::vec4& color);
    static void DrawQuad(const glm::vec3& position, const glm::vec2& size,
                         const glm::vec4& color);

    // 绘制带纹理的 Quad
    static void DrawQuad(const glm::vec2& position, const glm::vec2& size,
                         Texture* texture, const glm::vec4& tint = glm::vec4(1.0f));
    static void DrawQuad(const glm::vec3& position, const glm::vec2& size,
                         Texture* texture, const glm::vec4& tint = glm::vec4(1.0f));

    // 绘制带旋转的 Quad
    static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size,
                                float rotation, const glm::vec4& color);
    static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size,
                                float rotation, const glm::vec4& color);
    static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size,
                                float rotation, Texture* texture,
                                const glm::vec4& tint = glm::vec4(1.0f));

    // 绘制 Billboard (面向相机的 Quad)
    static void DrawBillboard(const glm::vec3& position, float size,
                              const glm::vec4& color);
    static void DrawBillboard(const glm::vec3& position, float size,
                              Texture* texture,
                              const glm::vec4& tint = glm::vec4(1.0f));
    static void DrawBillboard(const glm::vec3& position, const glm::vec2& size,
                              Texture* texture,
                              const glm::vec4& tint = glm::vec4(1.0f),
                              int entityID = -1);

    // 统计
    static void ResetStats();
    static Renderer2DStats GetStats();

private:
    static void StartBatch();
    static void NextBatch();
};

} // namespace GLRenderer
