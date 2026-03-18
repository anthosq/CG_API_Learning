#include "Renderer2D.h"
#include "RenderCommand.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace GLRenderer {

// 批处理常量
static constexpr uint32_t MaxQuads = 10000;
static constexpr uint32_t MaxVertices = MaxQuads * 4;
static constexpr uint32_t MaxIndices = MaxQuads * 6;
static constexpr uint32_t MaxTextureSlots = 16;  // OpenGL 3.3 最小保证

// Quad 顶点结构
struct QuadVertex {
    glm::vec3 Position;
    glm::vec4 Color;
    glm::vec2 TexCoord;
    float TexIndex;
    int EntityID;
};

// 渲染器内部数据
struct Renderer2DData {
    std::unique_ptr<VertexArray> QuadVAO;
    VertexBuffer* QuadVBO = nullptr;  // 由 VAO 所有，这里保存引用用于更新
    std::shared_ptr<Shader> QuadShader;
    std::unique_ptr<Texture> WhiteTexture;

    uint32_t QuadIndexCount = 0;
    QuadVertex* QuadVertexBufferBase = nullptr;
    QuadVertex* QuadVertexBufferPtr = nullptr;

    std::array<Texture*, MaxTextureSlots> TextureSlots;
    uint32_t TextureSlotIndex = 1;  // 0 = 白色纹理

    // 相机数据
    glm::mat4 ViewProjectionMatrix;
    glm::mat4 ViewMatrix;
    glm::vec3 CameraRight;
    glm::vec3 CameraUp;

    // Quad 顶点位置模板 (以中心为原点)
    glm::vec4 QuadVertexPositions[4];

    // 统计
    Renderer2DStats Stats;
};

static Renderer2DData s_Data;

void Renderer2D::Init() {
    std::cout << "[Renderer2D] Initializing..." << std::endl;

    // 创建 Quad VAO
    s_Data.QuadVAO = std::make_unique<VertexArray>();

    // 创建顶点缓冲 (动态)
    auto quadVBO = std::make_unique<VertexBuffer>(nullptr, MaxVertices * sizeof(QuadVertex), true);
    s_Data.QuadVBO = quadVBO.get();  // 保存引用用于更新

    // 设置顶点布局
    std::vector<VertexAttribute> layout = {
        VertexAttribute::Float(0, 3, sizeof(QuadVertex), offsetof(QuadVertex, Position)),
        VertexAttribute::Float(1, 4, sizeof(QuadVertex), offsetof(QuadVertex, Color)),
        VertexAttribute::Float(2, 2, sizeof(QuadVertex), offsetof(QuadVertex, TexCoord)),
        VertexAttribute::Float(3, 1, sizeof(QuadVertex), offsetof(QuadVertex, TexIndex)),
        VertexAttribute::Int(4, 1, sizeof(QuadVertex), offsetof(QuadVertex, EntityID))
    };

    s_Data.QuadVAO->AddVertexBuffer(std::move(quadVBO), layout);

    // 创建索引缓冲
    std::vector<uint32_t> indices(MaxIndices);
    uint32_t offset = 0;
    for (uint32_t i = 0; i < MaxIndices; i += 6) {
        indices[i + 0] = offset + 0;
        indices[i + 1] = offset + 1;
        indices[i + 2] = offset + 2;

        indices[i + 3] = offset + 2;
        indices[i + 4] = offset + 3;
        indices[i + 5] = offset + 0;

        offset += 4;
    }
    auto quadIBO = std::make_unique<IndexBuffer>(indices.data(), MaxIndices);
    s_Data.QuadVAO->SetIndexBuffer(std::move(quadIBO));

    // 分配 CPU 端顶点缓冲
    s_Data.QuadVertexBufferBase = new QuadVertex[MaxVertices];

    // 创建 1x1 白色纹理
    uint32_t whitePixel = 0xFFFFFFFF;
    s_Data.WhiteTexture = std::make_unique<Texture>(&whitePixel, 1, 1);

    // 初始化纹理槽位
    s_Data.TextureSlots[0] = s_Data.WhiteTexture.get();
    for (uint32_t i = 1; i < MaxTextureSlots; i++) {
        s_Data.TextureSlots[i] = nullptr;
    }

    // 加载着色器
    s_Data.QuadShader = std::make_shared<Shader>("assets/shaders/quad2d.glsl");

    // 设置纹理采样器数组
    s_Data.QuadShader->Bind();
    int samplers[MaxTextureSlots];
    for (int i = 0; i < MaxTextureSlots; i++) {
        samplers[i] = i;
    }
    s_Data.QuadShader->SetIntArray("u_Textures", samplers, MaxTextureSlots);

    // 初始化顶点位置模板
    s_Data.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
    s_Data.QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
    s_Data.QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
    s_Data.QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

    std::cout << "[Renderer2D] Initialized" << std::endl;
}

void Renderer2D::Shutdown() {
    delete[] s_Data.QuadVertexBufferBase;
    s_Data.QuadVertexBufferBase = nullptr;

    s_Data.QuadVAO.reset();  // 这会同时释放 VBO 和 IBO
    s_Data.QuadVBO = nullptr;
    s_Data.QuadShader.reset();
    s_Data.WhiteTexture.reset();

    std::cout << "[Renderer2D] Shutdown" << std::endl;
}

void Renderer2D::BeginScene(const Camera& camera, float aspectRatio) {
    s_Data.ViewProjectionMatrix = camera.GetProjectionMatrix(aspectRatio) * camera.GetViewMatrix();
    s_Data.ViewMatrix = camera.GetViewMatrix();

    // 提取相机右向量和上向量 (用于 Billboard)
    s_Data.CameraRight = { s_Data.ViewMatrix[0][0], s_Data.ViewMatrix[1][0], s_Data.ViewMatrix[2][0] };
    s_Data.CameraUp = { s_Data.ViewMatrix[0][1], s_Data.ViewMatrix[1][1], s_Data.ViewMatrix[2][1] };

    StartBatch();
}

void Renderer2D::BeginScene(const glm::mat4& viewProjection) {
    s_Data.ViewProjectionMatrix = viewProjection;

    // 没有单独的视图矩阵时，使用默认的上/右向量
    s_Data.CameraRight = { 1.0f, 0.0f, 0.0f };
    s_Data.CameraUp = { 0.0f, 1.0f, 0.0f };

    StartBatch();
}

void Renderer2D::EndScene() {
    Flush();
}

void Renderer2D::StartBatch() {
    s_Data.QuadIndexCount = 0;
    s_Data.QuadVertexBufferPtr = s_Data.QuadVertexBufferBase;
    s_Data.TextureSlotIndex = 1;

    // 清理上一批次的纹理槽位（保留槽位 0 的白色纹理）
    for (uint32_t i = 1; i < MaxTextureSlots; i++) {
        s_Data.TextureSlots[i] = nullptr;
    }
}

void Renderer2D::NextBatch() {
    Flush();
    StartBatch();
}

void Renderer2D::Flush() {
    if (s_Data.QuadIndexCount == 0)
        return;

    uint32_t dataSize = static_cast<uint32_t>(
        reinterpret_cast<uint8_t*>(s_Data.QuadVertexBufferPtr) -
        reinterpret_cast<uint8_t*>(s_Data.QuadVertexBufferBase)
    );

    // 先绑定 VAO，确保后续操作都在正确的上下文中
    s_Data.QuadVAO->Bind();

    // 更新顶点数据
    s_Data.QuadVBO->SetSubData(s_Data.QuadVertexBufferBase, dataSize, 0);

    // 设置渲染状态
    RenderCommand::EnableDepthTest();
    RenderCommand::SetDepthMask(false);
    RenderCommand::EnableBlending();
    RenderCommand::SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    // 绑定着色器
    s_Data.QuadShader->Bind();
    s_Data.QuadShader->SetMat4("u_ViewProjection", s_Data.ViewProjectionMatrix);

    // 重新设置纹理采样器 uniform（确保每次渲染时都正确）
    int samplers[MaxTextureSlots];
    for (int i = 0; i < static_cast<int>(MaxTextureSlots); i++) {
        samplers[i] = i;
    }
    s_Data.QuadShader->SetIntArray("u_Textures", samplers, MaxTextureSlots);

    // 绑定所有使用的纹理（确保正确激活纹理单元）
    for (uint32_t i = 0; i < s_Data.TextureSlotIndex; i++) {
        if (s_Data.TextureSlots[i]) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, s_Data.TextureSlots[i]->GetID());
        }
    }
    // 恢复活动纹理单元为 0
    glActiveTexture(GL_TEXTURE0);

    RenderCommand::DrawIndexed(*s_Data.QuadVAO, s_Data.QuadIndexCount);

    // 恢复状态
    RenderCommand::SetDepthMask(true);
    RenderCommand::DisableBlending();
    // glEnable(GL_CULL_FACE);

    s_Data.Stats.DrawCalls++;
}

// === 绘制 API ===

void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size,
                          const glm::vec4& color) {
    DrawQuad({ position.x, position.y, 0.0f }, size, color);
}

void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size,
                          const glm::vec4& color) {
    if (s_Data.QuadIndexCount >= MaxIndices) {
        NextBatch();
    }

    const float texIndex = 0.0f;  // 白色纹理
    constexpr glm::vec2 texCoords[] = {
        { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }
    };

    glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
                        * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

    for (int i = 0; i < 4; i++) {
        s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
        s_Data.QuadVertexBufferPtr->Color = color;
        s_Data.QuadVertexBufferPtr->TexCoord = texCoords[i];
        s_Data.QuadVertexBufferPtr->TexIndex = texIndex;
        s_Data.QuadVertexBufferPtr->EntityID = -1;
        s_Data.QuadVertexBufferPtr++;
    }

    s_Data.QuadIndexCount += 6;
    s_Data.Stats.QuadCount++;
}

void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size,
                          Texture* texture, const glm::vec4& tint) {
    DrawQuad({ position.x, position.y, 0.0f }, size, texture, tint);
}

void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size,
                          Texture* texture, const glm::vec4& tint) {
    if (s_Data.QuadIndexCount >= MaxIndices) {
        NextBatch();
    }

    // 查找或添加纹理槽位
    float texIndex = 0.0f;
    if (texture) {
        for (uint32_t i = 1; i < s_Data.TextureSlotIndex; i++) {
            if (s_Data.TextureSlots[i] == texture) {
                texIndex = static_cast<float>(i);
                break;
            }
        }

        if (texIndex == 0.0f) {
            if (s_Data.TextureSlotIndex >= MaxTextureSlots) {
                NextBatch();
            }
            texIndex = static_cast<float>(s_Data.TextureSlotIndex);
            s_Data.TextureSlots[s_Data.TextureSlotIndex] = texture;
            s_Data.TextureSlotIndex++;
        }
    }

    constexpr glm::vec2 texCoords[] = {
        { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }
    };

    glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
                        * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

    for (int i = 0; i < 4; i++) {
        s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
        s_Data.QuadVertexBufferPtr->Color = tint;
        s_Data.QuadVertexBufferPtr->TexCoord = texCoords[i];
        s_Data.QuadVertexBufferPtr->TexIndex = texIndex;
        s_Data.QuadVertexBufferPtr->EntityID = -1;
        s_Data.QuadVertexBufferPtr++;
    }

    s_Data.QuadIndexCount += 6;
    s_Data.Stats.QuadCount++;
}

void Renderer2D::DrawRotatedQuad(const glm::vec2& position, const glm::vec2& size,
                                 float rotation, const glm::vec4& color) {
    DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
}

void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size,
                                 float rotation, const glm::vec4& color) {
    if (s_Data.QuadIndexCount >= MaxIndices) {
        NextBatch();
    }

    const float texIndex = 0.0f;
    constexpr glm::vec2 texCoords[] = {
        { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }
    };

    glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
                        * glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
                        * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

    for (int i = 0; i < 4; i++) {
        s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
        s_Data.QuadVertexBufferPtr->Color = color;
        s_Data.QuadVertexBufferPtr->TexCoord = texCoords[i];
        s_Data.QuadVertexBufferPtr->TexIndex = texIndex;
        s_Data.QuadVertexBufferPtr->EntityID = -1;
        s_Data.QuadVertexBufferPtr++;
    }

    s_Data.QuadIndexCount += 6;
    s_Data.Stats.QuadCount++;
}

void Renderer2D::DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size,
                                 float rotation, Texture* texture, const glm::vec4& tint) {
    if (s_Data.QuadIndexCount >= MaxIndices) {
        NextBatch();
    }

    // 查找或添加纹理槽位
    float texIndex = 0.0f;
    if (texture) {
        for (uint32_t i = 1; i < s_Data.TextureSlotIndex; i++) {
            if (s_Data.TextureSlots[i] == texture) {
                texIndex = static_cast<float>(i);
                break;
            }
        }

        if (texIndex == 0.0f) {
            if (s_Data.TextureSlotIndex >= MaxTextureSlots) {
                NextBatch();
            }
            texIndex = static_cast<float>(s_Data.TextureSlotIndex);
            s_Data.TextureSlots[s_Data.TextureSlotIndex] = texture;
            s_Data.TextureSlotIndex++;
        }
    }

    constexpr glm::vec2 texCoords[] = {
        { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }
    };

    glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
                        * glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
                        * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

    for (int i = 0; i < 4; i++) {
        s_Data.QuadVertexBufferPtr->Position = transform * s_Data.QuadVertexPositions[i];
        s_Data.QuadVertexBufferPtr->Color = tint;
        s_Data.QuadVertexBufferPtr->TexCoord = texCoords[i];
        s_Data.QuadVertexBufferPtr->TexIndex = texIndex;
        s_Data.QuadVertexBufferPtr->EntityID = -1;
        s_Data.QuadVertexBufferPtr++;
    }

    s_Data.QuadIndexCount += 6;
    s_Data.Stats.QuadCount++;
}

// === Billboard ===

void Renderer2D::DrawBillboard(const glm::vec3& position, float size,
                               const glm::vec4& color) {
    DrawBillboard(position, { size, size }, nullptr, color, -1);
}

void Renderer2D::DrawBillboard(const glm::vec3& position, float size,
                               Texture* texture, const glm::vec4& tint) {
    DrawBillboard(position, { size, size }, texture, tint, -1);
}

void Renderer2D::DrawBillboard(const glm::vec3& position, const glm::vec2& size,
                               Texture* texture, const glm::vec4& tint, int entityID) {
    if (s_Data.QuadIndexCount >= MaxIndices) {
        NextBatch();
    }

    // 查找或添加纹理槽位
    float texIndex = 0.0f;
    if (texture) {
        for (uint32_t i = 1; i < s_Data.TextureSlotIndex; i++) {
            if (s_Data.TextureSlots[i] == texture) {
                texIndex = static_cast<float>(i);
                break;
            }
        }

        if (texIndex == 0.0f) {
            if (s_Data.TextureSlotIndex >= MaxTextureSlots) {
                NextBatch();
            }
            texIndex = static_cast<float>(s_Data.TextureSlotIndex);
            s_Data.TextureSlots[s_Data.TextureSlotIndex] = texture;
            s_Data.TextureSlotIndex++;
        }
    }

    // Billboard 顶点：使用相机的右/上向量
    glm::vec3 right = s_Data.CameraRight * size.x * 0.5f;
    glm::vec3 up = s_Data.CameraUp * size.y * 0.5f;

    glm::vec3 positions[4] = {
        position - right - up,  // 左下
        position + right - up,  // 右下
        position + right + up,  // 右上
        position - right + up   // 左上
    };

    constexpr glm::vec2 texCoords[] = {
        { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }
    };

    for (int i = 0; i < 4; i++) {
        s_Data.QuadVertexBufferPtr->Position = positions[i];
        s_Data.QuadVertexBufferPtr->Color = tint;
        s_Data.QuadVertexBufferPtr->TexCoord = texCoords[i];
        s_Data.QuadVertexBufferPtr->TexIndex = texIndex;
        s_Data.QuadVertexBufferPtr->EntityID = entityID;
        s_Data.QuadVertexBufferPtr++;
    }

    s_Data.QuadIndexCount += 6;
    s_Data.Stats.QuadCount++;
}

// === 统计 ===

void Renderer2D::ResetStats() {
    s_Data.Stats.DrawCalls = 0;
    s_Data.Stats.QuadCount = 0;
}

Renderer2DStats Renderer2D::GetStats() {
    return s_Data.Stats;
}

} // namespace GLRenderer
