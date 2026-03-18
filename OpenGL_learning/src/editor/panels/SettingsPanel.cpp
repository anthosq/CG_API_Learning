#include "SettingsPanel.h"
#include "editor/EditorContext.h"
#include <imgui.h>

namespace GLRenderer {

SettingsPanel::SettingsPanel()
    : Panel("Settings")
{
}

void SettingsPanel::OnDraw(EditorContext& context) {
    if (!m_SceneRenderer) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "No SceneRenderer set");
        return;
    }

    auto& settings = m_SceneRenderer->GetSettings();
    auto& environment = m_SceneRenderer->GetEnvironment();

    // Grid 设置
    if (ImGui::CollapsingHeader("Grid", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Grid", &settings.ShowGrid);

        if (settings.ShowGrid) {
            ImGui::Indent();

            bool gridChanged = false;

            gridChanged |= ImGui::DragFloat("Grid Size", &settings.GridSize, 1.0f, 10.0f, 1000.0f, "%.0f");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Total size of the grid");
            }

            gridChanged |= ImGui::DragFloat("Cell Size", &settings.GridCellSize, 0.1f, 0.1f, 10.0f, "%.1f");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Size of each grid cell");
            }

            // 如果网格设置改变，需要重建网格
            if (gridChanged) {
                // Grid 会在下一帧自动重建（如果 SceneRenderer 支持的话）
            }

            ImGui::Unindent();
        }
    }

    // Skybox 设置
    if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Skybox", &settings.ShowSkybox);
    }

    // 光照设置
    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 方向光
        ImGui::Text("Directional Light");
        ImGui::Indent();

        ImGui::DragFloat3("Direction", &environment.DirLight.Direction[0], 0.01f, -1.0f, 1.0f);
        ImGui::ColorEdit3("Ambient", &environment.DirLight.Ambient[0]);
        ImGui::ColorEdit3("Diffuse", &environment.DirLight.Diffuse[0]);
        ImGui::ColorEdit3("Specular", &environment.DirLight.Specular[0]);

        ImGui::Unindent();

        ImGui::Separator();

        // 点光源计数
        ImGui::Text("Point Lights: %d", environment.PointLightCount);
    }

    // 渲染设置
    if (ImGui::CollapsingHeader("Rendering")) {
        ImGui::Checkbox("Wireframe", &settings.Wireframe);
        ImGui::Checkbox("Enable Shadows", &settings.EnableShadows);
        ImGui::ColorEdit3("Ambient Color", &settings.AmbientColor[0]);
    }
}

} // namespace GLRenderer
