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
#include "asset/AssetManager.h"

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

private:
    // ImGui
    std::unique_ptr<ImGuiLayer> m_ImGuiLayer;

    // ECS
    std::unique_ptr<ECS::World> m_World;
    std::unique_ptr<ECS::SystemManager> m_SystemManager;

    // 场景渲染器
    std::unique_ptr<SceneRenderer> m_SceneRenderer;

    // 编辑器
    std::unique_ptr<Editor> m_Editor;
    ViewportPanel* m_ViewportPanel = nullptr;  // 由 Editor 拥有

    // 注：共享网格资源通过 MeshFactory 获取

    // 纹理资源 (通过 AssetManager 管理)
    AssetHandle m_DiffuseMapHandle;
    AssetHandle m_SpecularMapHandle;
    std::unique_ptr<TextureCube> m_SkyboxTexture;  // 天空盒暂不使用 AssetManager

    // 编辑器图标
    std::unique_ptr<Texture> m_PointLightIcon;
    std::unique_ptr<Texture> m_DirectionalLightIcon;
    std::unique_ptr<Texture> m_SpotLightIcon;
    std::unique_ptr<Texture> m_CameraIcon;

    // 天空盒路径
    std::array<std::filesystem::path, 6> m_SkyboxPaths;
};

} // namespace GLRenderer
