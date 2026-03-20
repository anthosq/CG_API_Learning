#pragma once

#include "graphics/Shader.h"
#include "graphics/Texture.h"
#include "graphics/Buffer.h"
#include "graphics/Camera.h"
#include "core/Ref.h"

#include <glm/glm.hpp>
#include <array>

namespace GLRenderer {

struct Renderer2DStats {
    uint32_t DrawCalls = 0;
    uint32_t QuadCount = 0;

    uint32_t GetTotalVertexCount() const { return QuadCount * 4; }
    uint32_t GetTotalIndexCount() const { return QuadCount * 6; }
};

class Renderer2D {
public:
    static void Init();
    static void Shutdown();

    static void BeginScene(const Camera& camera, float aspectRatio);
    static void BeginScene(const glm::mat4& viewProjection);
    static void EndScene();
    static void Flush();

    static void DrawQuad(const glm::vec2& position, const glm::vec2& size,
                         const glm::vec4& color);
    static void DrawQuad(const glm::vec3& position, const glm::vec2& size,
                         const glm::vec4& color);

    static void DrawQuad(const glm::vec2& position, const glm::vec2& size,
                         const Ref<Texture2D>& texture, const glm::vec4& tint = glm::vec4(1.0f));
    static void DrawQuad(const glm::vec3& position, const glm::vec2& size,
                         const Ref<Texture2D>& texture, const glm::vec4& tint = glm::vec4(1.0f));

    static void DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size,
                                float rotation, const glm::vec4& color);
    static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size,
                                float rotation, const glm::vec4& color);
    static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size,
                                float rotation, const Ref<Texture2D>& texture,
                                const glm::vec4& tint = glm::vec4(1.0f));

    static void DrawBillboard(const glm::vec3& position, float size,
                              const glm::vec4& color);
    static void DrawBillboard(const glm::vec3& position, float size,
                              const Ref<Texture2D>& texture,
                              const glm::vec4& tint = glm::vec4(1.0f));
    static void DrawBillboard(const glm::vec3& position, const glm::vec2& size,
                              const Ref<Texture2D>& texture,
                              const glm::vec4& tint = glm::vec4(1.0f),
                              int entityID = -1);

    // 线段绘制
    static void DrawLine(const glm::vec3& start, const glm::vec3& end,
                         const glm::vec4& color, float thickness = 2.0f);

    // 方向箭头 (从 origin 出发，指向 direction 方向)
    static void DrawDirectionArrow(const glm::vec3& origin, const glm::vec3& direction,
                                   float length, const glm::vec4& color, float thickness = 2.0f);

    static void ResetStats();
    static Renderer2DStats GetStats();

private:
    static void StartBatch();
    static void NextBatch();
};

} // namespace GLRenderer
