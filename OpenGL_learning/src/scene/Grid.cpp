#include "Grid.h"

namespace GLRenderer {

Grid::Grid(float size, float cellSize) {
    m_Settings.Size = size;
    m_Settings.CellSize = cellSize;
    SetupGrid();
}

Grid::Grid(const GridSettings& settings)
    : m_Settings(settings) {
    SetupGrid();
}

void Grid::Draw(const Shader& shader,
                const glm::mat4& view,
                const glm::mat4& projection,
                float nearPlane,
                float farPlane) {
    shader.Bind();

    const_cast<Shader&>(shader).SetMat4("view", view);
    const_cast<Shader&>(shader).SetMat4("projection", projection);
    const_cast<Shader&>(shader).SetFloat("gridSize", m_Settings.Size);
    const_cast<Shader&>(shader).SetFloat("gridCellSize", m_Settings.CellSize);
    const_cast<Shader&>(shader).SetVec3("gridColorThin", m_Settings.ThinColor);
    const_cast<Shader&>(shader).SetFloat("near", nearPlane);
    const_cast<Shader&>(shader).SetFloat("far", farPlane);

    // 注: draw buffer 重定向已在 SceneRenderer::GridPass() 中完成 (排除整型附件)
    // 这里只需标准混合即可
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (m_VAO) {
        m_VAO->Bind();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        m_VAO->Unbind();
    } else if (VAO != 0) {
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    glDisable(GL_BLEND);
    GL_CHECK_ERROR();
}

void Grid::SetupGrid() {
    float vertices[] = {
        -1.0f,  1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,

        -1.0f,  1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f
    };

    m_VAO = VertexArray::Create();

    auto vbo = VertexBuffer::Create(vertices, sizeof(vertices));

    std::vector<VertexAttribute> layout = {
        VertexAttribute::Float(0, 3, 3 * sizeof(float), 0)
    };

    m_VAO->AddVertexBuffer(vbo, layout);

    VAO = m_VAO->GetID();

    std::cout << "[Grid] 创建完成: size=" << m_Settings.Size
              << ", cellSize=" << m_Settings.CellSize << std::endl;
}

Ref<Grid> Grid::Create(float size, float cellSize) {
    return Ref<Grid>(new Grid(size, cellSize));
}

Ref<Grid> Grid::Create(const GridSettings& settings) {
    return Ref<Grid>(new Grid(settings));
}

} // namespace GLRenderer
