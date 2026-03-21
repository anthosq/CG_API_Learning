#include "SettingsPanel.h"
#include "editor/EditorContext.h"
#include "scene/ecs/Components.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>

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

    // 环境/IBL 设置
    if (ImGui::CollapsingHeader("Environment (IBL)", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool hasIBL = environment.IBLEnvironment && environment.IBLEnvironment->IsValid();

        // 状态指示
        if (hasIBL) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "IBL: Active");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "IBL: No environment loaded");
        }

        // 拖放区域: 将 .hdr 文件从资产浏览器拖到此处即可加载
        ImGui::PushStyleColor(ImGuiCol_Button,
            hasIBL ? ImVec4(0.15f, 0.35f, 0.15f, 1.0f) : ImVec4(0.25f, 0.2f, 0.1f, 1.0f));
        ImGui::Button("[ Drop .hdr here ]##ibl_drop", ImVec2(-1, 32));
        ImGui::PopStyleColor();
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                const char* path = static_cast<const char*>(payload->Data);
                std::filesystem::path assetPath(path);
                std::string ext = assetPath.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".hdr" || ext == ".exr") {
                    m_SceneRenderer->LoadEnvironment(assetPath);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // 加载 HDR 按钮
        if (ImGui::Button("Load HDR Environment...")) {
            ImGui::OpenPopup("##hdr_path_input");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Default")) {
            const char* defaultHDRs[] = {
                "assets/EnvironmentMaps/puresky_4k.hdr",
                "assets/EnvironmentMaps/meadow_2_2k.hdr",
            };
            bool loaded = false;
            for (const char* p : defaultHDRs) {
                if (std::filesystem::exists(p)) {
                    m_SceneRenderer->LoadEnvironment(p);
                    loaded = true;
                    break;
                }
            }
            if (!loaded) ImGui::OpenPopup("##no_default_hdr");
        }

        // HDR 路径输入弹窗
        if (ImGui::BeginPopup("##hdr_path_input")) {
            static char pathBuf[512] = "assets/EnvironmentMaps/";
            ImGui::Text("HDR Path:");
            ImGui::SetNextItemWidth(400.0f);
            ImGui::InputText("##hdr_path", pathBuf, sizeof(pathBuf));
            if (ImGui::Button("Load") && pathBuf[0] != '\0') {
                m_SceneRenderer->LoadEnvironment(pathBuf);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("##no_default_hdr")) {
            ImGui::Text("Default HDR not found: assets/EnvironmentMaps/meadow_2_2k.hdr");
            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        // IBL 强度和天空盒设置
        ImGui::Spacing();
        ImGui::SliderFloat("Env Intensity", &environment.EnvironmentIntensity, 0.0f, 5.0f);
        ImGui::Checkbox("Show Skybox", &settings.ShowSkybox);
        if (settings.ShowSkybox) {
            ImGui::Indent();
            ImGui::SliderFloat("Skybox LOD", &settings.SkyboxLOD, 0.0f, 8.0f, "%.1f");
            ImGui::Unindent();
        }
    }

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
