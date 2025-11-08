#pragma once

#include "gl_common.h"
#include "Shader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

class Grid {
public:
    Grid() = default;

    Grid(float size, float cellSize) : gridSize(size), gridCellSize(cellSize) {
        SetupGrid();
    }

    ~Grid() {
        if (VAO != 0) glDeleteVertexArrays(1, &VAO);
        if (VBO != 0) glDeleteBuffers(1, &VBO);
    }

    void Draw(const Shader &shader, const glm::mat4& view, const glm::mat4& projection) {
        shader.use();

        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        shader.setFloat("gridSize", gridSize);
        shader.setFloat("gridCellSize", gridCellSize);
        shader.setFloat3("gridColorThin", 0.5f, 0.5f, 0.5f);
        shader.setFloat3("gridColorThick", 0.3f, 0.3f, 0.3f);
        shader.setFloat("near", 0.1f);
        shader.setFloat("far", 100.0f);

        glBindVertexArray(VAO);
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
        glDrawArrays(GL_TRIANGLES, 0, 6);

        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "OpenGL Error after DrawArrays: 0x" << std::hex << err << std::dec << std::endl;
        }

        glDisable(GL_BLEND);
        glBindVertexArray(0);
    }

    void SetGridSize(float size) { gridSize = size; }
    void SetGridCellSize(float cellSize) { gridCellSize = cellSize; }

    unsigned int VAO = 0, VBO = 0;

private:
    float gridSize;
    float gridCellSize;

    void SetupGrid() {
        float vertices[] = {
            -1.0f,  1.0f, 0.0f,
            -1.0f, -1.0f, 0.0f,
             1.0f, -1.0f, 0.0f,

            -1.0f,  1.0f, 0.0f,
             1.0f, -1.0f, 0.0f,
             1.0f,  1.0f, 0.0f
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        GLint enabled;
        glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
        std::cout << "Grid setup - Vertex Attrib 0 enabled: " << enabled << std::endl;

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        std::cout << "Grid created: VAO=" << VAO << ", VBO=" << VBO 
                  << ", size=" << gridSize << ", cellSize=" << gridCellSize << std::endl;
    }
};