#include "StaticMesh.h"
#include "utils/GLCommon.h"

namespace GLRenderer {

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

Ref<StaticMesh> StaticMesh::Create() {
    return Ref<StaticMesh>(new StaticMesh());
}

void StaticMesh::CreateFromPrimitiveVertices(
    const PrimitiveVertex* vertices, uint32_t vertexCount,
    const uint32_t* indices, uint32_t indexCount)
{
    m_VertexCount = vertexCount;
    m_IndexCount = indexCount;
    m_DefaultDrawMode = DrawMode::Triangles;

    m_VAO = VertexArray::Create();
    m_VAO->Bind();

    auto vbo = VertexBuffer::Create(
        vertices,
        vertexCount * sizeof(PrimitiveVertex)
    );

    m_VAO->AddVertexBuffer(vbo, PrimitiveVertex::GetLayout());

    if (indices && indexCount > 0) {
        auto ibo = IndexBuffer::Create(indices, indexCount);
        m_VAO->SetIndexBuffer(ibo);
    }

    m_VAO->Unbind();

    CalculateBoundingBox(vertices, vertexCount);
}

void StaticMesh::CreateFromWireframeVertices(
    const WireframeVertex* vertices, uint32_t vertexCount,
    const uint32_t* indices, uint32_t indexCount)
{
    m_VertexCount = vertexCount;
    m_IndexCount = indexCount;
    m_DefaultDrawMode = DrawMode::Lines;

    m_VAO = VertexArray::Create();
    m_VAO->Bind();

    auto vbo = VertexBuffer::Create(
        vertices,
        vertexCount * sizeof(WireframeVertex)
    );

    m_VAO->AddVertexBuffer(vbo, WireframeVertex::GetLayout());

    if (indices && indexCount > 0) {
        auto ibo = IndexBuffer::Create(indices, indexCount);
        m_VAO->SetIndexBuffer(ibo);
    }

    m_VAO->Unbind();

    CalculateBoundingBox(vertices, vertexCount);
}

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
