#include "SettingsPanel.h"
#include "editor/EditorContext.h"
#include "scene/ecs/Components.h"
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
            ImGui::DragFloat("Grid Size", &settings.GridSize, 1.0f, 10.0f, 1000.0f, "%.0f");
            ImGui::DragFloat("Cell Size", &settings.GridCellSize, 0.1f, 0.1f, 10.0f, "%.1f");
            ImGui::Unindent();
        }
    }

    // Skybox 设置
    if (ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Skybox", &settings.ShowSkybox);
    }

    // 光照信息
    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 统计场景中的光源
        int dirLightCount = 0;
        ECS::Entity firstDirLight;

        if (context.World) {
            context.World->Each<ECS::DirectionalLightComponent>(
                [&](ECS::Entity entity, ECS::DirectionalLightComponent& light) {
                    if (dirLightCount == 0) {
                        firstDirLight = entity;
                    }
                    dirLightCount++;
                }
            );
        }

        ImGui::Text("Directional Lights: %d", dirLightCount);
        ImGui::Text("Point Lights: %d", environment.PointLightCount);

        ImGui::Spacing();

        if (dirLightCount == 0) {
            if (ImGui::Button("Create Directional Light") && context.World) {
                ECS::Entity newLight = context.World->CreateEntity("Directional Light");
                auto& transform = newLight.AddComponent<ECS::TransformComponent>();
                transform.Rotation = glm::radians(glm::vec3(-45.0f, -30.0f, 0.0f));
                auto& light = newLight.AddComponent<ECS::DirectionalLightComponent>();
                light.Color = glm::vec3(1.0f);
                light.Intensity = 1.0f;
                context.Select(newLight);
            }
        } else if (firstDirLight.IsValid()) {
            if (ImGui::Button("Select Directional Light")) {
                context.Select(firstDirLight);
            }
        }

        ImGui::Separator();
        ImGui::ColorEdit3("Ambient Color", &settings.AmbientColor[0]);
    }

    // 渲染设置
    if (ImGui::CollapsingHeader("Rendering")) {
        ImGui::Checkbox("Wireframe", &settings.Wireframe);
        ImGui::Checkbox("Enable Shadows", &settings.EnableShadows);
    }
}

} // namespace GLRenderer
