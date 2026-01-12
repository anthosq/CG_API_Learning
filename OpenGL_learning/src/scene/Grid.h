#pragma once

#include "utils/GLCommon.h"
#include "graphics/Shader.h"
#include "graphics/Buffer.h"
#include <memory>

namespace GLRenderer {

// ============================================================================
// GridSettings - 网格配置
// ============================================================================
struct GridSettings {
    float Size = 100.0f;           // 网格总大小
    float CellSize = 1.0f;         // 单元格大小
    glm::vec3 ThinColor = glm::vec3(0.5f);   // 细线颜色
    glm::vec3 ThickColor = glm::vec3(0.3f);  // 粗线颜色（主要网格线）
};

// ============================================================================
// Grid - 无限网格渲染
// ============================================================================
class Grid : public NonCopyable {
public:
    Grid() = default;
    Grid(float size, float cellSize);
    Grid(const GridSettings& settings);
    ~Grid() = default;  // RAII 由成员管理

    // 移动语义
    Grid(Grid&& other) noexcept = default;
    Grid& operator=(Grid&& other) noexcept = default;

    // 绘制网格
    // near/far: 相机的近/远平面，用于计算网格淡出
    void Draw(const Shader& shader,
              const glm::mat4& view,
              const glm::mat4& projection,
              float nearPlane = 0.1f,
              float farPlane = 100.0f);

    // 设置属性
    void SetGridSize(float size) { m_Settings.Size = size; }
    void SetGridCellSize(float cellSize) { m_Settings.CellSize = cellSize; }
    void SetThinColor(const glm::vec3& color) { m_Settings.ThinColor = color; }
    void SetThickColor(const glm::vec3& color) { m_Settings.ThickColor = color; }
    void SetSettings(const GridSettings& settings) { m_Settings = settings; }

    // 获取属性
    float GetGridSize() const { return m_Settings.Size; }
    float GetGridCellSize() const { return m_Settings.CellSize; }
    const GridSettings& GetSettings() const { return m_Settings; }

    // 兼容旧代码的 VAO（不推荐直接访问）
    unsigned int VAO = 0, VBO = 0;

private:
    void SetupGrid();

    GridSettings m_Settings;
    std::unique_ptr<VertexArray> m_VAO;
};

} // namespace GLRenderer

// 全局命名空间别名
using GLRenderer::Grid;
using GLRenderer::GridSettings;
