#include "EditorApp.h"
#include "core/Input.h"
#include "physics/PhysicsSystem.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <string>

namespace GLRenderer {

// 注：立方体顶点数据已迁移到 PrimitiveData.h，通过 MeshFactory 获取

EditorApp::EditorApp()
    : Application(WindowProps("GLRenderer Editor", 1920, 1080))
{
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
    Renderer2D::Init();

    // 初始化 ImGui
    m_ImGuiLayer = std::make_unique<ImGuiLayer>(GetWindow().GetNativeWindow());
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-Medium.ttf", 16.0f);

    auto& colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_TitleBgActive] = ImGui::ColorConvertU32ToFloat4(IM_COL32(186, 66, 30, 255));
    colors[ImGuiCol_Tab]                = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
    colors[ImGuiCol_TabHovered]         = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
    colors[ImGuiCol_TabActive]          = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
    colors[ImGuiCol_TabUnfocused]       = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

    // 初始化资产管理器
    AssetManager::Get().Initialize("assets");

    // 创建网格资源
    std::cout << "[EditorApp] Creating mesh resources..." << std::endl;
    CreateMeshResources();

    // 加载纹理 (通过 AssetManager)
    std::cout << "[EditorApp] Loading textures..." << std::endl;
    m_DiffuseMapHandle = AssetManager::Get().ImportTexture("assets/pic/container2.png");
    m_SpecularMapHandle = AssetManager::Get().ImportTexture("assets/pic/container2_specular.png");
    m_SkyboxTexture = Ref<TextureCube>::Create(m_SkyboxPaths);

    // 初始化 ECS
    std::cout << "[EditorApp] Initializing ECS..." << std::endl;
    m_World = std::make_unique<ECS::World>("EditorWorld");
    m_SystemManager = std::make_unique<ECS::SystemManager>();
    m_SystemManager->AddSystem<ECS::TransformSystem>();
    m_SystemManager->AddSystem<ECS::BehaviorSystem>();
    m_SystemManager->AddSystem<PhysicsSystem>();

    // 创建场景实体
    std::cout << "[EditorApp] Creating scene entities..." << std::endl;
    CreateSceneEntities();

    // 初始化场景渲染器
    std::cout << "[EditorApp] Initializing scene renderer..." << std::endl;
    m_SceneRenderer = std::make_unique<SceneRenderer>();
    m_SceneRenderer->Initialize();
    m_SceneRenderer->LoadShaders("assets/shaders");

    // 加载编辑器图标 (由 EditorApp 管理，不再由 SceneRenderer)
    LoadEditorIcons();

    // 设置默认纹理 (白色纹理用于 Primitives)
    m_SceneRenderer->SetDefaultDiffuse(Renderer::GetWhiteTexture());
    m_SceneRenderer->SetDefaultSpecular(Renderer::GetWhiteTexture());

    // 启动时加载默认 HDR 环境 (IBL 天空盒), 优先使用 puresky
    {
        const char* defaultHDRs[] = {
            "assets/EnvironmentMaps/puresky_4k.hdr",
            "assets/EnvironmentMaps/meadow_2_2k.hdr",
        };
        for (const char* hdrPath : defaultHDRs) {
            if (std::filesystem::exists(hdrPath)) {
                std::cout << "[EditorApp] 加载默认环境: " << hdrPath << std::endl;
                m_SceneRenderer->LoadEnvironment(hdrPath);
                break;
            }
        }
    }

    // 初始化编辑器
    std::cout << "[EditorApp] Initializing editor..." << std::endl;
    m_Editor = std::make_unique<Editor>();
    m_Editor->Initialize(m_World.get());

    // 注入场景 I/O 回调
    {
        auto& ctx = m_Editor->GetContext();
        ctx.OnNewScene    = [this]() { NewScene(); };
        ctx.OnSaveScene   = [this]() { SaveScene(); };
        ctx.OnSaveSceneAs = [this]() { SaveSceneAs(); };
        ctx.OnOpenScene   = [this]() { OpenScene(); };
        ctx.OnEnterPlay   = [this]() { EnterPlayMode(); };
        ctx.OnExitPlay    = [this]() { ExitPlayMode(); };
    }

    // 添加视口面板
    auto viewportPanel = std::make_unique<ViewportPanel>();
    m_ViewportPanel = viewportPanel.get();
    m_Editor->AddPanel(std::move(viewportPanel));

    // 添加设置面板
    auto settingsPanel = std::make_unique<SettingsPanel>();
    settingsPanel->SetSceneRenderer(m_SceneRenderer.get());
    m_Editor->AddPanel(std::move(settingsPanel));

    // 添加几何体创建面板
    auto primitivesPanel = std::make_unique<PrimitivesPanel>();
    m_Editor->AddPanel(std::move(primitivesPanel));

    std::cout << "[EditorApp] Initialization complete" << std::endl;
}

void EditorApp::OnShutdown() {
    std::cout << "[EditorApp] Shutting down..." << std::endl;

    m_Editor.reset();
    m_SceneRenderer.reset();
    m_SystemManager.reset();
    m_World.reset();

    // 编辑器图标和 SkyboxTexture 通过 Ref<T> 自动释放
    // 注：m_DiffuseMapHandle 和 m_SpecularMapHandle 由 AssetManager 管理
    // 注：MeshFactory 创建的网格由 AssetManager 管理

    AssetManager::Get().Shutdown();
    m_ImGuiLayer.reset();

    Renderer2D::Shutdown();
    Renderer::Shutdown();

    std::cout << "[EditorApp] Shutdown complete" << std::endl;
}

void EditorApp::NewScene()
{
    if (IsPlaying()) return;
    m_Editor->GetContext().ClearSelection();
    m_World->Clear();
    m_CurrentScenePath.clear();
    std::cout << "[EditorApp] 新建场景" << std::endl;
}

void EditorApp::SaveScene()
{
    if (IsPlaying()) return;
    if (m_CurrentScenePath.empty()) {
        m_ShowSaveAsDialog = true;
        return;
    }
    SceneSerializer::Serialize(*m_World, m_CurrentScenePath);
}

void EditorApp::SaveSceneAs()
{
    if (IsPlaying()) return;
    m_ShowSaveAsDialog = true;
}

void EditorApp::OpenScene(const std::filesystem::path& path)
{
    if (IsPlaying()) return;
    std::filesystem::path target = path;
    if (target.empty()) {
        target = "assets/scenes/scene.glscene";
    }
    if (!std::filesystem::exists(target)) {
        std::cerr << "[EditorApp] 场景文件不存在: " << target << std::endl;
        return;
    }
    m_Editor->GetContext().ClearSelection();
    if (SceneSerializer::Deserialize(*m_World, target))
        m_CurrentScenePath = target;
}

void EditorApp::EnterPlayMode()
{
    if (IsPlaying()) return;
    m_PlayWorldSnapshot = SceneSerializer::SerializeToString(*m_World);
    m_PlayWorld = std::make_unique<ECS::World>("PlayWorld");
    SceneSerializer::DeserializeFromString(*m_PlayWorld, m_PlayWorldSnapshot);
    m_Editor->GetContext().IsPlaying = true;
    m_Editor->GetContext().IsPaused  = false;
    m_Editor->GetContext().ClearSelection();
    std::cout << "[EditorApp] Enter Play Mode" << std::endl;
}

void EditorApp::ExitPlayMode()
{
    if (!IsPlaying()) return;

    // 必须先将 Context 切回编辑器世界，再销毁 PlayWorld。
    // 否则当前帧内后续的 ImGui 面板仍会通过 Context.World 访问已释放的 PlayWorld（悬空指针）。
    m_Editor->GetContext().IsPlaying = false;
    m_Editor->GetContext().IsPaused  = false;
    m_Editor->GetContext().ClearSelection();
    m_Editor->GetContext().World = m_World.get();

    m_PlayWorld.reset();
    m_PlayWorldSnapshot.clear();

    std::cout << "[EditorApp] Exit Play Mode" << std::endl;
}

void EditorApp::OnUpdate(float deltaTime) {
    m_LastDeltaTime = deltaTime;

    // 全局键盘快捷键
    bool ctrl = Input::IsKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
                Input::IsKeyPressed(GLFW_KEY_RIGHT_CONTROL);
    if (ctrl) {
        if (Input::IsKeyJustPressed(GLFW_KEY_N)) NewScene();
        if (Input::IsKeyJustPressed(GLFW_KEY_S)) SaveScene();
        if (Input::IsKeyJustPressed(GLFW_KEY_O)) OpenScene();
    }


    // 确定当前活动世界并同步给 Editor
    ECS::World& activeWorld = IsPlaying() ? *m_PlayWorld : *m_World;
    if (m_Editor) m_Editor->GetContext().World = &activeWorld;

    // 更新 ECS
    // Edit 模式：只跑 TransformSystem（更新世界矩阵），行为/物理系统冻结
    // Play 模式：跑全部系统，Pause 时暂停
    if (m_SystemManager) {
        const bool paused = m_Editor->GetContext().IsPaused;
        if (IsPlaying()) {
            if (!paused)
                m_SystemManager->Update(activeWorld, deltaTime);
        } else {
            if (auto* ts = m_SystemManager->GetSystem<ECS::TransformSystem>())
                ts->Update(activeWorld, deltaTime);
        }
    }

    // 更新编辑器
    if (m_Editor) {
        m_Editor->OnUpdate(deltaTime);
    }

    // 处理视口相机输入
    if (m_ViewportPanel) {
        m_ViewportPanel->ProcessCameraInput(deltaTime);

        // Play 中禁用鼠标拾取（运行时不做实体选择）
        if (!IsPlaying()) {
            int pickedEntityID = -1;
            bool hasNewPick = m_ViewportPanel->ProcessMousePicking(pickedEntityID);

            if (hasNewPick) {
                if (pickedEntityID >= 0 && m_World) {
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
                    m_Editor->GetContext().ClearSelection();
                }
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

    SetupDockSpace();

    if (m_Editor)
        m_Editor->OnImGuiRender();

    // Save As 弹窗
    if (m_ShowSaveAsDialog) {
        ImGui::OpenPopup("Save Scene As");
        m_ShowSaveAsDialog = false;
    }
    if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Scene Path:");
        ImGui::SetNextItemWidth(400.0f);
        ImGui::InputText("##savepath", m_SaveAsPathBuf, sizeof(m_SaveAsPathBuf));
        ImGui::Spacing();
        if (ImGui::Button("Save", ImVec2(120, 0))) {
            m_CurrentScenePath = m_SaveAsPathBuf;
            SceneSerializer::Serialize(*m_World, m_CurrentScenePath);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    m_ImGuiLayer->End();
}




void EditorApp::SetupDockSpace() {
    // 设置全屏 DockSpace
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

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

    ImGui::End();
}

void EditorApp::RenderSceneToViewport() {
    if (!m_ViewportPanel || !m_SceneRenderer) return;

    Ref<Framebuffer> fbo = m_ViewportPanel->GetFramebuffer();
    if (!fbo || !fbo->IsValid()) return;

    // 处理视口大小变化
    if (m_ViewportPanel->GetSize().x != fbo->GetWidth() ||
        m_ViewportPanel->GetSize().y != fbo->GetHeight()) {
        m_ViewportPanel->OnResize(m_ViewportPanel->GetSize().x, m_ViewportPanel->GetSize().y);
    }

    Camera& camera = m_ViewportPanel->GetCamera();
    float aspectRatio = m_ViewportPanel->GetAspectRatio();

    // 传递选中实体（用于描边）
    {
        int selectedID = -1;
        if (m_Editor->GetContext().HasSelection())
            selectedID = static_cast<int>(
                m_Editor->GetContext().SelectedEntity.GetHandle());
        m_SceneRenderer->SetSelectedEntityID(selectedID);
    }

    // 渲染场景：HDR FBO → CompositePass（同时复制 EntityID）→ viewport FBO
    ECS::World& renderWorld = IsPlaying() ? *m_PlayWorld : *m_World;
    m_SceneRenderer->RenderScene(renderWorld, *fbo, camera, aspectRatio, m_LastDeltaTime);

    // 渲染编辑器 overlay（billboard），EntityID 覆盖写入 viewport FBO attachment 1
    // picking 直接读 viewport FBO，同时包含几何体和 billboard 的 EntityID
    RenderEditorVisuals();
}

void EditorApp::CreateMeshResources() {
    // MeshFactory 现在是纯静态 API，无需初始化
    // 基元几何体通过 MeshFactory::CreateXXX() 按需创建
}

void EditorApp::CreateSceneEntities() {
    // 创建共享立方体网格
    AssetHandle cubeMeshHandle = MeshFactory::CreateCube();

    // 立方体位置
    glm::vec3 cubePositions[] = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(2.0f, 5.0f, -15.0f),
        glm::vec3(-1.5f, -2.2f, -2.5f),
        glm::vec3(-3.8f, -2.0f, -12.3f),
        glm::vec3(2.4f, -0.4f, -3.5f),
        glm::vec3(-1.7f, 3.0f, -7.5f),
        glm::vec3(1.3f, -2.0f, -2.5f),
        glm::vec3(1.5f, 2.0f, -2.5f),
        glm::vec3(1.5f, 0.2f, -1.5f),
        glm::vec3(-1.3f, 1.0f, -1.5f)
    };

    // 创建立方体实体（带 MeshComponent）
    for (int i = 0; i < 10; i++) {
        std::string name = "Cube_" + std::to_string(i);
        ECS::Entity cube = m_World->CreateEntity(name);

        // Transform - 设置位置和旋转
        auto& transform = cube.AddComponent<ECS::TransformComponent>(cubePositions[i]);
        float angle = 20.0f * i;
        transform.SetEulerAngles(glm::vec3(angle * 1.0f, angle * 0.3f, angle * 0.5f));

        // Mesh - 引用共享的立方体 (通过 AssetHandle)
        // 材质通过 MeshComponent.Materials 或 StaticMesh 默认材质处理
        cube.AddComponent<ECS::MeshComponent>(cubeMeshHandle);
    }

    // 点光源位置
    glm::vec3 pointLightPositions[] = {
        glm::vec3(0.7f, 0.2f, 2.0f),
        glm::vec3(2.3f, -3.3f, -4.0f),
        glm::vec3(-4.0f, 2.0f, -12.0f),
        glm::vec3(0.0f, 0.0f, -3.0f)
    };

    // 创建点光源实体
    for (int i = 0; i < 4; i++) {
        std::string name = "PointLight_" + std::to_string(i);
        ECS::Entity light = m_World->CreateEntity(name);

        light.AddComponent<ECS::TransformComponent>(pointLightPositions[i]);

        auto& pointLight = light.AddComponent<ECS::PointLightComponent>();
        pointLight.Color = glm::vec3(1.0f);
        pointLight.Intensity = 1.0f;
        pointLight.Constant = 1.0f;
        pointLight.Linear = 0.09f;
        pointLight.Quadratic = 0.032f;
    }

    // 创建带行为的示例实体
    {
        ECS::Entity rotatingCube = m_World->CreateEntity("RotatingCube");
        rotatingCube.AddComponent<ECS::TransformComponent>(glm::vec3(0.0f, 2.0f, 0.0f));
        rotatingCube.AddComponent<ECS::MeshComponent>(cubeMeshHandle);
        rotatingCube.AddComponent<ECS::RotatorComponent>(glm::vec3(0, 1, 0), 45.0f);

        ECS::Entity floatingCube = m_World->CreateEntity("FloatingCube");
        auto& floatTransform = floatingCube.AddComponent<ECS::TransformComponent>(glm::vec3(3.0f, 1.0f, 0.0f));
        floatingCube.AddComponent<ECS::MeshComponent>(cubeMeshHandle);
        auto& floating = floatingCube.AddComponent<ECS::FloatingComponent>(1.0f, 0.5f);
        floating.BasePosition = floatTransform.Position;

        ECS::Entity orbitingCube = m_World->CreateEntity("OrbitingCube");
        orbitingCube.AddComponent<ECS::TransformComponent>(glm::vec3(0.0f, 1.0f, 3.0f));
        orbitingCube.AddComponent<ECS::MeshComponent>(cubeMeshHandle);
        orbitingCube.AddComponent<ECS::OrbiterComponent>(glm::vec3(0.0f, 1.0f, 0.0f), 3.0f, 30.0f);
    }

    std::cout << "[EditorApp] Created " << m_World->GetEntityCount() << " entities" << std::endl;
}

void EditorApp::OnWindowResize(int width, int height) {
    // 窗口大小变化由 ImGui 视口自行处理
}

void EditorApp::LoadEditorIcons() {
    std::filesystem::path iconsDir = "assets/editor/Icons";

    auto pointLightPath = iconsDir / "PointLight.png";
    auto dirLightPath = iconsDir / "DirectionalLight.png";
    auto spotLightPath = iconsDir / "SpotLight.png";
    auto cameraPath = iconsDir / "Camera.png";

    if (std::filesystem::exists(pointLightPath)) {
        m_PointLightIcon = Ref<Texture2D>::Create(pointLightPath.string());
        std::cout << "[EditorApp] Loaded PointLight icon" << std::endl;
    }
    if (std::filesystem::exists(dirLightPath)) {
        m_DirectionalLightIcon = Ref<Texture2D>::Create(dirLightPath.string());
        std::cout << "[EditorApp] Loaded DirectionalLight icon" << std::endl;
    }
    if (std::filesystem::exists(spotLightPath)) {
        m_SpotLightIcon = Ref<Texture2D>::Create(spotLightPath.string());
        std::cout << "[EditorApp] Loaded SpotLight icon" << std::endl;
    }
    if (std::filesystem::exists(cameraPath)) {
        m_CameraIcon = Ref<Texture2D>::Create(cameraPath.string());
        std::cout << "[EditorApp] Loaded Camera icon" << std::endl;
    }
}

void EditorApp::RenderEditorVisuals() {
    if (!m_ViewportPanel || !m_World) return;
    ECS::World& activeWorld = IsPlaying() ? *m_PlayWorld : *m_World;

    Ref<Framebuffer> fbo = m_ViewportPanel->GetFramebuffer();
    if (!fbo || !fbo->IsValid()) return;

    Camera& camera = m_ViewportPanel->GetCamera();
    float aspectRatio = m_ViewportPanel->GetAspectRatio();

    // 将 HDR FBO 深度 blit 到 viewport FBO，使 billboard 受场景几何体遮挡
    m_SceneRenderer->CopyDepthToTarget(*fbo);

    fbo->Bind();
    glViewport(0, 0, fbo->GetWidth(), fbo->GetHeight());

    // 使用 Renderer2D 渲染编辑器可视化
    Renderer2D::BeginScene(camera, aspectRatio);

    // 渲染点光源图标
    if (m_PointLightIcon) {
        activeWorld.Each<ECS::TransformComponent, ECS::PointLightComponent>(
            [this](ECS::Entity entity, ECS::TransformComponent& transform, ECS::PointLightComponent& light) {
                Renderer2D::DrawBillboard(
                    transform.Position,
                    glm::vec2(0.5f),
                    m_PointLightIcon,
                    glm::vec4(1.0f),
                    static_cast<int>(entity.GetHandle())
                );
            }
        );
    }

    // 渲染方向光图标和方向箭头
    activeWorld.Each<ECS::TransformComponent, ECS::DirectionalLightComponent>(
        [this](ECS::Entity entity, ECS::TransformComponent& transform, ECS::DirectionalLightComponent& light) {
            if (m_DirectionalLightIcon) {
                Renderer2D::DrawBillboard(
                    transform.Position,
                    glm::vec2(0.5f),
                    m_DirectionalLightIcon,
                    glm::vec4(1.0f),
                    static_cast<int>(entity.GetHandle())
                );
            }

            Renderer2D::DrawDirectionArrow(
                transform.Position,
                transform.GetForward(),
                1.5f,
                glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
                3.0f
            );
        }
    );

    // 渲染聚光灯图标
    if (m_SpotLightIcon) {
        activeWorld.Each<ECS::TransformComponent, ECS::SpotLightComponent>(
            [this](ECS::Entity entity, ECS::TransformComponent& transform, ECS::SpotLightComponent& light) {
                Renderer2D::DrawBillboard(
                    transform.Position,
                    glm::vec2(0.5f),
                    m_SpotLightIcon,
                    glm::vec4(1.0f),
                    static_cast<int>(entity.GetHandle())
                );
            }
        );
    }

    // 渲染 SpriteComponent 实体（游戏中的 Billboard/Sprite）
    activeWorld.Each<ECS::TransformComponent, ECS::SpriteComponent>(
        [](ECS::Entity entity, ECS::TransformComponent& transform, ECS::SpriteComponent& sprite) {
            Ref<Texture2D> tex;
            if (sprite.TextureHandle.IsValid()) {
                tex = AssetManager::Get().GetAsset<Texture2D>(sprite.TextureHandle);
            }
            if (!tex) return;

            Renderer2D::DrawBillboard(
                transform.Position,
                sprite.Size,
                tex,
                sprite.Color,
                static_cast<int>(entity.GetHandle())
            );
        }
    );

    // 已选中实体有 FluidComponent 时，绘制仿真域边框
    if (m_Editor) {
        ECS::Entity sel = m_Editor->GetContext().SelectedEntity;
        if (sel.IsValid() && sel.HasComponent<ECS::FluidComponent>()) {
            const auto& f = sel.GetComponent<ECS::FluidComponent>();

            // 编辑模式下 PhysicsSystem 未运行，直接从 Transform 计算边框
            glm::vec3 lo = f.BoundaryMin;
            glm::vec3 hi = f.BoundaryMax;
            if (sel.HasComponent<ECS::TransformComponent>()) {
                const auto& tr = sel.GetComponent<ECS::TransformComponent>();
                const float hw = tr.Scale.x * 0.5f;
                const float hd = tr.Scale.z * 0.5f;
                lo = { tr.Position.x - hw, tr.Position.y,              tr.Position.z - hd };
                hi = { tr.Position.x + hw, tr.Position.y + tr.Scale.y, tr.Position.z + hd };
            }
            const glm::vec4 boxColor(0.2f, 0.8f, 1.0f, 0.9f);
            const float lw = 1.5f;

            // 底面
            Renderer2D::DrawLine({lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, boxColor, lw);
            Renderer2D::DrawLine({hi.x, lo.y, lo.z}, {hi.x, lo.y, hi.z}, boxColor, lw);
            Renderer2D::DrawLine({hi.x, lo.y, hi.z}, {lo.x, lo.y, hi.z}, boxColor, lw);
            Renderer2D::DrawLine({lo.x, lo.y, hi.z}, {lo.x, lo.y, lo.z}, boxColor, lw);
            // 顶面
            Renderer2D::DrawLine({lo.x, hi.y, lo.z}, {hi.x, hi.y, lo.z}, boxColor, lw);
            Renderer2D::DrawLine({hi.x, hi.y, lo.z}, {hi.x, hi.y, hi.z}, boxColor, lw);
            Renderer2D::DrawLine({hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}, boxColor, lw);
            Renderer2D::DrawLine({lo.x, hi.y, hi.z}, {lo.x, hi.y, lo.z}, boxColor, lw);
            // 四条竖边
            Renderer2D::DrawLine({lo.x, lo.y, lo.z}, {lo.x, hi.y, lo.z}, boxColor, lw);
            Renderer2D::DrawLine({hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, boxColor, lw);
            Renderer2D::DrawLine({hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, boxColor, lw);
            Renderer2D::DrawLine({lo.x, lo.y, hi.z}, {lo.x, hi.y, hi.z}, boxColor, lw);
        }
    }

    Renderer2D::EndScene();

    fbo->Unbind();
}

} // namespace GLRenderer
