#pragma once

#include "utils/GLCommon.h"
#include "graphics/Shader.h"
#include "graphics/Buffer.h"
#include "core/Ref.h"

namespace GLRenderer {

struct GridSettings {
    float Size = 100.0f;
    float CellSize = 1.0f;
    glm::vec3 ThinColor = glm::vec3(0.5f);
    glm::vec3 ThickColor = glm::vec3(0.3f);
};

class Grid : public RefCounter {
public:
    Grid() = default;
    Grid(float size, float cellSize);
    Grid(const GridSettings& settings);
    ~Grid() = default;

    void Draw(const Shader& shader,
              const glm::mat4& view,
              const glm::mat4& projection,
              float nearPlane = 0.1f,
              float farPlane = 100.0f);

    void SetGridSize(float size) { m_Settings.Size = size; }
    void SetGridCellSize(float cellSize) { m_Settings.CellSize = cellSize; }
    void SetThinColor(const glm::vec3& color) { m_Settings.ThinColor = color; }
    void SetThickColor(const glm::vec3& color) { m_Settings.ThickColor = color; }
    void SetSettings(const GridSettings& settings) { m_Settings = settings; }

    float GetGridSize() const { return m_Settings.Size; }
    float GetGridCellSize() const { return m_Settings.CellSize; }
    const GridSettings& GetSettings() const { return m_Settings; }

    unsigned int VAO = 0, VBO = 0;

    static Ref<Grid> Create(float size = 100.0f, float cellSize = 1.0f);
    static Ref<Grid> Create(const GridSettings& settings);

private:
    void SetupGrid();

    GridSettings m_Settings;
    Ref<VertexArray> m_VAO;
};

} // namespace GLRenderer

using GLRenderer::Grid;
using GLRenderer::GridSettings;
