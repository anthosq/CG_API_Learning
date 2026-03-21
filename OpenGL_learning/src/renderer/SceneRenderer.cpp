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


void SceneRenderer::SubmitMesh(Ref<MeshSource> meshSource,
                                uint32_t submeshIndex,
                                const glm::mat4& transform,
                                const Ref<MaterialTable>& materials,
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
    cmd.Materials = materials;
    cmd.EntityID = entityID;

    // 计算到相机的距离（用于透明排序）
    glm::vec3 pos = glm::vec3(cmd.Transform[3]);
    cmd.DistanceToCamera = glm::distance(m_CameraPosition, pos);

    // TODO: 根据材质判断透明度，目前全部作为不透明
    m_OpaqueDrawList.push_back(cmd);
}

void SceneRenderer::SubmitStaticMesh(const Ref<StaticMesh>& staticMesh,
                                      Ref<MeshSource> meshSource,
                                      const glm::mat4& transform,
                                      int entityID) {
    if (!staticMesh || !meshSource) return;

    // 获取要渲染的 submesh 列表
    const auto& submeshIndices = staticMesh->GetSubmeshes();
    auto materials = staticMesh->GetMaterials();

    if (submeshIndices.empty()) {
        // 如果没有指定 submesh，渲染所有
        for (uint32_t i = 0; i < meshSource->GetSubmeshCount(); ++i) {
            SubmitMesh(meshSource, i, transform, materials, entityID);
        }
    } else {
        // 渲染指定的 submesh
        for (uint32_t submeshIndex : submeshIndices) {
            SubmitMesh(meshSource, submeshIndex, transform, materials, entityID);
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

            // 尝试获取 StaticMesh
            auto staticMesh = AssetManager::Get().GetAsset<StaticMesh>(meshComp.MeshHandle);
            if (staticMesh) {
                auto meshSource = AssetManager::Get().GetAsset<MeshSource>(staticMesh->GetMeshSource());
                if (meshSource) {
                    SubmitStaticMesh(staticMesh, meshSource, transform.WorldMatrix,
                                     static_cast<int>(entity.GetHandle()));
                }
                return;
            }

            // 尝试获取 MeshSource (直接引用)
            auto meshSource = AssetManager::Get().GetAsset<MeshSource>(meshComp.MeshHandle);
            if (meshSource) {
                // 渲染所有 submesh
                for (uint32_t i = 0; i < meshSource->GetSubmeshCount(); ++i) {
                    SubmitMesh(meshSource, i, transform.WorldMatrix, nullptr,
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

    auto litShader = m_ShaderLibrary.Get("lit");
    if (!litShader) return;

    // 设置渲染状态
    RenderCommand::EnableDepthTest();
    RenderCommand::SetDepthMask(true);
    glDisable(GL_CULL_FACE);

    if (m_Settings.Wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    litShader->Bind();
    m_OpaquePipeline->SetShader(litShader);

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

    // 统一渲染所有 MeshSource
    for (const auto& cmd : m_OpaqueDrawList) {
        if (!cmd.MeshSource) continue;

        auto vao = cmd.MeshSource->GetVertexArray();
        if (!vao) continue;

        litShader->SetMat4("u_Model", cmd.Transform);
        litShader->SetMat3("u_NormalMatrix", cmd.NormalMatrix);
        litShader->SetInt("u_EntityID", cmd.EntityID);

        const auto& submeshes = cmd.MeshSource->GetSubmeshes();
        if (cmd.SubmeshIndex >= submeshes.size()) continue;

        const auto& submesh = submeshes[cmd.SubmeshIndex];

        // 绑定纹理：优先使用 MaterialTable，否则使用 MeshSource 自带的材质
        bool hasTexture = false;
        if (cmd.Materials) {
            AssetHandle matHandle = cmd.Materials->GetMaterial(submesh.MaterialIndex);
            if (matHandle.IsValid()) {
                auto matAsset = AssetManager::Get().GetAsset<MaterialAsset>(matHandle);
                if (matAsset) {
                    AssetHandle albedoHandle = matAsset->GetAlbedoMap();
                    if (albedoHandle.IsValid()) {
                        auto albedoTex = AssetManager::Get().GetAsset<Texture2D>(albedoHandle);
                        if (albedoTex) {
                            albedoTex->Bind(0);
                            litShader->SetInt("material.diffuse", 0);
                            hasTexture = true;
                        }
                    }
                }
            }
        }

        if (!hasTexture) {
            const auto& meshMaterials = cmd.MeshSource->GetMaterials();
            if (submesh.MaterialIndex < meshMaterials.size()) {
                AssetHandle texHandle = meshMaterials[submesh.MaterialIndex];
                if (texHandle.IsValid()) {
                    auto texture = AssetManager::Get().GetAsset<Texture2D>(texHandle);
                    if (texture) {
                        texture->Bind(0);
                        litShader->SetInt("material.diffuse", 0);
                        hasTexture = true;
                    }
                }
            }
        }

        // 如果没有纹理，重新绑定默认纹理
        if (!hasTexture && m_DefaultDiffuse) {
            m_DefaultDiffuse->Bind(0);
        }

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

    litShader->SetMat4("u_View", m_ViewMatrix);
    litShader->SetMat4("u_Projection", m_ProjectionMatrix);
    litShader->SetVec3("u_ViewPos", m_CameraPosition);
    SetupLighting(*litShader);

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

    // 创建立方体网格 (用于天空盒等)
    AssetHandle cubeHandle = MeshFactory::CreateCube();
    auto staticMesh = AssetManager::Get().GetAsset<StaticMesh>(cubeHandle);
    if (staticMesh) {
        m_CubeMesh = AssetManager::Get().GetAsset<MeshSource>(staticMesh->GetMeshSource());
    }
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
}

} // namespace GLRenderer
