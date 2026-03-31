#include "SandboxApp.h"
#include "core/Input.h"
#include <imgui.h>
#include <map>

namespace GLRenderer {

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

static float s_ScreenQuadVertices[] = {
    // positions   // texCoords
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
};

static float s_TransparentVertices[] = {
    // positions        // texture Coords
    0.0f, 1.0f, 0.0f,   0.0f, 1.0f,
    0.0f, 0.0f, 0.0f,   0.0f, 0.0f,
    1.0f, 0.0f, 0.0f,   1.0f, 0.0f,

    0.0f, 1.0f, 0.0f,   0.0f, 1.0f,
    1.0f, 0.0f, 0.0f,   1.0f, 0.0f,
    1.0f, 1.0f, 0.0f,   1.0f, 1.0f
};

SandboxApp::SandboxApp()
    : Application(WindowProps("GLRenderer Sandbox", 1280, 720))
    , m_Camera(glm::vec3(5.0f, 5.0f, 5.0f))
{
    // 初始化相机朝向
    m_Camera.Yaw = -135.0f;
    m_Camera.Pitch = -30.0f;

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

    // 初始化透明物体位置
    m_WindowPositions = {
        glm::vec3(-1.5f, 0.0f, -0.48f),
        glm::vec3(1.5f, 0.0f, 0.51f),
        glm::vec3(0.0f, 0.0f, 0.7f),
        glm::vec3(-0.3f, 0.0f, -2.3f),
        glm::vec3(0.5f, 0.0f, -0.6f)
    };

    // 初始化点光源
    for (int i = 0; i < 4; ++i) {
        m_PointLights[i].Position = m_PointLightPositions[i];
        m_PointLights[i].Ambient = m_LightAmbient;
        m_PointLights[i].Diffuse = glm::vec3(0.8f);
        m_PointLights[i].Specular = glm::vec3(1.0f);
    }

    // 初始化方向光
    m_DirLight.Direction = glm::vec3(-0.2f, -1.0f, -0.3f);
    m_DirLight.Ambient = m_LightAmbient;
    m_DirLight.Diffuse = m_LightDiffuse;
    m_DirLight.Specular = m_LightSpecular;
}

SandboxApp::~SandboxApp() = default;


void SandboxApp::OnInit() {
    std::cout << "[SandboxApp] 初始化中..." << std::endl;

    // 初始化渲染器
    Renderer::Init();

    // 初始化 ImGui
    m_ImGuiLayer = std::make_unique<ImGuiLayer>(GetWindow().GetNativeWindow());

    std::cout << "[SandboxApp] 设置着色器..." << std::endl;
    // 设置着色器
    SetupShaders();

    std::cout << "[SandboxApp] 设置缓冲区..." << std::endl;
    // 设置缓冲区
    SetupBuffers();

    std::cout << "[SandboxApp] 设置纹理..." << std::endl;
    // 设置纹理
    SetupTextures();

    std::cout << "[SandboxApp] 设置帧缓冲..." << std::endl;
    // 设置帧缓冲
    SetupFramebuffers();

    std::cout << "[SandboxApp] 创建网格..." << std::endl;
    // 创建网格
    m_Grid = std::make_unique<Grid>(m_GridSize, m_GridCellSize);

    // TODO: 使用 MeshSource/StaticMesh 加载模型

    std::cout << "[SandboxApp] 初始化 ECS 系统..." << std::endl;
    // 初始化 ECS
    m_ECSWorld = std::make_unique<ECS::World>("SandboxWorld");
    m_ECSSystemManager = std::make_unique<ECS::SystemManager>();

    // 注册系统
    m_ECSSystemManager->AddSystem<ECS::TransformSystem>();
    m_ECSSystemManager->AddSystem<ECS::BehaviorSystem>();

    // 创建示例实体
    {
        // 创建一个旋转的实体
        ECS::Entity rotatingEntity = m_ECSWorld->CreateEntity("RotatingCube");
        rotatingEntity.AddComponent<ECS::TransformComponent>(glm::vec3(0.0f, 2.0f, 0.0f));
        rotatingEntity.AddComponent<ECS::RotatorComponent>(glm::vec3(0, 1, 0), 45.0f);

        // 创建一个浮动的实体
        ECS::Entity floatingEntity = m_ECSWorld->CreateEntity("FloatingCube");
        auto& floatTransform = floatingEntity.AddComponent<ECS::TransformComponent>(glm::vec3(3.0f, 1.0f, 0.0f));
        auto& floating = floatingEntity.AddComponent<ECS::FloatingComponent>(1.0f, 0.5f);
        floating.BasePosition = floatTransform.Position;

        // 创建一个轨道运动的实体
        ECS::Entity orbitingEntity = m_ECSWorld->CreateEntity("OrbitingCube");
        orbitingEntity.AddComponent<ECS::TransformComponent>(glm::vec3(0.0f, 1.0f, 3.0f));
        orbitingEntity.AddComponent<ECS::OrbiterComponent>(glm::vec3(0.0f, 1.0f, 0.0f), 3.0f, 30.0f);
    }

    std::cout << "[SandboxApp] 初始化完成" << std::endl;
}

void SandboxApp::OnShutdown() {
    std::cout << "[SandboxApp] 关闭中..." << std::endl;

    // 清理 ECS
    m_ECSSystemManager.reset();
    m_ECSWorld.reset();

    m_ImGuiLayer.reset();
    m_Grid.reset();
    m_SceneFBO.reset();
    m_DepthFBO.reset();

    Renderer::Shutdown();

    std::cout << "[SandboxApp] 已关闭" << std::endl;
}

void SandboxApp::OnUpdate(float deltaTime) {
    ProcessInput(deltaTime);

    // 更新 ECS 系统
    if (m_ECSWorld && m_ECSSystemManager) {
        m_ECSSystemManager->Update(*m_ECSWorld, deltaTime);
    }
}

void SandboxApp::OnRender() {
    // 开始帧渲染
    float aspectRatio = GetWindow().GetAspectRatio();
    Renderer::BeginFrame(m_Camera, aspectRatio);

    // 渲染到帧缓冲
    m_SceneFBO->Bind();
    RenderCommand::Clear();
    RenderCommand::EnableDepthTest();
    RenderCommand::SetDepthFunc(GL_LESS);
    RenderCommand::EnableBlending();
    RenderCommand::SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    // 渲染场景
    RenderScene();

    // 绘制天空盒
    RenderSkybox();

    // 深度可视化（如果启用）
    if (m_DebugDepth) {
        VisualizeDepthBuffer();
    }

    // 后处理
    RenderPostProcess();

    Renderer::EndFrame();
}

void SandboxApp::OnImGuiRender() {
    m_ImGuiLayer->Begin();

    ImGui::Begin("Lighting Control Panel");

    // FPS 显示
    ImGui::Text("FPS: %.1f (%.3f ms/frame)",
                ImGui::GetIO().Framerate,
                1000.0f / ImGui::GetIO().Framerate);
    ImGui::Separator();

    // 相机信息
    if (ImGui::CollapsingHeader("Camera Info", ImGuiTreeNodeFlags_DefaultOpen)) {
        glm::vec3 pos = m_Camera.GetPosition();
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        ImGui::Text("Yaw: %.2f, Pitch: %.2f", m_Camera.Yaw, m_Camera.Pitch);
        ImGui::Text("FOV: %.2f", m_Camera.Zoom);
        ImGui::Text("Camera Control: %s", m_CameraControlEnabled ? "Enabled (TAB to disable)" : "Disabled (TAB to enable)");
    }

    ImGui::Separator();

    // 光源控制
    if (ImGui::CollapsingHeader("Light Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Auto Rotate Light", &m_AutoRotateLight);

        if (!m_AutoRotateLight) {
            // ImGui::SliderFloat3("Light Position", &lightPos.x, -5.0f, 5.0f);
        } else {
            ImGui::SliderFloat("Rotation Speed", &m_LightRotationSpeed, 0.1f, 5.0f);
            ImGui::SliderFloat("Rotation Radius", &m_LightRotationRadius, 0.5f, 5.0f);
        }

        ImGui::ColorEdit3("Light Color", &m_LightColor.x);
        ImGui::Separator();

        ImGui::Text("Light Properties:");
        ImGui::ColorEdit3("Ambient", &m_LightAmbient.x);
        ImGui::ColorEdit3("Diffuse", &m_LightDiffuse.x);
        ImGui::ColorEdit3("Specular", &m_LightSpecular.x);
    }

    ImGui::Separator();

    // 添加网格控制
    if (ImGui::CollapsingHeader("Grid Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Grid", &m_ShowGrid);

        if (ImGui::SliderFloat("Grid Size", &m_GridSize, 10.0f, 200.0f)) {
            m_Grid->SetGridSize(m_GridSize);
        }

        if (ImGui::SliderFloat("Cell Size", &m_GridCellSize, 0.1f, 10.0f)) {
            m_Grid->SetGridCellSize(m_GridCellSize);
        }
    }

    // 深度检测
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Depth Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Visualize Depth Buffer", &m_DebugDepth);
    }

    // ECS 信息
    ImGui::Separator();
    if (ImGui::CollapsingHeader("ECS System", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show ECS Demo Panel", &m_ShowECSDemo);

        if (m_ECSWorld) {
            ImGui::Text("World: %s", m_ECSWorld->GetName().c_str());
            ImGui::Text("Entity Count: %zu", m_ECSWorld->GetEntityCount());
            ImGui::Text("Transform Components: %zu", m_ECSWorld->GetComponentCount<ECS::TransformComponent>());
            ImGui::Text("Rotator Components: %zu", m_ECSWorld->GetComponentCount<ECS::RotatorComponent>());
            ImGui::Text("Floating Components: %zu", m_ECSWorld->GetComponentCount<ECS::FloatingComponent>());
            ImGui::Text("Orbiter Components: %zu", m_ECSWorld->GetComponentCount<ECS::OrbiterComponent>());
        }
    }

    ImGui::End();

    // ECS 演示面板
    if (m_ShowECSDemo && m_ECSWorld) {
        ImGui::Begin("ECS Demo", &m_ShowECSDemo);

        ImGui::Text("Entity List:");
        ImGui::Separator();

        // 遍历所有带 TransformComponent 的实体
        m_ECSWorld->Each<ECS::TransformComponent>([](ECS::Entity entity, ECS::TransformComponent& transform) {
            auto& tag = entity.GetComponent<ECS::TagComponent>();

            if (ImGui::TreeNode(tag.Name.c_str())) {
                ImGui::Text("Position: (%.2f, %.2f, %.2f)",
                    transform.Position.x, transform.Position.y, transform.Position.z);

                glm::vec3 euler = transform.GetEulerAngles();
                ImGui::Text("Rotation: (%.2f, %.2f, %.2f)",
                    euler.x, euler.y, euler.z);

                ImGui::Text("Scale: (%.2f, %.2f, %.2f)",
                    transform.Scale.x, transform.Scale.y, transform.Scale.z);

                // 显示附加组件
                if (entity.HasComponent<ECS::RotatorComponent>()) {
                    auto& rotator = entity.GetComponent<ECS::RotatorComponent>();
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
                        "  [Rotator] Speed: %.1f deg/s", rotator.Speed);
                }

                if (entity.HasComponent<ECS::FloatingComponent>()) {
                    auto& floating = entity.GetComponent<ECS::FloatingComponent>();
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 1.0f, 1.0f),
                        "  [Floating] Amp: %.2f, Freq: %.2f", floating.Amplitude, floating.Frequency);
                }

                if (entity.HasComponent<ECS::OrbiterComponent>()) {
                    auto& orbiter = entity.GetComponent<ECS::OrbiterComponent>();
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
                        "  [Orbiter] Radius: %.2f, Speed: %.1f", orbiter.Radius, orbiter.Speed);
                }

                ImGui::TreePop();
            }
        });

        ImGui::Separator();

        // 创建新实体的按钮
        if (ImGui::Button("Add Rotating Entity")) {
            static int rotatorCount = 0;
            ECS::Entity newEntity = m_ECSWorld->CreateEntity("NewRotator_" + std::to_string(rotatorCount++));
            newEntity.AddComponent<ECS::TransformComponent>(
                glm::vec3((rand() % 10) - 5, (rand() % 5), (rand() % 10) - 5));
            newEntity.AddComponent<ECS::RotatorComponent>(
                glm::vec3(0, 1, 0), 30.0f + (rand() % 60));
        }

        ImGui::SameLine();

        if (ImGui::Button("Add Floating Entity")) {
            static int floaterCount = 0;
            ECS::Entity newEntity = m_ECSWorld->CreateEntity("NewFloater_" + std::to_string(floaterCount++));
            auto& t = newEntity.AddComponent<ECS::TransformComponent>(
                glm::vec3((rand() % 10) - 5, 1.0f, (rand() % 10) - 5));
            auto& f = newEntity.AddComponent<ECS::FloatingComponent>(
                0.5f + (rand() % 100) / 100.0f, 0.5f + (rand() % 100) / 100.0f);
            f.BasePosition = t.Position;
        }

        ImGui::End();
    }

    m_ImGuiLayer->End();
}

void SandboxApp::OnWindowResize(int width, int height) {
    if (width > 0 && height > 0) {
        if (m_SceneFBO) {
            m_SceneFBO->Resize(width, height);
        }
        if (m_DepthFBO) {
            m_DepthFBO->Resize(width, height);
        }
    }
}

void SandboxApp::SetupShaders() {
    m_LightShader = Ref<Shader>::Create(
        std::filesystem::path("assets/shaders/light_vertex.glsl"),
        std::filesystem::path("assets/shaders/light_fragment.glsl")
    );

    m_LampShader = Ref<Shader>::Create(
        std::filesystem::path("assets/shaders/lamp_vertex.glsl"),
        std::filesystem::path("assets/shaders/lamp_fragment.glsl")
    );

    m_ModelShader = Ref<Shader>::Create(
        std::filesystem::path("assets/shaders/model_loading_vertex.glsl"),
        std::filesystem::path("assets/shaders/model_loading_frag.glsl")
    );

    m_GridShader = Ref<Shader>::Create(
        std::filesystem::path("assets/shaders/grid_vertex.glsl"),
        std::filesystem::path("assets/shaders/grid_fragment.glsl")
    );

    m_SingleColorShader = Ref<Shader>::Create(
        std::filesystem::path("assets/shaders/light_vertex.glsl"),
        std::filesystem::path("assets/shaders/shader_single_color.glsl")
    );

    m_DebugDepthShader = Ref<Shader>::Create(
        std::filesystem::path("assets/shaders/screen_vertex.glsl"),
        std::filesystem::path("assets/shaders/depth_debug_fragment.glsl")
    );

    m_TransparentShader = Ref<Shader>::Create(
        std::filesystem::path("assets/shaders/grass_vertex.glsl"),
        std::filesystem::path("assets/shaders/grass_fragment.glsl")
    );

    m_ScreenShader = Ref<Shader>::Create(
        std::filesystem::path("assets/shaders/framebuffer_vertex.glsl"),
        std::filesystem::path("assets/shaders/framebuffer_frag.glsl")
    );

    m_SkyboxShader = Ref<Shader>::Create(
        std::filesystem::path("assets/shaders/skybox_vertex.glsl"),
        std::filesystem::path("assets/shaders/skybox_frag.glsl")
    );
}

void SandboxApp::SetupBuffers() {
    // 立方体 VAO
    m_CubeVAO = std::make_unique<VertexArray>();
    auto cubeVBO = Ref<VertexBuffer>::Create(s_CubeVertices, sizeof(s_CubeVertices));

    std::vector<VertexAttribute> cubeLayout = {
        VertexAttribute::Float(0, 3, 8 * sizeof(float), 0),                     // 位置
        VertexAttribute::Float(1, 3, 8 * sizeof(float), 3 * sizeof(float)),     // 法线
        VertexAttribute::Float(2, 2, 8 * sizeof(float), 6 * sizeof(float))      // 纹理坐标
    };
    m_CubeVAO->AddVertexBuffer(cubeVBO, cubeLayout);

    // 屏幕四边形 VAO
    m_ScreenQuadVAO = std::make_unique<VertexArray>();
    auto screenVBO = Ref<VertexBuffer>::Create(s_ScreenQuadVertices, sizeof(s_ScreenQuadVertices));

    std::vector<VertexAttribute> screenLayout = {
        VertexAttribute::Float(0, 2, 4 * sizeof(float), 0),                     // 位置
        VertexAttribute::Float(1, 2, 4 * sizeof(float), 2 * sizeof(float))      // 纹理坐标
    };
    m_ScreenQuadVAO->AddVertexBuffer(screenVBO, screenLayout);

    // 透明物体 VAO
    m_TransparentVAO = std::make_unique<VertexArray>();
    auto transparentVBO = Ref<VertexBuffer>::Create(s_TransparentVertices, sizeof(s_TransparentVertices));

    std::vector<VertexAttribute> transparentLayout = {
        VertexAttribute::Float(0, 3, 5 * sizeof(float), 0),                     // 位置
        VertexAttribute::Float(1, 2, 5 * sizeof(float), 3 * sizeof(float))      // 纹理坐标
    };
    m_TransparentVAO->AddVertexBuffer(transparentVBO, transparentLayout);

}

void SandboxApp::SetupTextures() {
    m_DiffuseMap = Ref<Texture2D>::Create("assets/pic/container2.png");
    m_SpecularMap = Ref<Texture2D>::Create("assets/pic/container2_specular.png");

    // 透明纹理使用特殊配置
    TextureSpec transparentSpec;
    transparentSpec.WrapS = GL_CLAMP_TO_EDGE;
    transparentSpec.WrapT = GL_CLAMP_TO_EDGE;
    m_TransparentTexture = Ref<Texture2D>::Create("assets/pic/blending_transparent_window.png", transparentSpec);

    // skyboxTextures
    m_SkyboxPaths = {
        "assets/pic/skybox/right.jpg",
        "assets/pic/skybox/left.jpg",
        "assets/pic/skybox/top.jpg",
        "assets/pic/skybox/bottom.jpg",
        "assets/pic/skybox/front.jpg",
        "assets/pic/skybox/back.jpg"
    };
    m_SkyboxTexture = Ref<TextureCube>::Create(m_SkyboxPaths);
}

void SandboxApp::SetupFramebuffers() {
    int width = GetWindow().GetWidth();
    int height = GetWindow().GetHeight();

    // 场景帧缓冲
    FramebufferSpec sceneSpec;
    sceneSpec.Width = width;
    sceneSpec.Height = height;
    sceneSpec.HasColorAttachment = true;
    sceneSpec.HasDepthAttachment = true;
    m_SceneFBO = std::make_unique<Framebuffer>(sceneSpec);

    // 深度帧缓冲（用于调试）
    FramebufferSpec depthSpec;
    depthSpec.Width = width;
    depthSpec.Height = height;
    depthSpec.HasColorAttachment = false;
    depthSpec.HasDepthAttachment = true;
    depthSpec.DepthAsTexture = true;
    m_DepthFBO = std::make_unique<Framebuffer>(depthSpec);

}

void SandboxApp::RenderSkybox() {
    glm::mat4 view = glm::mat4(glm::mat3(m_Camera.GetViewMatrix())); // 去除平移部分
    float aspectRatio = GetWindow().GetAspectRatio();
    glm::mat4 projection = m_Camera.GetProjectionMatrix(aspectRatio);

    glDepthFunc(GL_LEQUAL);
    // glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    m_SkyboxShader->Bind();
    m_SkyboxShader->SetInt("skybox", 0);
    m_SkyboxShader->SetMat4("view", view);
    m_SkyboxShader->SetMat4("projection", projection);

    m_CubeVAO->Bind();
    m_SkyboxTexture->Bind(0);
    RenderCommand::DrawArrays(GL_TRIANGLES, 0, 36);
    m_CubeVAO->Unbind();

    glDepthMask(GL_TRUE);
    // glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
}

void SandboxApp::RenderScene() {
    glm::mat4 view = m_Camera.GetViewMatrix();
    float aspectRatio = GetWindow().GetAspectRatio();
    glm::mat4 projection = m_Camera.GetProjectionMatrix(aspectRatio);

    // 渲染网格
    if (m_ShowGrid) {
        m_Grid->Draw(*m_GridShader, view, projection,
                     m_Camera.GetNearPlane(), m_Camera.GetFarPlane());
    }

    // 渲染光源立方体
    RenderLamps();

    // 渲染立方体
    RenderCubes();

    // 渲染轮廓立方体
    RenderOutlinedCube();

    // TODO: 使用 MeshSource/StaticMesh 渲染模型

    // 渲染透明物体
    RenderTransparentObjects();
}

void SandboxApp::RenderLamps() {
    glm::mat4 view = m_Camera.GetViewMatrix();
    float aspectRatio = GetWindow().GetAspectRatio();
    glm::mat4 projection = m_Camera.GetProjectionMatrix(aspectRatio);

    m_LampShader->Bind();
    m_LampShader->SetMat4("view", view);
    m_LampShader->SetMat4("projection", projection);

    m_CubeVAO->Bind();

    for (int i = 0; i < 4; ++i) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, m_PointLightPositions[i]);
        model = glm::scale(model, glm::vec3(0.2f));
        m_LampShader->SetMat4("model", model);
        m_LampShader->SetVec3("lightColor", m_LightColor);

        RenderCommand::DrawArrays(GL_TRIANGLES, 0, 36);
    }

    m_CubeVAO->Unbind();
}

void SandboxApp::RenderCubes() {
    glm::mat4 view = m_Camera.GetViewMatrix();
    float aspectRatio = GetWindow().GetAspectRatio();
    glm::mat4 projection = m_Camera.GetProjectionMatrix(aspectRatio);

    m_LightShader->Bind();

    m_DiffuseMap->Bind(0);
    m_LightShader->SetInt("material.diffuse", 0);

    m_SpecularMap->Bind(1);
    m_LightShader->SetInt("material.specular", 1);

    // 设置材质属性
    m_LightShader->SetFloat("material.shininess", m_MaterialShininess);

    // 设置方向光
    m_LightShader->SetVec3("dirLight.direction", m_DirLight.Direction);
    m_LightShader->SetVec3("dirLight.ambient", m_LightAmbient);
    m_LightShader->SetVec3("dirLight.diffuse", m_LightDiffuse);
    m_LightShader->SetVec3("dirLight.specular", m_LightSpecular);

    // 设置点光源
    for (int i = 0; i < 4; ++i) {
        std::string base = "pointLight[" + std::to_string(i) + "]";
        m_LightShader->SetVec3(base + ".position", m_PointLightPositions[i]);
        m_LightShader->SetVec3(base + ".ambient", m_LightAmbient);
        m_LightShader->SetVec3(base + ".diffuse", glm::vec3(0.8f));
        m_LightShader->SetVec3(base + ".specular", glm::vec3(1.0f));
        m_LightShader->SetFloat(base + ".constant", 1.0f);
        m_LightShader->SetFloat(base + ".linear", 0.09f);
        m_LightShader->SetFloat(base + ".quadratic", 0.032f);
    }

    // 设置聚光灯
    glm::vec3 camPos = m_Camera.GetPosition();
    glm::vec3 camFront = m_Camera.GetFront();
    m_LightShader->SetVec3("spotLight.position", camPos);
    m_LightShader->SetVec3("spotLight.direction", camFront);
    m_LightShader->SetVec3("spotLight.ambient", glm::vec3(0.0f));
    m_LightShader->SetVec3("spotLight.diffuse", glm::vec3(1.0f));
    m_LightShader->SetVec3("spotLight.specular", glm::vec3(1.0f));
    m_LightShader->SetFloat("spotLight.constant", 1.0f);
    m_LightShader->SetFloat("spotLight.linear", 0.09f);
    m_LightShader->SetFloat("spotLight.quadratic", 0.032f);
    m_LightShader->SetFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
    m_LightShader->SetFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));

    // 设置观察位置
    m_LightShader->SetVec3("viewPos", camPos);

    // 设置视图和投影矩阵
    m_LightShader->SetMat4("view", view);
    m_LightShader->SetMat4("projection", projection);

    m_CubeVAO->Bind();

    // 绘制立方体（跳过第一个，留给轮廓渲染）
    for (unsigned int i = 1; i < 10; ++i) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, m_CubePositions[i]);
        float angle = 20.0f * i;
        model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));

        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
        m_LightShader->SetMat3("normalMatrix", normalMatrix);
        m_LightShader->SetMat4("model", model);

        RenderCommand::DrawArrays(GL_TRIANGLES, 0, 36);
    }

    m_CubeVAO->Unbind();
}

void SandboxApp::RenderOutlinedCube() {
    glm::mat4 view = m_Camera.GetViewMatrix();
    float aspectRatio = GetWindow().GetAspectRatio();
    glm::mat4 projection = m_Camera.GetProjectionMatrix(aspectRatio);

    m_CubeVAO->Bind();

    // 启用模板测试
    RenderCommand::EnableStencilTest();
    RenderCommand::SetStencilMask(0xFF);

    // 第一遍：绘制立方体，写入模板缓冲
    RenderCommand::SetStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    RenderCommand::SetStencilFunc(GL_ALWAYS, 1, 0xFF);

    m_LightShader->Bind();
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, m_CubePositions[0]);
    glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
    m_LightShader->SetMat3("normalMatrix", normalMatrix);
    m_LightShader->SetMat4("model", model);
    RenderCommand::DrawArrays(GL_TRIANGLES, 0, 36);

    // 第二遍：绘制轮廓
    RenderCommand::SetStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    RenderCommand::SetStencilMask(0x00);
    RenderCommand::DisableDepthTest();

    m_SingleColorShader->Bind();
    m_SingleColorShader->SetMat4("view", view);
    m_SingleColorShader->SetMat4("projection", projection);
    model = glm::mat4(1.0f);
    model = glm::translate(model, m_CubePositions[0]);
    model = glm::scale(model, glm::vec3(1.02f));
    m_SingleColorShader->SetMat4("model", model);
    RenderCommand::DrawArrays(GL_TRIANGLES, 0, 36);

    // 恢复状态
    RenderCommand::SetStencilMask(0xFF);
    RenderCommand::SetStencilFunc(GL_ALWAYS, 0, 0xFF);
    RenderCommand::EnableDepthTest();

    m_CubeVAO->Unbind();
}

void SandboxApp::RenderTransparentObjects() {
    glm::mat4 view = m_Camera.GetViewMatrix();
    float aspectRatio = GetWindow().GetAspectRatio();
    glm::mat4 projection = m_Camera.GetProjectionMatrix(aspectRatio);

    m_TransparentShader->Bind();
    m_TransparentShader->SetMat4("projection", projection);
    m_TransparentShader->SetMat4("view", view);

    m_TransparentVAO->Bind();
    m_TransparentTexture->Bind(0);
    m_TransparentShader->SetInt("texture1", 0);

    // 按距离排序（从远到近）
    std::map<float, glm::vec3> sorted;
    glm::vec3 camPos = m_Camera.GetPosition();
    for (const auto& pos : m_WindowPositions) {
        float distance = glm::length(camPos - pos);
        sorted[distance] = pos;
    }

    // 从远到近绘制
    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, it->second);
        m_TransparentShader->SetMat4("model", model);
        RenderCommand::DrawArrays(GL_TRIANGLES, 0, 6);
    }

    m_TransparentVAO->Unbind();
}

void SandboxApp::RenderPostProcess() {
    // 绑定默认帧缓冲
    Framebuffer::BindDefault();
    RenderCommand::SetClearColor(glm::vec4(0.1f, 0.1f, 0.1f, 1.0f));
    RenderCommand::ClearColorBuffer();

    m_ScreenShader->Bind();
    m_ScreenQuadVAO->Bind();
    RenderCommand::DisableDepthTest();

    // 绑定场景纹理
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_SceneFBO->GetColorAttachment());
    m_ScreenShader->SetInt("screenTexture", 0);

    RenderCommand::DrawArrays(GL_TRIANGLES, 0, 6);

    RenderCommand::EnableDepthTest();
    m_ScreenQuadVAO->Unbind();
}

void SandboxApp::VisualizeDepthBuffer() {
    // 将深度缓冲可视化
    glBindTexture(GL_TEXTURE_2D, m_DepthFBO->GetDepthAttachment());
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                     0, 0, GetWindow().GetWidth(), GetWindow().GetHeight(), 0);

    RenderCommand::DisableDepthTest();

    m_DebugDepthShader->Bind();
    m_DebugDepthShader->SetFloat("near", m_Camera.GetNearPlane());
    m_DebugDepthShader->SetFloat("far", m_Camera.GetFarPlane());
    m_DebugDepthShader->SetInt("visualizationMode", 0);
    m_DebugDepthShader->SetInt("depthMap", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_DepthFBO->GetDepthAttachment());

    m_ScreenQuadVAO->Bind();
    RenderCommand::DrawArrays(GL_TRIANGLES, 0, 6);
    m_ScreenQuadVAO->Unbind();

    RenderCommand::EnableDepthTest();
}

void SandboxApp::ProcessInput(float deltaTime) {
    // ESC 退出
    if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) {
        Close();
    }

    // TAB 切换相机控制
    static bool tabWasPressed = false;
    bool tabPressed = Input::IsKeyPressed(GLFW_KEY_TAB);
    if (tabPressed && !tabWasPressed) {
        m_CameraControlEnabled = !m_CameraControlEnabled;

        if (m_CameraControlEnabled) {
            GetWindow().SetCursorMode(GLFW_CURSOR_DISABLED);
            Input::ResetFirstMouse();
        } else {
            GetWindow().SetCursorMode(GLFW_CURSOR_NORMAL);
        }
    }
    tabWasPressed = tabPressed;

    // 相机控制
    if (m_CameraControlEnabled) {
        // 检查 ImGui 是否捕获鼠标
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse) {
            // 键盘移动
            if (Input::IsKeyPressed(GLFW_KEY_W))
                m_Camera.ProcessKeyboard(FORWARD, deltaTime);
            if (Input::IsKeyPressed(GLFW_KEY_S))
                m_Camera.ProcessKeyboard(BACKWARD, deltaTime);
            if (Input::IsKeyPressed(GLFW_KEY_A))
                m_Camera.ProcessKeyboard(LEFT, deltaTime);
            if (Input::IsKeyPressed(GLFW_KEY_D))
                m_Camera.ProcessKeyboard(RIGHT, deltaTime);

            // 鼠标移动
            glm::vec2 mouseDelta = Input::GetMouseDelta();
            if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
                m_Camera.ProcessMouseMovement(mouseDelta.x, mouseDelta.y);
            }

            // 滚轮缩放
            float scroll = Input::GetScrollOffset();
            if (scroll != 0.0f) {
                m_Camera.ProcessMouseScroll(scroll);
            }
        }
    }
}

} // namespace GLRenderer
