#include "SceneRenderer.h"
#include "RenderCommand.h"
#include "asset/AssetManager.h"
#include "graphics/MeshSource.h"
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

    // 默认不设置方向光（全部为零）
    // 如需方向光，通过 GetEnvironment().DirLight 配置

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
        UpdateLightingUBO();
    }

    Renderer::BeginFrame(camera, aspectRatio);
}

void SceneRenderer::EndScene() {
    Renderer::EndFrame();
}


void SceneRenderer::SubmitMesh(Ref<MeshSource> meshSource,
                                uint32_t submeshIndex,
                                const glm::mat4& transform,
                                const Ref<MaterialTable>& materialTable,
                                int entityID) {
    if (!meshSource) return;

    // 确保 GPU 缓冲区已创建
    if (!meshSource->HasGPUBuffers()) {
        meshSource->CreateGPUBuffers();
    }

    const auto& submeshes = meshSource->GetSubmeshes();
    if (submeshIndex >= submeshes.size()) return;

    const auto& submesh = submeshes[submeshIndex];

    DrawCommand cmd;
    cmd.MeshSource = meshSource;
    cmd.SubmeshIndex = submeshIndex;
    cmd.Transform = transform * submesh.Transform;
    cmd.NormalMatrix = glm::transpose(glm::inverse(glm::mat3(cmd.Transform)));
    cmd.Materials = materialTable;  // 组件级材质表
    cmd.EntityID = entityID;

    // 计算到相机的距离（用于透明排序）
    glm::vec3 pos = glm::vec3(cmd.Transform[3]);
    cmd.DistanceToCamera = glm::distance(m_CameraPosition, pos);

    // TODO: 根据材质判断透明度，目前全部作为不透明
    m_OpaqueDrawList.push_back(cmd);
}

void SceneRenderer::SubmitStaticMesh(const Ref<StaticMesh>& staticMesh,
                                      Ref<MeshSource> meshSource,
                                      const Ref<MaterialTable>& materialTable,
                                      const glm::mat4& transform,
                                      int entityID) {
    if (!staticMesh || !meshSource) return;

    // 获取要渲染的 submesh 列表
    const auto& submeshIndices = staticMesh->GetSubmeshes();

    // 组件级材质优先，否则用 StaticMesh 默认材质
    Ref<MaterialTable> effectiveMaterials = materialTable ? materialTable : staticMesh->GetMaterials();

    if (submeshIndices.empty()) {
        // 如果没有指定 submesh，渲染所有
        for (uint32_t i = 0; i < meshSource->GetSubmeshCount(); ++i) {
            SubmitMesh(meshSource, i, transform, effectiveMaterials, entityID);
        }
    } else {
        // 渲染指定的 submesh
        for (uint32_t submeshIndex : submeshIndices) {
            SubmitMesh(meshSource, submeshIndex, transform, effectiveMaterials, entityID);
        }
    }
}

void SceneRenderer::SortTransparentDrawList() {
    // 按距离从远到近排序（透明物体需要从后往前渲染）
    std::sort(m_TransparentDrawList.begin(), m_TransparentDrawList.end());
}

void SceneRenderer::CollectDrawCommandsFromECS(ECS::World& world) {
    // 收集 MeshComponent 实体
    world.Each<ECS::TransformComponent, ECS::MeshComponent>(
        [this](ECS::Entity entity,
               ECS::TransformComponent& transform,
               ECS::MeshComponent& meshComp) {
            if (!meshComp.MeshHandle.IsValid()) return;
            if (!meshComp.Visible) return;

            // 尝试获取 StaticMesh
            auto staticMesh = AssetManager::Get().GetAsset<StaticMesh>(meshComp.MeshHandle);
            if (staticMesh) {
                auto meshSource = AssetManager::Get().GetAsset<MeshSource>(staticMesh->GetMeshSource());
                if (meshSource) {
                    // 传入组件级 MaterialTable，材质选择逻辑在 SubmitStaticMesh 内部
                    SubmitStaticMesh(staticMesh, meshSource, meshComp.Materials,
                                     transform.WorldMatrix, static_cast<int>(entity.GetHandle()));
                }
                return;
            }

            // 尝试获取 MeshSource (直接引用)
            auto meshSource = AssetManager::Get().GetAsset<MeshSource>(meshComp.MeshHandle);
            if (meshSource) {
                // 渲染所有 submesh
                for (uint32_t i = 0; i < meshSource->GetSubmeshCount(); ++i) {
                    SubmitMesh(meshSource, i, transform.WorldMatrix, meshComp.Materials,
                               static_cast<int>(entity.GetHandle()));
                }
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

    if (!m_CubeMesh || !m_CubeMesh->GetVertexArray()) return;

    // 去除平移部分
    glm::mat4 view = glm::mat4(glm::mat3(m_ViewMatrix));

    // 天空盒渲染状态
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    skyboxShader->Bind();
    skyboxShader->SetInt("skybox", 0);
    skyboxShader->SetMat4("view", view);
    skyboxShader->SetMat4("projection", m_ProjectionMatrix);

    auto vao = m_CubeMesh->GetVertexArray();
    vao->Bind();
    m_Environment.Skybox->Bind(0);

    // 使用第一个 submesh 渲染
    const auto& submeshes = m_CubeMesh->GetSubmeshes();
    if (!submeshes.empty()) {
        const auto& submesh = submeshes[0];
        glDrawElementsBaseVertex(
            GL_TRIANGLES,
            submesh.IndexCount,
            GL_UNSIGNED_INT,
            (void*)(submesh.BaseIndex * sizeof(uint32_t)),
            submesh.BaseVertex
        );
    }
    vao->Unbind();

    // 恢复状态
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void SceneRenderer::GeometryPass() {
    if (m_OpaqueDrawList.empty()) return;

    auto pbrShader = m_ShaderLibrary.Get("pbr");
    if (!pbrShader) {
        // fallback to lit shader
        pbrShader = m_ShaderLibrary.Get("lit");
        if (!pbrShader) return;
    }

    // 设置渲染状态
    RenderCommand::EnableDepthTest();
    RenderCommand::SetDepthMask(true);
    glDisable(GL_CULL_FACE);

    if (m_Settings.Wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    pbrShader->Bind();
    m_OpaquePipeline->SetShader(pbrShader);

    pbrShader->SetFloat("u_EnvironmentIntensity", m_Environment.EnvironmentIntensity);

    for (const auto& cmd : m_OpaqueDrawList) {
        if (!cmd.MeshSource) continue;

        auto vao = cmd.MeshSource->GetVertexArray();
        if (!vao) continue;

        // 获取并绑定材质
        auto material = GetMaterialForDrawCommand(cmd);
        BindPBRMaterial(*pbrShader, material);

        // 设置变换
        pbrShader->SetMat4("u_Model", cmd.Transform);
        pbrShader->SetMat3("u_NormalMatrix", cmd.NormalMatrix);
        pbrShader->SetInt("u_EntityID", cmd.EntityID);

        const auto& submeshes = cmd.MeshSource->GetSubmeshes();
        if (cmd.SubmeshIndex >= submeshes.size()) continue;
        const auto& submesh = submeshes[cmd.SubmeshIndex];

        vao->Bind();
        glDrawElementsBaseVertex(
            GL_TRIANGLES,
            submesh.IndexCount,
            GL_UNSIGNED_INT,
            (void*)(submesh.BaseIndex * sizeof(uint32_t)),
            submesh.BaseVertex
        );
        vao->Unbind();
    }

    // 恢复渲染状态
    if (m_Settings.Wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // 重置 OpenGL 状态
    glBindVertexArray(0);
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void SceneRenderer::TransparentPass() {
    if (m_TransparentDrawList.empty()) return;

    auto litShader = m_ShaderLibrary.Get("lit");
    if (!litShader) return;

    // 透明渲染状态
    RenderCommand::EnableDepthTest();
    RenderCommand::SetDepthMask(false);
    RenderCommand::EnableBlending();
    RenderCommand::SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    litShader->Bind();
    m_TransparentPipeline->SetShader(litShader);

    // UBO 已经在 BeginScene 中更新，无需单独设置光照 uniform

    for (const auto& cmd : m_TransparentDrawList) {
        if (!cmd.MeshSource) continue;

        auto vao = cmd.MeshSource->GetVertexArray();
        if (!vao) continue;

        litShader->SetMat4("u_Model", cmd.Transform);
        litShader->SetMat3("u_NormalMatrix", cmd.NormalMatrix);
        litShader->SetInt("u_EntityID", cmd.EntityID);

        const auto& submeshes = cmd.MeshSource->GetSubmeshes();
        if (cmd.SubmeshIndex >= submeshes.size()) continue;

        const auto& submesh = submeshes[cmd.SubmeshIndex];

        vao->Bind();
        glDrawElementsBaseVertex(
            GL_TRIANGLES,
            submesh.IndexCount,
            GL_UNSIGNED_INT,
            (void*)(submesh.BaseIndex * sizeof(uint32_t)),
            submesh.BaseVertex
        );
        vao->Unbind();
    }

    // 恢复状态
    RenderCommand::SetDepthMask(true);
    RenderCommand::DisableBlending();
}

void SceneRenderer::UpdateLightingUBO() {
    LightingUBO lightData = {};

    // 方向光
    lightData.DirLightDirection = glm::vec4(m_Environment.DirLight.Direction, 1.0f);
    lightData.DirLightColor = glm::vec4(m_Environment.DirLight.Diffuse, 0.0f);

    // 点光源
    int count = std::min(m_Environment.PointLightCount, MAX_POINT_LIGHTS);
    lightData.LightCounts.x = count;
    for (int i = 0; i < count; ++i) {
        const auto& light = m_Environment.PointLights[i];
        float radius = 1.0f / (light.Quadratic > 0.0f ? sqrt(light.Quadratic) : 1.0f);
        lightData.PointLightPosRadius[i] = glm::vec4(light.Position, radius);
        lightData.PointLightColorIntensity[i] = glm::vec4(light.Diffuse, 1.0f);
        lightData.PointLightParams[i] = glm::vec4(0.5f, 0.0f, 0.0f, 0.0f);
    }

    // 聚光灯
    lightData.LightCounts.y = m_Environment.SpotlightEnabled ? 1 : 0;
    if (m_Environment.SpotlightEnabled) {
        const auto& spot = m_Environment.Spotlight;
        float range = 1.0f / (spot.Quadratic > 0.0f ? sqrt(spot.Quadratic) : 1.0f);
        lightData.SpotLightPosRange = glm::vec4(spot.Position, range);
        lightData.SpotLightDirAngle = glm::vec4(spot.Direction, spot.OuterCutOff);
        lightData.SpotLightColorIntensity = glm::vec4(spot.Diffuse, 1.0f);
        lightData.SpotLightParams = glm::vec4(0.5f, 2.0f, 0.0f, 0.0f);
    }

    // 环境光
    lightData.AmbientColor = glm::vec4(m_Settings.AmbientColor, 0.3f);

    m_LightingUBO->SetData(&lightData, sizeof(lightData));
}

void SceneRenderer::BindPBRMaterial(Shader& shader, const Ref<MaterialAsset>& material) {
    if (!material) return;

    auto mat = material->GetMaterial();
    if (!mat) return;

    // Material::Apply 上传 uniform 并绑定纹理
    mat->Apply(shader);
}

Ref<MaterialAsset> SceneRenderer::GetMaterialForDrawCommand(const DrawCommand& cmd) {
    // 优先使用 MaterialTable
    if (cmd.Materials) {
        const auto& submeshes = cmd.MeshSource->GetSubmeshes();
        if (cmd.SubmeshIndex < submeshes.size()) {
            uint32_t matIndex = submeshes[cmd.SubmeshIndex].MaterialIndex;
            if (cmd.Materials->HasMaterial(matIndex)) {
                auto mat = AssetManager::Get().GetAsset<MaterialAsset>(
                    cmd.Materials->GetMaterial(matIndex));
                if (mat) return mat;
            }
        }
    }

    // 返回默认材质
    return m_DefaultMaterial;
}

void SceneRenderer::CreateDefaultResources() {
    m_Grid = std::make_unique<Grid>(m_Settings.GridSize, m_Settings.GridCellSize);

    // 创建立方体网格 (用于天空盒等)
    AssetHandle cubeHandle = MeshFactory::CreateCube();
    auto staticMesh = AssetManager::Get().GetAsset<StaticMesh>(cubeHandle);
    if (staticMesh) {
        m_CubeMesh = AssetManager::Get().GetAsset<MeshSource>(staticMesh->GetMeshSource());
    }

    // 创建默认 PBR 材质
    m_DefaultMaterial = MaterialAsset::Create(false);
    m_DefaultMaterial->SetAlbedoColor(glm::vec3(0.8f));
    m_DefaultMaterial->SetMetallic(0.0f);
    m_DefaultMaterial->SetRoughness(0.5f);
}

void SceneRenderer::SyncLightsFromECS(ECS::World& world) {
    // 同步方向光（使用第一个找到的方向光实体）
    bool foundDirLight = false;
    world.Each<ECS::TransformComponent, ECS::DirectionalLightComponent>(
        [this, &foundDirLight](ECS::Entity entity,
               ECS::TransformComponent& transform,
               ECS::DirectionalLightComponent& light) {
            if (!foundDirLight) {
                // 从 Transform 的旋转获取方向（forward 向量）
                glm::vec3 direction = transform.GetForward();
                m_Environment.DirLight.Direction = direction;
                m_Environment.DirLight.Ambient = glm::vec3(0.1f) * light.Intensity;
                m_Environment.DirLight.Diffuse = light.Color * light.Intensity;
                m_Environment.DirLight.Specular = light.Color * light.Intensity;
                foundDirLight = true;
            }
        }
    );

    // 如果没有方向光实体，使用默认的弱方向光（保证场景基本可见）
    if (!foundDirLight) {
        m_Environment.DirLight.Direction = glm::vec3(-0.2f, -1.0f, -0.3f);
        m_Environment.DirLight.Ambient = glm::vec3(0.1f);
        m_Environment.DirLight.Diffuse = glm::vec3(0.3f);   // 默认漫反射光
        m_Environment.DirLight.Specular = glm::vec3(0.3f);  // 默认镜面反射光
    }

    // 同步点光源
    m_Environment.PointLightCount = 0;

    world.Each<ECS::TransformComponent, ECS::PointLightComponent>(
        [this](ECS::Entity entity,
               ECS::TransformComponent& transform,
               ECS::PointLightComponent& light) {
            if (m_Environment.PointLightCount < MAX_POINT_LIGHTS) {
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
}

} // namespace GLRenderer
