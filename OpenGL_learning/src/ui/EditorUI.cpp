#include "EditorUI.h"

namespace GLRenderer {

void EditorUI::OnImGuiRender(const Camera& camera, EditorSettings& settings, float fps) {
    // 统计信息面板
    if (settings.ShowStats) {
        DrawStatsPanel(fps);
    }

    // 主设置面板
    ImGui::Begin("Settings");

    // 相机面板
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawCameraPanel(camera, settings);
    }

    // 光照面板
    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawLightPanel(settings);
    }

    // 网格面板
    if (ImGui::CollapsingHeader("Grid")) {
        DrawGridPanel(settings);
    }

    // 调试面板
    if (ImGui::CollapsingHeader("Debug")) {
        DrawDebugPanel(settings);
    }

    ImGui::End();
}

void EditorUI::DrawStatsPanel(float fps) {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200, 60), ImGuiCond_FirstUseEver);

    ImGui::Begin("Stats", nullptr,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse);

    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame Time: %.3f ms", 1000.0f / fps);

    ImGui::End();
}

void EditorUI::DrawCameraPanel(const Camera& camera, EditorSettings& settings) {
    // 相机控制开关
    if (ImGui::Checkbox("Enable Camera Control (TAB)", &settings.CameraControlEnabled)) {
        // 切换时的处理可以在外部进行
    }

    ImGui::Separator();

    // 相机信息（只读）
    ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                camera.GetPosition().x,
                camera.GetPosition().y,
                camera.GetPosition().z);
    ImGui::Text("Front: (%.2f, %.2f, %.2f)",
                camera.GetFront().x,
                camera.GetFront().y,
                camera.GetFront().z);
    ImGui::Text("Yaw: %.1f, Pitch: %.1f",
                camera.GetYaw(), camera.GetPitch());
    ImGui::Text("FOV: %.1f", camera.GetZoom());

    ImGui::Separator();
    ImGui::TextDisabled("Press TAB to toggle camera control");
    ImGui::TextDisabled("WASD to move, Mouse to look");
}

void EditorUI::DrawLightPanel(EditorSettings& settings) {
    // 光源位置
    ImGui::DragFloat3("Position", &settings.LightPosition.x, 0.1f);

    // 光源颜色
    ImGui::ColorEdit3("Color", &settings.LightColor.x);

    ImGui::Separator();

    // 光照分量
    ImGui::ColorEdit3("Ambient", &settings.LightAmbient.x);
    ImGui::ColorEdit3("Diffuse", &settings.LightDiffuse.x);
    ImGui::ColorEdit3("Specular", &settings.LightSpecular.x);

    ImGui::Separator();

    // 材质
    ImGui::SliderFloat("Shininess", &settings.MaterialShininess, 1.0f, 256.0f);

    ImGui::Separator();

    // 自动旋转
    ImGui::Checkbox("Auto Rotate", &settings.AutoRotateLight);
    if (settings.AutoRotateLight) {
        ImGui::SliderFloat("Rotation Speed", &settings.LightRotationSpeed, 0.1f, 5.0f);
        ImGui::SliderFloat("Rotation Radius", &settings.LightRotationRadius, 0.5f, 10.0f);
    }
}

void EditorUI::DrawGridPanel(EditorSettings& settings) {
    ImGui::Checkbox("Show Grid", &settings.ShowGrid);

    if (settings.ShowGrid) {
        ImGui::SliderFloat("Grid Size", &settings.GridSize, 10.0f, 500.0f);
        ImGui::SliderFloat("Cell Size", &settings.GridCellSize, 0.1f, 10.0f);
    }
}

void EditorUI::DrawDebugPanel(EditorSettings& settings) {
    ImGui::Checkbox("Show Stats", &settings.ShowStats);
    ImGui::Checkbox("Debug Depth Buffer", &settings.DebugDepth);
}

} // namespace GLRenderer
