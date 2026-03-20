#pragma once

#include "core/Application.h"
#include "core/Ref.h"
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

#include <memory>
#include <array>
#include <filesystem>

namespace GLRenderer {

// SandboxApp - 沙盒应用程序
class SandboxApp : public Application {
public:
    SandboxApp();
    ~SandboxApp() override;

protected:
    // 生命周期方法
    void OnInit() override;
    void OnShutdown() override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
    void OnImGuiRender() override;
    void OnWindowResize(int width, int height) override;

private:
    void SetupShaders();
    void SetupBuffers();
    void SetupTextures();
    void SetupFramebuffers();

    void RenderSkybox();
    void RenderScene();
    void RenderLamps();
    void RenderCubes();
    void RenderOutlinedCube();
    void RenderTransparentObjects();
    void RenderPostProcess();
    void VisualizeDepthBuffer();

    void ProcessInput(float deltaTime);

private:
    Camera m_Camera;
    bool m_CameraControlEnabled = false;

    Ref<Shader> m_LightShader;         // 光照着色器
    Ref<Shader> m_LampShader;          // 光源立方体着色器
    Ref<Shader> m_ModelShader;         // 模型着色器
    Ref<Shader> m_GridShader;          // 网格着色器
    Ref<Shader> m_SingleColorShader;   // 单色着色器（轮廓）
    Ref<Shader> m_DebugDepthShader;    // 深度调试着色器
    Ref<Shader> m_TransparentShader;   // 透明物体着色器
    Ref<Shader> m_ScreenShader;        // 屏幕后处理着色器

    Ref<Shader> m_SkyboxShader;        // 天空盒着色器


    Ref<Texture2D> m_DiffuseMap;
    Ref<Texture2D> m_SpecularMap;
    Ref<Texture2D> m_TransparentTexture;

    Ref<TextureCube> m_SkyboxTexture;

    std::unique_ptr<VertexArray> m_CubeVAO;
    std::unique_ptr<VertexBuffer> m_CubeVBO;

    std::unique_ptr<VertexArray> m_ScreenQuadVAO;
    std::unique_ptr<VertexBuffer> m_ScreenQuadVBO;

    std::unique_ptr<VertexArray> m_TransparentVAO;
    std::unique_ptr<VertexBuffer> m_TransparentVBO;

    std::unique_ptr<Framebuffer> m_SceneFBO;
    std::unique_ptr<Framebuffer> m_DepthFBO;

    std::unique_ptr<Grid> m_Grid;
    // TODO: 使用 MeshSource/StaticMesh 替代 Model
    std::unique_ptr<ImGuiLayer> m_ImGuiLayer;

    DirectionalLight m_DirLight;
    PointLight m_PointLights[4];
    SpotLight m_SpotLight;

    glm::vec3 m_LightColor = glm::vec3(1.0f);
    glm::vec3 m_LightAmbient = glm::vec3(0.2f);
    glm::vec3 m_LightDiffuse = glm::vec3(0.5f);
    glm::vec3 m_LightSpecular = glm::vec3(1.0f);

    float m_MaterialShininess = 32.0f;

    bool m_ShowGrid = true;
    float m_GridSize = 100.0f;
    float m_GridCellSize = 1.0f;
    bool m_DebugDepth = false;
    bool m_AutoRotateLight = true;
    float m_LightRotationSpeed = 1.0f;
    float m_LightRotationRadius = 2.0f;

    glm::vec3 m_CubePositions[10];
    glm::vec3 m_PointLightPositions[4];
    std::vector<glm::vec3> m_WindowPositions;
    std::array<std::filesystem::path, 6> m_SkyboxPaths;

    // ECS 系统
    std::unique_ptr<ECS::World> m_ECSWorld;
    std::unique_ptr<ECS::SystemManager> m_ECSSystemManager;
    bool m_ShowECSDemo = false;
};

} // namespace GLRenderer
