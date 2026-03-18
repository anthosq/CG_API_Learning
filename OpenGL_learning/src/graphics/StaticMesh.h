#pragma once

// StaticMesh - 轻量级网格封装
//
// 用于:
// - 基础几何体 (立方体、球体、平面等)
// - 碰撞体可视化 (线框渲染)
// - 程序化生成的网格
//
// 与 Mesh 类的区别:
// - Mesh: 支持骨骼动画、多纹理、复杂材质 (Assimp 模型)
// - StaticMesh: 轻量级，固定顶点格式，无动画

#include "Buffer.h"
#include "PrimitiveData.h"
#include "core/AABB.h"
#include <memory>
#include <cstdint>

namespace GLRenderer {

// 几何体类型
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

// 绘制模式
enum class DrawMode {
    Triangles,  // GL_TRIANGLES
    Lines,      // GL_LINES (线框)
    Points      // GL_POINTS
};

class StaticMesh : public NonCopyable {
public:
    StaticMesh() = default;
    ~StaticMesh() = default;

    // 移动语义
    StaticMesh(StaticMesh&& other) noexcept;
    StaticMesh& operator=(StaticMesh&& other) noexcept;

    // 渲染
    void Draw() const;              // 使用默认模式 (Triangles 或 Lines)
    void Draw(DrawMode mode) const; // 指定绘制模式

    // 访问器
    VertexArray* GetVertexArray() const { return m_VAO.get(); }
    uint32_t GetVertexCount() const { return m_VertexCount; }
    uint32_t GetIndexCount() const { return m_IndexCount; }
    bool IsIndexed() const { return m_IndexCount > 0; }
    PrimitiveType GetType() const { return m_Type; }
    DrawMode GetDefaultDrawMode() const { return m_DefaultDrawMode; }
    const AABB& GetBoundingBox() const { return m_BoundingBox; }

    // 绑定/解绑 VAO
    void Bind() const;
    void Unbind() const;

private:
    friend class MeshFactory;

    // 从顶点数据创建 (由 MeshFactory 调用)
    void CreateFromPrimitiveVertices(
        const PrimitiveVertex* vertices, uint32_t vertexCount,
        const uint32_t* indices = nullptr, uint32_t indexCount = 0);

    void CreateFromWireframeVertices(
        const WireframeVertex* vertices, uint32_t vertexCount,
        const uint32_t* indices = nullptr, uint32_t indexCount = 0);

    // 计算包围盒
    void CalculateBoundingBox(const PrimitiveVertex* vertices, uint32_t count);
    void CalculateBoundingBox(const WireframeVertex* vertices, uint32_t count);

private:
    std::unique_ptr<VertexArray> m_VAO;
    uint32_t m_VertexCount = 0;
    uint32_t m_IndexCount = 0;
    PrimitiveType m_Type = PrimitiveType::Custom;
    DrawMode m_DefaultDrawMode = DrawMode::Triangles;
    AABB m_BoundingBox;
};

} // namespace GLRenderer
