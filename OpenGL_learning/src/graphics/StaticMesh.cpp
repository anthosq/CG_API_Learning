#include "StaticMesh.h"
#include "utils/GLCommon.h"

namespace GLRenderer {

// ===========================================================================
// 移动语义
// ===========================================================================

StaticMesh::StaticMesh(StaticMesh&& other) noexcept
    : m_VAO(std::move(other.m_VAO))
    , m_VertexCount(other.m_VertexCount)
    , m_IndexCount(other.m_IndexCount)
    , m_Type(other.m_Type)
    , m_DefaultDrawMode(other.m_DefaultDrawMode)
    , m_BoundingBox(other.m_BoundingBox)
{
    other.m_VertexCount = 0;
    other.m_IndexCount = 0;
}

StaticMesh& StaticMesh::operator=(StaticMesh&& other) noexcept {
    if (this != &other) {
        m_VAO = std::move(other.m_VAO);
        m_VertexCount = other.m_VertexCount;
        m_IndexCount = other.m_IndexCount;
        m_Type = other.m_Type;
        m_DefaultDrawMode = other.m_DefaultDrawMode;
        m_BoundingBox = other.m_BoundingBox;

        other.m_VertexCount = 0;
        other.m_IndexCount = 0;
    }
    return *this;
}

// ===========================================================================
// 渲染
// ===========================================================================

void StaticMesh::Draw() const {
    Draw(m_DefaultDrawMode);
}

void StaticMesh::Draw(DrawMode mode) const {
    if (!m_VAO) return;

    m_VAO->Bind();

    GLenum glMode;
    switch (mode) {
        case DrawMode::Triangles: glMode = GL_TRIANGLES; break;
        case DrawMode::Lines:     glMode = GL_LINES;     break;
        case DrawMode::Points:    glMode = GL_POINTS;    break;
        default:                  glMode = GL_TRIANGLES; break;
    }

    if (IsIndexed()) {
        glDrawElements(glMode, static_cast<GLsizei>(m_IndexCount), GL_UNSIGNED_INT, nullptr);
    } else {
        glDrawArrays(glMode, 0, static_cast<GLsizei>(m_VertexCount));
    }

    m_VAO->Unbind();
}

void StaticMesh::Bind() const {
    if (m_VAO) {
        m_VAO->Bind();
    }
}

void StaticMesh::Unbind() const {
    if (m_VAO) {
        m_VAO->Unbind();
    }
}

// ===========================================================================
// 创建方法 (由 MeshFactory 调用)
// ===========================================================================

void StaticMesh::CreateFromPrimitiveVertices(
    const PrimitiveVertex* vertices, uint32_t vertexCount,
    const uint32_t* indices, uint32_t indexCount)
{
    m_VertexCount = vertexCount;
    m_IndexCount = indexCount;
    m_DefaultDrawMode = DrawMode::Triangles;

    // 创建 VAO
    m_VAO = std::make_unique<VertexArray>();
    m_VAO->Bind();

    // 创建 VBO
    auto vbo = std::make_unique<VertexBuffer>(
        vertices,
        vertexCount * sizeof(PrimitiveVertex)
    );

    // 配置顶点属性
    m_VAO->AddVertexBuffer(std::move(vbo), PrimitiveVertex::GetLayout());

    // 创建 EBO (如果有索引)
    if (indices && indexCount > 0) {
        auto ibo = std::make_unique<IndexBuffer>(indices, indexCount);
        m_VAO->SetIndexBuffer(std::move(ibo));
    }

    m_VAO->Unbind();

    // 计算包围盒
    CalculateBoundingBox(vertices, vertexCount);
}

void StaticMesh::CreateFromWireframeVertices(
    const WireframeVertex* vertices, uint32_t vertexCount,
    const uint32_t* indices, uint32_t indexCount)
{
    m_VertexCount = vertexCount;
    m_IndexCount = indexCount;
    m_DefaultDrawMode = DrawMode::Lines;

    // 创建 VAO
    m_VAO = std::make_unique<VertexArray>();
    m_VAO->Bind();

    // 创建 VBO
    auto vbo = std::make_unique<VertexBuffer>(
        vertices,
        vertexCount * sizeof(WireframeVertex)
    );

    // 配置顶点属性
    m_VAO->AddVertexBuffer(std::move(vbo), WireframeVertex::GetLayout());

    // 创建 EBO (如果有索引)
    if (indices && indexCount > 0) {
        auto ibo = std::make_unique<IndexBuffer>(indices, indexCount);
        m_VAO->SetIndexBuffer(std::move(ibo));
    }

    m_VAO->Unbind();

    // 计算包围盒
    CalculateBoundingBox(vertices, vertexCount);
}

// ===========================================================================
// 包围盒计算
// ===========================================================================

void StaticMesh::CalculateBoundingBox(const PrimitiveVertex* vertices, uint32_t count) {
    m_BoundingBox.Reset();
    for (uint32_t i = 0; i < count; ++i) {
        m_BoundingBox.Encapsulate(vertices[i].Position);
    }
}

void StaticMesh::CalculateBoundingBox(const WireframeVertex* vertices, uint32_t count) {
    m_BoundingBox.Reset();
    for (uint32_t i = 0; i < count; ++i) {
        m_BoundingBox.Encapsulate(vertices[i].Position);
    }
}

} // namespace GLRenderer
