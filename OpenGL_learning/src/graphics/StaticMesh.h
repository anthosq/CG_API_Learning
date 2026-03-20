#pragma once

#include "Buffer.h"
#include "PrimitiveData.h"
#include "asset/Asset.h"
#include "core/AABB.h"
#include "core/Ref.h"
#include <cstdint>

namespace GLRenderer {

enum class PrimitiveType {
    Cube,
    Sphere,
    Plane,
    Cylinder,
    Capsule,
    Cone,
    Quad,
    Custom
};

enum class DrawMode {
    Triangles,
    Lines,
    Points
};

class StaticMesh : public Asset {
public:
    StaticMesh() = default;
    ~StaticMesh() = default;

    void Draw() const;
    void Draw(DrawMode mode) const;

    Ref<VertexArray> GetVertexArray() const { return m_VAO; }
    uint32_t GetVertexCount() const { return m_VertexCount; }
    uint32_t GetIndexCount() const { return m_IndexCount; }
    bool IsIndexed() const { return m_IndexCount > 0; }
    PrimitiveType GetType() const { return m_Type; }
    DrawMode GetDefaultDrawMode() const { return m_DefaultDrawMode; }
    const AABB& GetBoundingBox() const { return m_BoundingBox; }

    void Bind() const;
    void Unbind() const;

    static Ref<StaticMesh> Create();

private:
    friend class MeshFactory;

    void CreateFromPrimitiveVertices(
        const PrimitiveVertex* vertices, uint32_t vertexCount,
        const uint32_t* indices = nullptr, uint32_t indexCount = 0);

    void CreateFromWireframeVertices(
        const WireframeVertex* vertices, uint32_t vertexCount,
        const uint32_t* indices = nullptr, uint32_t indexCount = 0);

    void CalculateBoundingBox(const PrimitiveVertex* vertices, uint32_t count);
    void CalculateBoundingBox(const WireframeVertex* vertices, uint32_t count);

private:
    Ref<VertexArray> m_VAO;
    uint32_t m_VertexCount = 0;
    uint32_t m_IndexCount = 0;
    PrimitiveType m_Type = PrimitiveType::Custom;
    DrawMode m_DefaultDrawMode = DrawMode::Triangles;
    AABB m_BoundingBox;
};

} // namespace GLRenderer
