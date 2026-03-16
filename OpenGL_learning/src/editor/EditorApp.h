#pragma once

// ============================================================================
// EditorApp.h - 编辑器应用程序
// ============================================================================
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
#include "scene/Grid.h"
#include "scene/Light.h"
#include "ui/ImGuiLayer.h"
#include "renderer/Renderer.h"
#include "renderer/RenderCommand.h"
#include "scene/ecs/ECS.h"
#include "editor/Editor.h"
#include "editor/panels/ViewportPanel.h"
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

    void RenderSkybox();

private:
    // DockSpace 设置
    void SetupDockSpace();

    // 场景渲染到视口 FBO
    void RenderSceneToViewport();

    void SetupShaders();

    void SetupBuffers();

    void SetupTextures();


private:
    // ImGui
    std::unique_ptr<ImGuiLayer> m_ImGuiLayer;

    // ECS
    std::unique_ptr<ECS::World> m_World;
    std::unique_ptr<ECS::SystemManager> m_SystemManager;

    // 编辑器
    std::unique_ptr<Editor> m_Editor;
    ViewportPanel* m_ViewportPanel = nullptr;  // 由 Editor 拥有

    // 场景资源
    Shader m_GridShader;
    Shader m_LightShader;
    Shader m_LampShader;
    Shader m_SkyboxShader;

    Texture m_DiffuseMap;
    Texture m_SpecularMap;
    TextureCube m_SkyboxTexture;

    std::unique_ptr<VertexArray> m_CubeVAO;

    std::unique_ptr<Grid> m_Grid;
    Model m_Model;

    // 光源
    DirectionalLight m_DirLight;
    PointLight m_PointLights[4];

    // 场景设置
    bool m_ShowGrid = true;
    float m_GridSize = 100.0f;
    float m_GridCellSize = 1.0f;

    glm::vec3 m_CubePositions[10];
    glm::vec3 m_PointLightPositions[4];
    std::array<std::filesystem::path, 6> m_SkyboxPaths;
};

} // namespace GLRenderer
