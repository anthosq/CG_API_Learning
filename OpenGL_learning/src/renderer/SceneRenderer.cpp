#include "SceneRenderer.h"
#include "RenderCommand.h"
#include "asset/AssetManager.h"
#include <algorithm>

namespace GLRenderer {

// 注：立方体顶点数据已迁移到 PrimitiveData.h，通过 MeshFactory 获取

SceneRenderer::SceneRenderer() = default;

SceneRenderer::~SceneRenderer() {
    Shutdown();
}

void SceneRenderer::Initialize() {
    if (m_Initialized) return;

    std::cout << "[SceneRenderer] Initializing..." << std::endl;

    CreateDefaultResources();

    // 设置默认光照
    m_Environment.DirLight.Direction = glm::vec3(-0.2f, -1.0f, -0.3f);
    m_Environment.DirLight.Ambient = glm::vec3(0.05f);
    m_Environment.DirLight.Diffuse = glm::vec3(0.4f);
    m_Environment.DirLight.Specular = glm::vec3(0.5f);

    // 创建 Pipeline
    m_OpaquePipeline = std::make_unique<Pipeline>(PipelineSpecification::Opaque());
    m_TransparentPipeline = std::make_unique<Pipeline>(PipelineSpecification::Transparent());

    // 创建 UBO
    m_CameraUBO = std::make_unique<UniformBuffer>(static_cast<uint32_t>(sizeof(CameraUBO)), UBO_BINDING_CAMERA);
    m_LightingUBO = std::make_unique<UniformBuffer>(static_cast<uint32_t>(sizeof(LightingUBO)), UBO_BINDING_LIGHTING);

    m_Initialized = true;
    std::cout << "[SceneRenderer] Initialized" << std::endl;
}

void SceneRenderer::Shutdown() {
    if (!m_Initialized) return;

    m_Grid.reset();
    // 注：立方体由 MeshFactory 管理，无需手动释放
    m_CameraUBO.reset();
    m_LightingUBO.reset();
    m_ShaderLibrary.Clear();

    m_Initialized = false;
    std::cout << "[SceneRenderer] Shutdown" << std::endl;
}

void SceneRenderer::LoadShaders(const std::filesystem::path& shaderDir) {
    std::cout << "[SceneRenderer] Loading shaders from: " << shaderDir << std::endl;

    // 加载统一格式着色器
    for (const auto& entry : std::filesystem::directory_iterator(shaderDir)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            // 只加载 .glsl 文件（统一格式）
            if (ext == ".glsl") {
                m_ShaderLibrary.Load(entry.path());
            }
        }
    }

    // 为所有着色器绑定 UBO
    for (const auto& [name, shader] : m_ShaderLibrary.GetShaders()) {
        if (shader && shader->IsValid()) {
            shader->BindUniformBlock("CameraData", UBO_BINDING_CAMERA);
            shader->BindUniformBlock("LightingData", UBO_BINDING_LIGHTING);
        }
    }
}

void SceneRenderer::RenderScene(ECS::World& world,
                                 Framebuffer& targetFBO,
                                 const Camera& camera,
                                 float aspectRatio) {
    if (!m_Initialized) {
        std::cerr << "[SceneRenderer] Not initialized!" << std::endl;
        return;
    }

    // 设置目标 Framebuffer
    m_TargetFramebuffer = &targetFBO;

    // 同步 ECS 点光源到环境
    SyncLightsFromECS(world);

    // === 新渲染流程 ===
    // 1. 开始场景 - 清空绘制列表，缓存相机数据
    BeginScene(camera, aspectRatio);

    // 2. 收集绘制命令
    CollectDrawCommandsFromECS(world);

    // 3. 结束场景
    EndScene();

    // 4. 执行绘制列表
    FlushDrawList();
}

void SceneRenderer::BeginScene(const Camera& camera, float aspectRatio) {
    // 清空绘制列表
    m_OpaqueDrawList.clear();
    m_TransparentDrawList.clear();
    m_LightEntities.clear();

    // 缓存相机数据
    m_ViewMatrix = camera.GetViewMatrix();
    m_ProjectionMatrix = camera.GetProjectionMatrix(aspectRatio);
    m_CameraPosition = camera.GetPosition();

    if (m_CameraUBO) {
        CameraUBO cameraData;
        cameraData.View = m_ViewMatrix;
        cameraData.Projection = m_ProjectionMatrix;
        cameraData.ViewProjection = m_ProjectionMatrix * m_ViewMatrix;
        cameraData.Position = glm::vec4(m_CameraPosition, 1.0f);
        m_CameraUBO->SetData(&cameraData, sizeof(cameraData));
    }

    if (m_LightingUBO) {
        LightingUBO lightData = {};

        // 方向光
        lightData.DirLightDirection = glm::vec4(m_Environment.DirLight.Direction, 1.0f);
        lightData.DirLightColor = glm::vec4(m_Environment.DirLight.Diffuse, 1.0f);

        // 点光源
        lightData.LightCounts.x = m_Environment.PointLightCount;
        for (int i = 0; i < m_Environment.PointLightCount && i < MAX_POINT_LIGHTS; ++i) {
            const auto& light = m_Environment.PointLights[i];
            lightData.PointLightPositions[i] = glm::vec4(light.Position, 1.0f);
            lightData.PointLightColors[i] = glm::vec4(light.Diffuse, 1.0f);
            lightData.PointLightParams[i] = glm::vec4(
                light.Constant, light.Linear, light.Quadratic, 0.0f
            );
        }

        // 环境光
        lightData.AmbientColor = glm::vec4(m_Settings.AmbientColor, 1.0f);

        m_LightingUBO->SetData(&lightData, sizeof(lightData));
    }

    Renderer::BeginFrame(camera, aspectRatio);
}

void SceneRenderer::EndScene() {
    Renderer::EndFrame();
}


void SceneRenderer::SubmitMesh(VertexArray* mesh, uint32_t vertexCount,
                                const glm::mat4& transform, int entityID) {
    SubmitMesh(mesh, vertexCount, 0, transform, entityID);
}

void SceneRenderer::SubmitMesh(VertexArray* mesh, uint32_t vertexCount, uint32_t indexCount,
                                const glm::mat4& transform, int entityID) {
    if (!mesh) return;

    DrawCommand cmd;
    cmd.Type = DrawCommandType::Mesh;
    cmd.MeshPtr = mesh;
    cmd.VertexCount = vertexCount;
    cmd.IndexCount = indexCount;
    cmd.UseIndices = (indexCount > 0);
    cmd.Transform = transform;
    cmd.NormalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
    cmd.EntityID = entityID;

    // TODO: 判断是否透明（目前全部作为不透明处理）
    m_OpaqueDrawList.push_back(cmd);
}

void SceneRenderer::SubmitModel(Model* model, const glm::mat4& transform,
                                 bool hasDiffuse, bool hasSpecular, int entityID) {
    if (!model) return;

    DrawCommand cmd;
    cmd.Type = DrawCommandType::Model;
    cmd.ModelPtr = model;
    cmd.Transform = transform;
    cmd.NormalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
    cmd.HasDiffuseTexture = hasDiffuse;
    cmd.HasSpecularTexture = hasSpecular;
    cmd.EntityID = entityID;

    // 计算到相机的距离（用于透明排序）
    glm::vec3 pos = glm::vec3(transform[3]);
    cmd.DistanceToCamera = glm::distance(m_CameraPosition, pos);

    // TODO: 根据材质判断透明度，目前全部作为不透明
    m_OpaqueDrawList.push_back(cmd);
}

void SceneRenderer::SortTransparentDrawList() {
    // 按距离从远到近排序（透明物体需要从后往前渲染）
    std::sort(m_TransparentDrawList.begin(), m_TransparentDrawList.end());
}

void SceneRenderer::CollectDrawCommandsFromECS(ECS::World& world) {
    // 收集 ModelComponent 实体
    world.Each<ECS::TransformComponent, ECS::ModelComponent>(
        [this](ECS::Entity entity,
               ECS::TransformComponent& transform,
               ECS::ModelComponent& modelComp) {
            if (!modelComp.Visible) return;

            Model* model = AssetManager::Get().GetModel(modelComp.Handle);
            if (!model) return;

            bool hasDiffuse = model->HasDiffuseTextures();
            bool hasSpecular = model->HasSpecularTextures();

            SubmitModel(model, transform.WorldMatrix, hasDiffuse, hasSpecular,
                       static_cast<int>(entity.GetHandle()));
        }
    );

    // 收集 MeshComponent 实体
    world.Each<ECS::TransformComponent, ECS::MeshComponent>(
        [this](ECS::Entity entity,
               ECS::TransformComponent& transform,
               ECS::MeshComponent& mesh) {
            if (!mesh.VAO) return;

            if (mesh.UseIndices && mesh.IndexCount > 0) {
                SubmitMesh(mesh.VAO, mesh.VertexCount, mesh.IndexCount,
                          transform.WorldMatrix, static_cast<int>(entity.GetHandle()));
            } else {
                SubmitMesh(mesh.VAO, mesh.VertexCount, transform.WorldMatrix,
                          static_cast<int>(entity.GetHandle()));
            }
        }
    );

    // 收集点光源实体（用于光照计算，可视化由 EditorApp 处理）
    world.Each<ECS::TransformComponent, ECS::PointLightComponent>(
        [this](ECS::Entity entity,
               ECS::TransformComponent& transform,
               ECS::PointLightComponent& light) {
            LightEntityInfo info;
            info.Position = transform.Position;
            info.Color = light.Color * light.Intensity;
            info.EntityID = static_cast<int>(entity.GetHandle());
            info.LightType = 0;  // Point Light
            m_LightEntities.push_back(info);
        }
    );

    // 注：光源和 SpriteComponent 的可视化由 EditorApp 通过 Renderer2D 处理
}

// === Pass 方法 ===

void SceneRenderer::FlushDrawList() {
    if (!m_TargetFramebuffer) {
        std::cerr << "[SceneRenderer] No target framebuffer set!" << std::endl;
        return;
    }

    // 绑定目标 FBO
    m_TargetFramebuffer->Bind();
    RenderCommand::SetViewport(0, 0, m_TargetFramebuffer->GetWidth(),
                                      m_TargetFramebuffer->GetHeight());
    RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    RenderCommand::Clear();
    RenderCommand::EnableDepthTest();

    // 清除实体 ID 附件
    m_TargetFramebuffer->ClearEntityID(-1);

    // 1. 网格 Pass
    if (m_Settings.ShowGrid) {
        GridPass();
    }

    // 2. 不透明几何 Pass
    GeometryPass();

    // 3. 透明物体 Pass
    if (!m_TransparentDrawList.empty()) {
        SortTransparentDrawList();
        TransparentPass();
    }

    // 4. 天空盒 Pass（深度 LEQUAL）
    if (m_Settings.ShowSkybox && m_Environment.Skybox) {
        SkyboxPass();
    }

    // 注：Billboard/Sprite 由 EditorApp::RenderEditorVisuals() 通过 Renderer2D 渲染

    // 解绑 FBO
    m_TargetFramebuffer->Unbind();
}

void SceneRenderer::GridPass() {
    if (!m_Grid) return;

    auto gridShader = m_ShaderLibrary.Get("grid");
    if (!gridShader) return;

    m_Grid->SetGridSize(m_Settings.GridSize);
    m_Grid->SetGridCellSize(m_Settings.GridCellSize);

    // 计算 near/far（使用默认值，因为 Pass 没有直接访问 Camera）
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    m_Grid->Draw(*gridShader, m_ViewMatrix, m_ProjectionMatrix, nearPlane, farPlane);
}

void SceneRenderer::SkyboxPass() {
    if (!m_Environment.Skybox) return;

    auto skyboxShader = m_ShaderLibrary.Get("skybox");
    if (!skyboxShader) return;

    StaticMesh* cube = MeshFactory::Get().GetCube();
    if (!cube) return;

    // 去除平移部分
    glm::mat4 view = glm::mat4(glm::mat3(m_ViewMatrix));

    // 天空盒渲染状态
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);  // 禁用背面剔除，天空盒从内部观看

    skyboxShader->Bind();
    skyboxShader->SetInt("skybox", 0);
    skyboxShader->SetMat4("view", view);
    skyboxShader->SetMat4("projection", m_ProjectionMatrix);

    cube->Bind();
    m_Environment.Skybox->Bind(0);
    RenderCommand::DrawArrays(GL_TRIANGLES, 0, cube->GetVertexCount());
    cube->Unbind();

    // 恢复状态
    // glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void SceneRenderer::GeometryPass() {
    if (m_OpaqueDrawList.empty()) return;

    auto modelShader = m_ShaderLibrary.Get("model");
    auto litShader = m_ShaderLibrary.Get("lit");
    if (!modelShader && !litShader) return;

    // 设置渲染状态（暂不使用 Pipeline，保持与原逻辑一致）
    RenderCommand::EnableDepthTest();
    RenderCommand::SetDepthMask(true);
    glDisable(GL_CULL_FACE);  // 禁用背面剔除，某些 FBX 模型面法线可能有问题

    // Wireframe 模式
    if (m_Settings.Wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    if (modelShader) {
        modelShader->Bind();
        m_OpaquePipeline->SetShader(modelShader.get());

        modelShader->SetMat4("u_View", m_ViewMatrix);
        modelShader->SetMat4("u_Projection", m_ProjectionMatrix);
        modelShader->SetVec3("u_ViewPos", m_CameraPosition);
        modelShader->SetInt("u_PointLightCount", m_Environment.PointLightCount);
        SetupLighting(*modelShader);

        for (const auto& cmd : m_OpaqueDrawList) {
            if (cmd.Type != DrawCommandType::Model) continue;
            if (!cmd.ModelPtr) continue;

            modelShader->SetMat4("u_Model", cmd.Transform);
            modelShader->SetMat3("u_NormalMatrix", cmd.NormalMatrix);
            modelShader->SetInt("u_EntityID", cmd.EntityID);
            modelShader->SetBool("u_HasDiffuse", cmd.HasDiffuseTexture);
            modelShader->SetBool("u_HasSpecular", cmd.HasSpecularTexture);

            // 如果没有 diffuse 纹理，绑定默认纹理
            if (!cmd.HasDiffuseTexture && m_DefaultDiffuse) {
                glActiveTexture(GL_TEXTURE0);
                m_DefaultDiffuse->Bind(0);
                modelShader->SetInt("texture_diffuse1", 0);
            }

            cmd.ModelPtr->Draw(*modelShader);
        }
    }

    // 处理 Mesh 类型的绘制命令
    if (litShader) {
        litShader->Bind();
        m_OpaquePipeline->SetShader(litShader.get());

        litShader->SetMat4("u_View", m_ViewMatrix);
        litShader->SetMat4("u_Projection", m_ProjectionMatrix);
        litShader->SetVec3("u_ViewPos", m_CameraPosition);
        SetupLighting(*litShader);

        // 绑定默认纹理
        if (m_DefaultDiffuse) {
            m_DefaultDiffuse->Bind(0);
            litShader->SetInt("material.diffuse", 0);
        }
        if (m_DefaultSpecular) {
            m_DefaultSpecular->Bind(1);
            litShader->SetInt("material.specular", 1);
        }
        litShader->SetFloat("material.shininess", 32.0f);

        for (const auto& cmd : m_OpaqueDrawList) {
            if (cmd.Type != DrawCommandType::Mesh) continue;
            if (!cmd.MeshPtr) continue;

            litShader->SetMat4("u_Model", cmd.Transform);
            litShader->SetMat3("u_NormalMatrix", cmd.NormalMatrix);
            litShader->SetInt("u_EntityID", cmd.EntityID);

            cmd.MeshPtr->Bind();
            if (cmd.UseIndices && cmd.IndexCount > 0) {
                RenderCommand::DrawIndexed(GL_TRIANGLES, cmd.IndexCount, GL_UNSIGNED_INT, nullptr);
            } else {
                RenderCommand::DrawArrays(GL_TRIANGLES, 0, cmd.VertexCount);
            }
            cmd.MeshPtr->Unbind();
        }
    }

    // 恢复渲染状态
    if (m_Settings.Wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    glEnable(GL_CULL_FACE);
}

void SceneRenderer::TransparentPass() {
    if (m_TransparentDrawList.empty()) return;

    // 透明渲染状态
    RenderCommand::EnableDepthTest();
    RenderCommand::SetDepthMask(false);  // 禁用深度写入
    RenderCommand::EnableBlending();
    RenderCommand::SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    auto modelShader = m_ShaderLibrary.Get("model");
    if (modelShader) {
        modelShader->Bind();
        m_TransparentPipeline->SetShader(modelShader.get());

        modelShader->SetMat4("u_View", m_ViewMatrix);
        modelShader->SetMat4("u_Projection", m_ProjectionMatrix);
        modelShader->SetVec3("u_ViewPos", m_CameraPosition);
        SetupLighting(*modelShader);

        for (const auto& cmd : m_TransparentDrawList) {
            if (cmd.Type != DrawCommandType::Model) continue;
            if (!cmd.ModelPtr) continue;

            modelShader->SetMat4("u_Model", cmd.Transform);
            modelShader->SetMat3("u_NormalMatrix", cmd.NormalMatrix);
            modelShader->SetInt("u_EntityID", cmd.EntityID);
            modelShader->SetBool("u_HasDiffuse", cmd.HasDiffuseTexture);
            modelShader->SetBool("u_HasSpecular", cmd.HasSpecularTexture);

            cmd.ModelPtr->Draw(*modelShader);
        }
    }

    // 恢复状态
    RenderCommand::SetDepthMask(true);
    RenderCommand::DisableBlending();
    glEnable(GL_CULL_FACE);
}

void SceneRenderer::SetupLighting(Shader& shader) {
    // 方向光
    Renderer::SetupDirectionalLight(shader, m_Environment.DirLight);

    // 点光源
    Renderer::SetupPointLights(shader, m_Environment.PointLights, m_Environment.PointLightCount);

    // 聚光灯
    if (m_Environment.SpotlightEnabled) {
        Renderer::SetupSpotLight(shader, m_Environment.Spotlight);
    } else {
        Renderer::DisableSpotLight(shader);
    }
}

void SceneRenderer::CreateDefaultResources() {
    m_Grid = std::make_unique<Grid>(m_Settings.GridSize, m_Settings.GridCellSize);

    // 初始化 MeshFactory（如果尚未初始化）
    // 注：MeshFactory 是单例，首次调用 GetCube() 时会自动初始化
}

void SceneRenderer::SyncLightsFromECS(ECS::World& world) {
    // 同步点光源
    m_Environment.PointLightCount = 0;

    world.Each<ECS::TransformComponent, ECS::PointLightComponent>(
        [this](ECS::Entity entity,
               ECS::TransformComponent& transform,
               ECS::PointLightComponent& light) {
            if (m_Environment.PointLightCount < 4) {
                int idx = m_Environment.PointLightCount++;
                m_Environment.PointLights[idx].Position = transform.Position;
                m_Environment.PointLights[idx].Ambient = glm::vec3(0.05f) * light.Intensity;
                m_Environment.PointLights[idx].Diffuse = light.Color * light.Intensity * 0.8f;
                m_Environment.PointLights[idx].Specular = light.Color * light.Intensity;
                m_Environment.PointLights[idx].Constant = light.Constant;
                m_Environment.PointLights[idx].Linear = light.Linear;
                m_Environment.PointLights[idx].Quadratic = light.Quadratic;
            }
        }
    );

    // TODO: 同步方向光和聚光灯（如果有 ECS 组件的话）
}

} // namespace GLRenderer
