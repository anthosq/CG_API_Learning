#include "EditorApp.h"
#include "core/Input.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>

namespace GLRenderer {

// 立方体顶点数据
static float s_CubeVertices[] = {
    // positions          // normals           // texture coords
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,

    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,

    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,

    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f
};

EditorApp::EditorApp()
    : Application(WindowProps("GLRenderer Editor", 1920, 1080))
{
    // 初始化立方体位置
    m_CubePositions[0] = glm::vec3(0.0f, 0.0f, 0.0f);
    m_CubePositions[1] = glm::vec3(2.0f, 5.0f, -15.0f);
    m_CubePositions[2] = glm::vec3(-1.5f, -2.2f, -2.5f);
    m_CubePositions[3] = glm::vec3(-3.8f, -2.0f, -12.3f);
    m_CubePositions[4] = glm::vec3(2.4f, -0.4f, -3.5f);
    m_CubePositions[5] = glm::vec3(-1.7f, 3.0f, -7.5f);
    m_CubePositions[6] = glm::vec3(1.3f, -2.0f, -2.5f);
    m_CubePositions[7] = glm::vec3(1.5f, 2.0f, -2.5f);
    m_CubePositions[8] = glm::vec3(1.5f, 0.2f, -1.5f);
    m_CubePositions[9] = glm::vec3(-1.3f, 1.0f, -1.5f);

    // 初始化点光源位置
    m_PointLightPositions[0] = glm::vec3(0.7f, 0.2f, 2.0f);
    m_PointLightPositions[1] = glm::vec3(2.3f, -3.3f, -4.0f);
    m_PointLightPositions[2] = glm::vec3(-4.0f, 2.0f, -12.0f);
    m_PointLightPositions[3] = glm::vec3(0.0f, 0.0f, -3.0f);

    // 天空盒路径
    m_SkyboxPaths = {
        "assets/pic/skybox/right.jpg",
        "assets/pic/skybox/left.jpg",
        "assets/pic/skybox/top.jpg",
        "assets/pic/skybox/bottom.jpg",
        "assets/pic/skybox/front.jpg",
        "assets/pic/skybox/back.jpg"
    };
}

EditorApp::~EditorApp() = default;

void EditorApp::OnInit() {
    std::cout << "[EditorApp] Initializing..." << std::endl;

    // 初始化渲染器
    Renderer::Init();

    // 初始化 ImGui
    m_ImGuiLayer = std::make_unique<ImGuiLayer>(GetWindow().GetNativeWindow());
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-Medium.ttf", 16.0f);

    auto& colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_TitleBgActive] = ImGui::ColorConvertU32ToFloat4(IM_COL32(186, 66, 30, 255));
    colors[ImGuiCol_Tab]                = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f }; // 默认暗色
    colors[ImGuiCol_TabHovered]         = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f }; // 悬停稍亮
    colors[ImGuiCol_TabActive]          = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f }; // 选中状态
    colors[ImGuiCol_TabUnfocused]       = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f }; // 未聚焦且未选中
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };    // 未聚焦但当前被选中

    // 初始化资产管理器
    AssetManager::Get().Initialize("assets");

    // 设置着色器
    std::cout << "[EditorApp] Setting up shaders..." << std::endl;
    SetupShaders();

    // 设置缓冲区
    std::cout << "[EditorApp] Setting up buffers..." << std::endl;
    SetupBuffers();

    // 设置纹理
    std::cout << "[EditorApp] Setting up textures..." << std::endl;
    SetupTextures();

    // 创建网格
    std::cout << "[EditorApp] Creating grid..." << std::endl;
    m_Grid = std::make_unique<Grid>(m_GridSize, m_GridCellSize);

    // 加载模型
    std::cout << "[EditorApp] Loading model..." << std::endl;
    m_Model = Model("assets/model/Hana/Hana.fbx");

    // 初始化 ECS
    std::cout << "[EditorApp] Initializing ECS..." << std::endl;
    m_World = std::make_unique<ECS::World>("EditorWorld");
    m_SystemManager = std::make_unique<ECS::SystemManager>();
    m_SystemManager->AddSystem<ECS::TransformSystem>();
    m_SystemManager->AddSystem<ECS::BehaviorSystem>();

    // 创建示例实体
    {
        ECS::Entity rotatingEntity = m_World->CreateEntity("RotatingCube");
        rotatingEntity.AddComponent<ECS::TransformComponent>(glm::vec3(0.0f, 2.0f, 0.0f));
        rotatingEntity.AddComponent<ECS::RotatorComponent>(glm::vec3(0, 1, 0), 45.0f);

        ECS::Entity floatingEntity = m_World->CreateEntity("FloatingCube");
        auto& floatTransform = floatingEntity.AddComponent<ECS::TransformComponent>(glm::vec3(3.0f, 1.0f, 0.0f));
        auto& floating = floatingEntity.AddComponent<ECS::FloatingComponent>(1.0f, 0.5f);
        floating.BasePosition = floatTransform.Position;

        ECS::Entity orbitingEntity = m_World->CreateEntity("OrbitingCube");
        orbitingEntity.AddComponent<ECS::TransformComponent>(glm::vec3(0.0f, 1.0f, 3.0f));
        orbitingEntity.AddComponent<ECS::OrbiterComponent>(glm::vec3(0.0f, 1.0f, 0.0f), 3.0f, 30.0f);
    }

    // 初始化编辑器
    std::cout << "[EditorApp] Initializing editor..." << std::endl;
    m_Editor = std::make_unique<Editor>();
    m_Editor->Initialize(m_World.get());

    // 添加视口面板
    auto viewportPanel = std::make_unique<ViewportPanel>();
    m_ViewportPanel = viewportPanel.get();
    m_Editor->AddPanel(std::move(viewportPanel));

    // 设置光源
    m_DirLight.Direction = glm::vec3(-0.2f, -1.0f, -0.3f);
    m_DirLight.Ambient = glm::vec3(0.05f);
    m_DirLight.Diffuse = glm::vec3(0.4f);
    m_DirLight.Specular = glm::vec3(0.5f);

    for (int i = 0; i < 4; i++) {
        m_PointLights[i].Position = m_PointLightPositions[i];
        m_PointLights[i].Ambient = glm::vec3(0.05f);
        m_PointLights[i].Diffuse = glm::vec3(0.8f);
        m_PointLights[i].Specular = glm::vec3(1.0f);
        m_PointLights[i].Constant = 1.0f;
        m_PointLights[i].Linear = 0.09f;
        m_PointLights[i].Quadratic = 0.032f;
    }

    std::cout << "[EditorApp] Initialization complete" << std::endl;
}

void EditorApp::OnShutdown() {
    std::cout << "[EditorApp] Shutting down..." << std::endl;

    m_Editor.reset();
    m_SystemManager.reset();
    m_World.reset();

    AssetManager::Get().Shutdown();

    m_ImGuiLayer.reset();
    m_Grid.reset();

    Renderer::Shutdown();

    std::cout << "[EditorApp] Shutdown complete" << std::endl;
}

void EditorApp::OnUpdate(float deltaTime) {
    // 更新 ECS
    if (m_World && m_SystemManager) {
        m_SystemManager->Update(*m_World, deltaTime);
    }

    // 更新编辑器
    if (m_Editor) {
        m_Editor->OnUpdate(deltaTime);
    }

    // 处理视口相机输入
    if (m_ViewportPanel) {
        m_ViewportPanel->ProcessCameraInput(deltaTime);

        // 处理鼠标拾取（仅在点击时读取，返回是否有新的选择）
        int pickedEntityID = -1;
        bool hasNewPick = m_ViewportPanel->ProcessMousePicking(pickedEntityID);

        // 处理选择结果
        if (hasNewPick) {
            if (pickedEntityID >= 0 && m_World) {
                // 检查是否是 ECS 实体（ID < 100 是 ECS 实体，>=100 是临时测试 ID）
                auto& registry = m_World->GetRegistry();
                auto entity = static_cast<entt::entity>(pickedEntityID);
                if (registry.valid(entity)) {
                    ECS::Entity ecsEntity(entity, m_World.get());
                    m_Editor->GetContext().Select(ecsEntity);
                    std::cout << "[EditorApp] Selected ECS entity: " << pickedEntityID << std::endl;
                } else {
                    std::cout << "[EditorApp] Clicked non-ECS object: " << pickedEntityID << std::endl;
                }
            } else {
                // 点击空白处取消选择
                m_Editor->GetContext().ClearSelection();
            }
        }
    }
}

void EditorApp::OnRender() {
    // 渲染场景到视口 FBO
    RenderSceneToViewport();
}

void EditorApp::OnImGuiRender() {
    m_ImGuiLayer->Begin();

    // 设置 DockSpace
    SetupDockSpace();

    // 渲染编辑器面板
    if (m_Editor) {
        m_Editor->OnImGuiRender();
    }

    m_ImGuiLayer->End();
}

void EditorApp::RenderSkybox() {
    Camera& camera = m_ViewportPanel->GetCamera();
    glm::mat4 view = glm::mat4(glm::mat3(camera.GetViewMatrix())); // 去除平移部分
    float aspectRatio = m_ViewportPanel->GetAspectRatio();
    glm::mat4 projection = camera.GetProjectionMatrix(aspectRatio);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    m_SkyboxShader.Bind();
    m_SkyboxShader.SetInt("skybox", 0);
    m_SkyboxShader.SetMat4("view", view);
    m_SkyboxShader.SetMat4("projection", projection);

    m_CubeVAO->Bind();
    m_SkyboxTexture.Bind(0);
    RenderCommand::DrawArrays(GL_TRIANGLES, 0, 36);
    m_CubeVAO->Unbind();

    glDepthMask(GL_TRUE);
    // glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
}



void EditorApp::SetupDockSpace() {
    // 设置全屏 DockSpace
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // 创建 DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("EditorDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    }

    // 菜单栏
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) {}
            if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {}
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                GetWindow().Close();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            // 面板可见性切换
            for (const auto& panel : m_Editor->GetPanels()) {
                bool isOpen = panel->IsOpen();
                if (ImGui::MenuItem(panel->GetName().c_str(), nullptr, &isOpen)) {
                    panel->SetOpen(isOpen);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {}
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::End();
}

void EditorApp::RenderSceneToViewport() {
    if (!m_ViewportPanel) return;

    Framebuffer* fbo = m_ViewportPanel->GetFramebuffer();
    if (!fbo || !fbo->IsValid()) return;

    if (m_ViewportPanel->GetSize().x != fbo->GetWidth() || m_ViewportPanel->GetSize().y != fbo->GetHeight()) {
        m_ViewportPanel->OnResize(m_ViewportPanel->GetSize().x, m_ViewportPanel->GetSize().y);
    }

    Camera& camera = m_ViewportPanel->GetCamera();
    float aspectRatio = m_ViewportPanel->GetAspectRatio();

    // 开始帧
    Renderer::BeginFrame(camera, aspectRatio);

    // 绑定视口 FBO
    fbo->Bind();
    RenderCommand::SetViewport(0, 0, fbo->GetWidth(), fbo->GetHeight());
    RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    RenderCommand::Clear();
    RenderCommand::EnableDepthTest();

    // 清除实体 ID 附件（-1 表示无实体）
    fbo->ClearEntityID(-1);

    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 projection = camera.GetProjectionMatrix(aspectRatio);

    // 渲染网格
    if (m_ShowGrid && m_Grid) {
        m_Grid->Draw(m_GridShader, view, projection, camera.GetNearPlane(), camera.GetFarPlane());
    }

    // 渲染立方体
    m_LightShader.Bind();
    m_LightShader.SetMat4("view", view);
    m_LightShader.SetMat4("projection", projection);
    m_LightShader.SetVec3("viewPos", camera.GetPosition());

    // 设置光源
    Renderer::SetupDirectionalLight(m_LightShader, m_DirLight);
    Renderer::SetupPointLights(m_LightShader, m_PointLights, 4);
    Renderer::DisableSpotLight(m_LightShader);  // 禁用聚光灯，避免未定义 uniform 导致 NaN

    // 材质
    m_CubeVAO->Bind();

    m_DiffuseMap.Bind(0);
    m_SpecularMap.Bind(1);
    m_LightShader.SetInt("material.diffuse", 0);
    m_LightShader.SetInt("material.specular", 1);
    m_LightShader.SetFloat("material.shininess", 32.0f);

    for (unsigned int i = 0; i < 10; i++) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, m_CubePositions[i]);
        float angle = 20.0f * i;
        model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));

        // 设置变换矩阵（包括法线矩阵）
        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
        m_LightShader.SetMat4("model", model);
        m_LightShader.SetMat3("normalMatrix", normalMatrix);
        m_LightShader.SetInt("u_EntityID", 100 + i);  // 临时 ID 用于测试拾取
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    // 渲染灯立方体
    m_LampShader.Bind();
    m_LampShader.SetMat4("view", view);
    m_LampShader.SetMat4("projection", projection);
    m_LampShader.SetVec3("lightColor", glm::vec3(1.0f));

    for (int i = 0; i < 4; i++) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, m_PointLightPositions[i]);
        model = glm::scale(model, glm::vec3(0.2f));
        m_LampShader.SetMat4("model", model);
        m_LampShader.SetInt("u_EntityID", 200 + i);  // 临时 ID 用于测试拾取
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    RenderSkybox();

    // 解绑 FBO
    fbo->Unbind();

    Renderer::EndFrame();
}

void EditorApp::SetupShaders() {
    m_GridShader = Shader(
        std::filesystem::path("assets/shaders/grid_vertex.glsl"),
        std::filesystem::path("assets/shaders/grid_fragment.glsl")
    );

    m_LightShader = Shader(
        std::filesystem::path("assets/shaders/light_vertex.glsl"),
        std::filesystem::path("assets/shaders/light_fragment.glsl")
    );

    m_LampShader = Shader(
        std::filesystem::path("assets/shaders/lamp_vertex.glsl"),
        std::filesystem::path("assets/shaders/lamp_fragment.glsl")
    );

    m_SkyboxShader = Shader(
        std::filesystem::path("assets/shaders/skybox_vertex.glsl"),
        std::filesystem::path("assets/shaders/skybox_frag.glsl")
    );
}

void EditorApp::SetupBuffers() {
    // 立方体 VAO/VBO
    m_CubeVAO = std::make_unique<VertexArray>();
    auto m_CubeVBO = std::make_unique<VertexBuffer>(s_CubeVertices, sizeof(s_CubeVertices));

    m_CubeVAO->Bind();
    m_CubeVBO->Bind();

    std::vector<VertexAttribute> cubeLayout = {
        VertexAttribute::Float(0, 3, 8 * sizeof(float), 0),                     // 位置
        VertexAttribute::Float(1, 3, 8 * sizeof(float), 3 * sizeof(float)),     // 法线
        VertexAttribute::Float(2, 2, 8 * sizeof(float), 6 * sizeof(float))      // 纹理坐标
    };
    m_CubeVAO->AddVertexBuffer(std::move(m_CubeVBO), cubeLayout);

    m_CubeVAO->Unbind();
}

void EditorApp::SetupTextures() {
    m_DiffuseMap = Texture("assets/pic/container2.png");
    m_SpecularMap = Texture("assets/pic/container2_specular.png");
    m_SkyboxTexture = TextureCube(m_SkyboxPaths);

    // 验证纹理加载
    std::cout << "[EditorApp] Diffuse texture ID: " << m_DiffuseMap.GetID()
              << ", Size: " << m_DiffuseMap.GetWidth() << "x" << m_DiffuseMap.GetHeight() << std::endl;
    std::cout << "[EditorApp] Specular texture ID: " << m_SpecularMap.GetID()
              << ", Size: " << m_SpecularMap.GetWidth() << "x" << m_SpecularMap.GetHeight() << std::endl;
    std::cout << "[EditorApp] Skybox texture ID: " << m_SkyboxTexture.GetID() << std::endl;
}

void EditorApp::OnWindowResize(int width, int height) {
    // 窗口大小变化由 ImGui 视口自行处理
}

} // namespace GLRenderer
