#include "Editor.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/AssetBrowserPanel.h"
#include "panels/ConsolePanel.h"
#include "asset/AssetManager.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>

namespace GLRenderer {

Editor::Editor() = default;

Editor::~Editor() {
    Shutdown();
}

void Editor::Initialize(ECS::World* world) {
    std::cout << "[Editor] Initializing..." << std::endl;

    m_Context.World = world;

    // 创建面板
    m_Panels.push_back(std::make_unique<HierarchyPanel>());
    m_Panels.push_back(std::make_unique<InspectorPanel>());
    m_Panels.push_back(std::make_unique<AssetBrowserPanel>());
    m_Panels.push_back(std::make_unique<ConsolePanel>());

    // 记录初始化日志
    ConsolePanel::LogInfo("Editor initialized");
    ConsolePanel::LogInfo("World: " + world->GetName());

    std::cout << "[Editor] Initialized with " << m_Panels.size() << " panels" << std::endl;
}

void Editor::Shutdown() {
    m_Panels.clear();
    m_Context.World = nullptr;
    std::cout << "[Editor] Shutdown" << std::endl;
}

void Editor::OnUpdate(float deltaTime) {
    // 处理编辑器快捷键
    // 例如: Delete 键删除选中实体

    ImGuiIO& io = ImGui::GetIO();

    // 只在没有输入焦点时处理快捷键
    if (!io.WantCaptureKeyboard) {
        // Delete - 删除选中实体
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_Context.HasSelection()) {
            if (m_Context.World) {
                m_Context.World->DestroyEntity(m_Context.SelectedEntity);
                m_Context.ClearSelection();
                ConsolePanel::LogInfo("Entity deleted");
            }
        }

        // Ctrl+D - 复制实体（待实现）
        // if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        //     // TODO: Duplicate entity
        // }
    }
}

void Editor::OnImGuiRender() {
    // 绘制菜单栏
    DrawMenuBar();

    // 绘制工具栏
    DrawToolbar();

    // 绘制所有面板
    for (auto& panel : m_Panels) {
        panel->Draw(m_Context);
    }

    // ImGui Demo 窗口（调试用）
    if (m_ShowDemoWindow) {
        ImGui::ShowDemoWindow(&m_ShowDemoWindow);
    }
}

void Editor::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene",  "Ctrl+N")) { if (m_Context.OnNewScene)    m_Context.OnNewScene(); }
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) { if (m_Context.OnOpenScene)   m_Context.OnOpenScene(); }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) { if (m_Context.OnSaveScene)   m_Context.OnSaveScene(); }
            if (ImGui::MenuItem("Save As..."))            { if (m_Context.OnSaveSceneAs) m_Context.OnSaveSceneAs(); }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                // TODO: 撤销
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                // TODO: 重做
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Del", false, m_Context.HasSelection())) {
                if (m_Context.World) {
                    m_Context.World->DestroyEntity(m_Context.SelectedEntity);
                    m_Context.ClearSelection();
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            for (auto& panel : m_Panels) {
                bool isOpen = panel->IsOpen();
                if (ImGui::MenuItem(panel->GetName().c_str(), nullptr, isOpen)) {
                    panel->SetOpen(!isOpen);
                }
            }
            ImGui::Separator();
            ImGui::MenuItem("ImGui Demo", nullptr, &m_ShowDemoWindow);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Entity")) {
            if (ImGui::MenuItem("Create Empty")) {
                if (m_Context.World) {
                    auto entity = m_Context.World->CreateEntity("New Entity");
                    entity.AddComponent<ECS::TransformComponent>();
                    m_Context.Select(entity);
                }
            }
            if (ImGui::BeginMenu("Create...")) {
                if (ImGui::MenuItem("Cube")) {
                    if (m_Context.World) {
                        auto entity = m_Context.World->CreateEntity("Cube");
                        entity.AddComponent<ECS::TransformComponent>();
                        entity.AddComponent<ECS::MeshComponent>();
                        m_Context.Select(entity);
                    }
                }
                if (ImGui::MenuItem("Point Light")) {
                    if (m_Context.World) {
                        auto entity = m_Context.World->CreateEntity("Point Light");
                        entity.AddComponent<ECS::TransformComponent>();
                        entity.AddComponent<ECS::PointLightComponent>();
                        m_Context.Select(entity);
                    }
                }
                if (ImGui::MenuItem("Directional Light")) {
                    if (m_Context.World) {
                        auto entity = m_Context.World->CreateEntity("Directional Light");
                        entity.AddComponent<ECS::TransformComponent>();
                        entity.AddComponent<ECS::DirectionalLightComponent>();
                        m_Context.Select(entity);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                // TODO: 关于对话框
                ConsolePanel::LogInfo("GLRenderer Editor v0.1");
            }
            ImGui::EndMenu();
        }

        // 右侧状态信息
        float rightOffset = ImGui::GetContentRegionAvail().x - 200;
        if (rightOffset > 0) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + rightOffset);
        }

        // 显示 FPS
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        // 显示资产计数
        ImGui::SameLine();
        ImGui::Text("| Assets: %zu", AssetManager::Get().GetTotalAssetCount());

        ImGui::EndMainMenuBar();
    }
}

void Editor::DrawToolbar() {
    const bool isPlaying = m_Context.IsPlaying;  // 快照：防止按钮回调在同帧改变状态导致 Push/Pop 不匹配

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

    if (isPlaying)
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.45f, 0.22f, 0.0f, 1.0f));

    if (ImGui::BeginViewportSideBar("##Toolbar", ImGui::GetMainViewport(),
                                     ImGuiDir_Up, 32.0f,
                                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
        // Gizmo 模式按钮
        const char* gizmoModes[] = { "T", "R", "S" };
        const char* gizmoTips[] = { "Translate", "Rotate", "Scale" };

        for (int i = 0; i < 3; i++) {
            bool selected = (static_cast<int>(m_Context.CurrentGizmoMode) == i);

            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
            }

            if (ImGui::Button(gizmoModes[i], ImVec2(24, 24))) {
                m_Context.CurrentGizmoMode = static_cast<EditorContext::GizmoMode>(i);
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", gizmoTips[i]);
            }

            if (selected) {
                ImGui::PopStyleColor();
            }

            ImGui::SameLine();
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // 空间切换
        const char* spaceLabel = m_Context.CurrentGizmoSpace == EditorContext::GizmoSpace::Local ? "Local" : "World";
        if (ImGui::Button(spaceLabel, ImVec2(50, 24))) {
            m_Context.CurrentGizmoSpace =
                m_Context.CurrentGizmoSpace == EditorContext::GizmoSpace::Local
                    ? EditorContext::GizmoSpace::World
                    : EditorContext::GizmoSpace::Local;
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Play / Pause / Stop（使用快照 isPlaying，避免回调改状态后 Pop 不匹配）
        if (!isPlaying) {
            if (ImGui::Button("Play", ImVec2(50, 24)))
                if (m_Context.OnEnterPlay) m_Context.OnEnterPlay();
        } else {
            const char* pauseLabel = m_Context.IsPaused ? "Resume" : "Pause";
            if (ImGui::Button(pauseLabel, ImVec2(55, 24)))
                m_Context.IsPaused = !m_Context.IsPaused;
            ImGui::SameLine();
            if (ImGui::Button("Stop", ImVec2(50, 24)))
                if (m_Context.OnExitPlay) m_Context.OnExitPlay();
        }

        ImGui::End();
    }

    if (isPlaying)
        ImGui::PopStyleColor();

    ImGui::PopStyleVar(2);
}

void Editor::SetPanelOpen(const std::string& name, bool open) {
    for (auto& panel : m_Panels) {
        if (panel->GetName() == name) {
            panel->SetOpen(open);
            return;
        }
    }
}

} // namespace GLRenderer
