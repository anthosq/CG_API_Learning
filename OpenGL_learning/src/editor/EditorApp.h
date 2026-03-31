#pragma once

//
// EditorApp 是独立的编辑器应用，包含：
// 1. DockSpace 布局
// 2. 视口渲染 (场景渲染到 FBO)
// 3. 编辑器面板管理
// 4. 场景资源管理
//

#include "core/Application.h"
#include "graphics/Camera.h"
#include "graphics/Shader.h"
#include "graphics/Texture.h"
#include "graphics/Buffer.h"
#include "graphics/Framebuffer.h"
#include "graphics/Mesh.h"
#include "graphics/MeshFactory.h"
#include "scene/Grid.h"
#include "scene/Light.h"
#include "ui/ImGuiLayer.h"
#include "renderer/Renderer.h"
#include "renderer/Renderer2D.h"
#include "renderer/RenderCommand.h"
#include "renderer/SceneRenderer.h"
#include "scene/ecs/ECS.h"
#include "editor/Editor.h"
#include "editor/panels/ViewportPanel.h"
#include "editor/panels/SettingsPanel.h"
#include "editor/panels/PrimitivesPanel.h"
#include "editor/panels/GPUProfilerPanel.h"
#include "asset/AssetManager.h"
#include "scene/SceneSerializer.h"

#include <memory>
#include <filesystem>

namespace GLRenderer {

class EditorApp : public Application {
public:
    EditorApp();
    ~EditorApp() override;

protected:
    void OnInit() override;
    void OnShutdown() override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
    void OnImGuiRender() override;
    void OnWindowResize(int width, int height) override;

private:
    // DockSpace 设置
    void SetupDockSpace();

    // 场景渲染到视口 FBO
    void RenderSceneToViewport();

    // 渲染编辑器可视化 (光源图标等)
    void RenderEditorVisuals();

    // 加载编辑器图标
    void LoadEditorIcons();

    // 创建示例场景实体
    void CreateSceneEntities();

    // 创建网格资源（立方体 VAO）
    void CreateMeshResources();

    // 场景 I/O
    void NewScene();
    void SaveScene();
    void SaveSceneAs();
    void OpenScene(const std::filesystem::path& path = {});

    // Play Mode
    void EnterPlayMode();
    void ExitPlayMode();
    bool IsPlaying() const { return m_Editor && m_Editor->GetContext().IsPlaying; }

private:
    // ImGui
    std::unique_ptr<ImGuiLayer> m_ImGuiLayer;

    // ECS
    std::unique_ptr<ECS::World> m_World;        // 编辑器世界（始终保留）
    std::unique_ptr<ECS::World> m_PlayWorld;    // 运行时副本（Play 期间有效）
    std::string m_PlayWorldSnapshot;            // Enter Play 前的 JSON 快照
    std::unique_ptr<ECS::SystemManager> m_SystemManager;

    // 场景渲染器
    std::unique_ptr<SceneRenderer> m_SceneRenderer;

    // 编辑器
    std::unique_ptr<Editor> m_Editor;
    ViewportPanel* m_ViewportPanel = nullptr;  // 由 Editor 拥有

    float m_LastDeltaTime = 0.0f;  // OnUpdate → OnRender 传递 deltaTime

    // 注：共享网格资源通过 MeshFactory 获取

    // 纹理资源 (通过 AssetManager 管理)
    AssetHandle m_DiffuseMapHandle;
    AssetHandle m_SpecularMapHandle;
    Ref<TextureCube> m_SkyboxTexture;  // 天空盒暂不使用 AssetManager

    // 编辑器图标
    Ref<Texture2D> m_PointLightIcon;
    Ref<Texture2D> m_DirectionalLightIcon;
    Ref<Texture2D> m_SpotLightIcon;
    Ref<Texture2D> m_CameraIcon;

    // 天空盒路径
    std::array<std::filesystem::path, 6> m_SkyboxPaths;

    // 场景文件路径（空 = 尚未保存）
    std::filesystem::path m_CurrentScenePath;

    // "另存为" 输入状态
    bool        m_ShowSaveAsDialog = false;
    char        m_SaveAsPathBuf[512] = "assets/scenes/scene.glscene";
};

} // namespace GLRenderer
