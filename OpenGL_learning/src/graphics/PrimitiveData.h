#pragma once

// PrimitiveData - 基础几何体顶点数据和格式定义
//
// 提供:
// - 轻量级顶点结构 (PrimitiveVertex, WireframeVertex)
// - 预定义的几何体数据 (立方体、四边形等)
// - 顶点布局辅助函数

#include "Buffer.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace GLRenderer {

// 基础几何体顶点 (32 bytes)
// 比 MeshVertex (112 bytes) 更轻量，适合基础几何体
struct PrimitiveVertex {
    glm::vec3 Position;   // 12 bytes, offset 0
    glm::vec3 Normal;     // 12 bytes, offset 12
    glm::vec2 TexCoords;  // 8 bytes,  offset 24
    // Total: 32 bytes

    PrimitiveVertex() = default;
    PrimitiveVertex(const glm::vec3& pos, const glm::vec3& normal, const glm::vec2& uv)
        : Position(pos), Normal(normal), TexCoords(uv) {}

    // 获取顶点属性布局
    static std::vector<VertexAttribute> GetLayout() {
        return {
            VertexAttribute::Float(0, 3, sizeof(PrimitiveVertex), offsetof(PrimitiveVertex, Position)),
            VertexAttribute::Float(1, 3, sizeof(PrimitiveVertex), offsetof(PrimitiveVertex, Normal)),
            VertexAttribute::Float(2, 2, sizeof(PrimitiveVertex), offsetof(PrimitiveVertex, TexCoords))
        };
    }
};

// 线框顶点 (12 bytes) - 仅位置，用于调试/碰撞可视化
struct WireframeVertex {
    glm::vec3 Position;  // 12 bytes

    WireframeVertex() = default;
    WireframeVertex(const glm::vec3& pos) : Position(pos) {}

    // 获取顶点属性布局
    static std::vector<VertexAttribute> GetLayout() {
        return {
            VertexAttribute::Float(0, 3, sizeof(WireframeVertex), 0)
        };
    }
};

namespace PrimitiveData {

// --- 立方体 (非索引，36 顶点) ---
// 位于原点，尺寸 1x1x1 (-0.5 到 0.5)
extern const PrimitiveVertex CubeVertices[36];
constexpr uint32_t CubeVertexCount = 36;

// --- 立方体 (索引化，8 顶点 + 36 索引) ---
extern const PrimitiveVertex CubeVerticesIndexed[24];  // 每个面 4 顶点，共 24 顶点（有法线）
extern const uint32_t CubeIndices[36];
constexpr uint32_t CubeIndexedVertexCount = 24;
constexpr uint32_t CubeIndexCount = 36;

// --- 立方体线框 (12 边 = 24 线段索引) ---
extern const WireframeVertex CubeWireframeVertices[8];
extern const uint32_t CubeWireframeIndices[24];
constexpr uint32_t CubeWireframeVertexCount = 8;
constexpr uint32_t CubeWireframeIndexCount = 24;

// --- 四边形 (平面，4 顶点 + 6 索引) ---
// XZ 平面，法线朝上 (+Y)
extern const PrimitiveVertex QuadVertices[4];
extern const uint32_t QuadIndices[6];
constexpr uint32_t QuadVertexCount = 4;
constexpr uint32_t QuadIndexCount = 6;

} // namespace PrimitiveData

} // namespace GLRenderer
