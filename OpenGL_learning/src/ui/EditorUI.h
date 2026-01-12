#pragma once

#include "utils/GLCommon.h"
#include "graphics/Camera.h"
#include <imgui.h>

namespace GLRenderer {

// ============================================================================
// EditorSettings - 编辑器设置
// ============================================================================
struct EditorSettings {
    // 网格设置
    bool ShowGrid = true;
    float GridSize = 100.0f;
    float GridCellSize = 1.0f;

    // 光照设置
    bool AutoRotateLight = false;
    float LightRotationSpeed = 1.0f;
    float LightRotationRadius = 2.0f;
    glm::vec3 LightPosition = glm::vec3(1.2f, 1.0f, 2.0f);
    glm::vec3 LightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 LightAmbient = glm::vec3(0.2f, 0.2f, 0.2f);
    glm::vec3 LightDiffuse = glm::vec3(0.5f, 0.5f, 0.5f);
    glm::vec3 LightSpecular = glm::vec3(1.0f, 1.0f, 1.0f);

    // 材质设置
    float MaterialShininess = 32.0f;

    // 相机控制
    bool CameraControlEnabled = false;

    // 调试选项
    bool DebugDepth = false;
    bool ShowStats = true;
};

// ============================================================================
// EditorUI - 编辑器 UI 面板
// ============================================================================
class EditorUI {
public:
    EditorUI() = default;

    // 渲染所有 UI 面板
    void OnImGuiRender(const Camera& camera, EditorSettings& settings, float fps);

private:
    // 各个面板
    void DrawStatsPanel(float fps);
    void DrawCameraPanel(const Camera& camera, EditorSettings& settings);
    void DrawLightPanel(EditorSettings& settings);
    void DrawGridPanel(EditorSettings& settings);
    void DrawDebugPanel(EditorSettings& settings);
};

} // namespace GLRenderer

using GLRenderer::EditorUI;
using GLRenderer::EditorSettings;
