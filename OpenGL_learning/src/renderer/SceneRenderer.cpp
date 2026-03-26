#include "SceneRenderer.h"
#include "RenderCommand.h"
#include "asset/AssetManager.h"
#include "graphics/MeshSource.h"
#include "graphics/IBLProcessor.h"
#include "renderer/Material.h"
#include <algorithm>
#include <random>

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
    m_CameraUBO   = std::make_unique<UniformBuffer>(static_cast<uint32_t>(sizeof(CameraUBO)),   UBO_BINDING_CAMERA);
    m_LightingUBO = std::make_unique<UniformBuffer>(static_cast<uint32_t>(sizeof(LightingUBO)), UBO_BINDING_LIGHTING);
    m_ShadowUBO   = std::make_unique<UniformBuffer>(static_cast<uint32_t>(sizeof(ShadowUBO)),   UBO_BINDING_SHADOW);

    // 创建阴影贴图和 Pipeline
    ShadowMap::Config shadowCfg;
    shadowCfg.Resolution   = m_Settings.ShadowResolution;
    shadowCfg.CascadeCount = m_Settings.ShadowCascadeCount;
    m_ShadowMap = ShadowMap::Create(shadowCfg);

    m_ShadowPipeline = std::make_unique<Pipeline>(PipelineSpecification::Shadow());

    // 点光源阴影 Cubemap Array
    {
        PointShadowMapArray::Config psmCfg;
        psmCfg.Resolution = m_Settings.PointShadowResolution;
        psmCfg.Count      = static_cast<uint32_t>(MAX_SHADOW_POINT_LIGHTS);
        m_PointShadowArray = PointShadowMapArray::Create(psmCfg);
    }
    std::fill(m_PointShadowAssignments,
              m_PointShadowAssignments + MAX_SHADOW_POINT_LIGHTS, -1);

    // 生成 SSAO 采样核（只需一次）
    GenerateSSAOKernel();

    // Tiled Forward+：预分配最小 SSBO（实际大小在第一次 FlushDrawList 时扩容）
    m_PointLightSSBO     = StorageBuffer::Create(sizeof(PointLightGPU) * MAX_POINT_LIGHTS);
    m_TileLightCountSSBO = StorageBuffer::Create(sizeof(int) * 1);   // 懒扩容
    m_TileLightIndexSSBO = StorageBuffer::Create(sizeof(int) * 1);   // 懒扩容

    // HDR FBO 在首次 FlushDrawList 时根据 TargetFramebuffer 尺寸懒创建
    m_Initialized = true;
    std::cout << "[SceneRenderer] Initialized" << std::endl;
}

void SceneRenderer::Shutdown() {
    if (!m_Initialized) return;

    m_Grid.reset();
    m_CameraUBO.reset();
    m_LightingUBO.reset();
    m_ShadowUBO.reset();
    m_ShadowMap = nullptr;
    m_ShadowPipeline.reset();
    m_PointShadowArray = nullptr;
    m_ShaderLibrary.Clear();

    m_TileLightCullPipeline.Reset();
    m_PointLightSSBO.Reset();
    m_TileLightCountSSBO.Reset();
    m_TileLightIndexSSBO.Reset();

    m_GNormalFBO.Reset();
    m_SSAOFbo.Reset();
    m_SSAOBlurFBO.Reset();
    m_GBuffer = nullptr;

    if (m_HiZTex)            { glDeleteTextures(1, &m_HiZTex);            m_HiZTex            = 0; }
    if (m_SSROutputTex)      { glDeleteTextures(1, &m_SSROutputTex);      m_SSROutputTex      = 0; }
    if (m_BackFaceDepthTex)  { glDeleteTextures(1, &m_BackFaceDepthTex);  m_BackFaceDepthTex  = 0; }
    if (m_BackFaceDepthFBO)  { glDeleteFramebuffers(1, &m_BackFaceDepthFBO); m_BackFaceDepthFBO = 0; }
    m_HiZPipeline.Reset();
    m_SSRPipeline.Reset();

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
            shader->BindUniformBlock("CameraData",   UBO_BINDING_CAMERA);
            shader->BindUniformBlock("LightingData", UBO_BINDING_LIGHTING);
        }
    }

    // 加载 Tiled Forward+ 光源剔除 Compute Shader
    auto cullPath = shaderDir / "tiled_light_cull.glsl";
    if (std::filesystem::exists(cullPath)) {
        m_TileLightCullPipeline = ComputePipeline::Create(cullPath);
        if (!m_TileLightCullPipeline || !m_TileLightCullPipeline->IsValid())
            std::cerr << "[SceneRenderer] Failed to load tiled_light_cull.glsl\n";
        else
            std::cout << "[SceneRenderer] Tiled Forward+ compute shader loaded\n";
    }

    // Hi-Z 构建 Compute Shader（SSR 加速结构）
    auto hizPath = shaderDir / "hiz_build.glsl";
    if (std::filesystem::exists(hizPath)) {
        m_HiZPipeline = ComputePipeline::Create(hizPath);
        if (!m_HiZPipeline || !m_HiZPipeline->IsValid())
            std::cerr << "[SceneRenderer] Failed to load hiz_build.glsl\n";
        else
            std::cout << "[SceneRenderer] Hi-Z compute shader loaded\n";
    }

    // SSR Compute Shader
    auto ssrPath = shaderDir / "ssr.glsl";
    if (std::filesystem::exists(ssrPath)) {
        m_SSRPipeline = ComputePipeline::Create(ssrPath);
        if (!m_SSRPipeline || !m_SSRPipeline->IsValid())
            std::cerr << "[SceneRenderer] Failed to load ssr.glsl\n";
        else
            std::cout << "[SceneRenderer] SSR compute shader loaded\n";
    }

    // 粒子 Compute Pipelines
    auto loadParticlePipeline = [&](const char* filename, Ref<ComputePipeline>& out) {
        auto path = shaderDir / filename;
        if (std::filesystem::exists(path)) {
            out = ComputePipeline::Create(path);
            if (!out || !out->IsValid())
                std::cerr << "[SceneRenderer] Failed to load " << filename << "\n";
            else
                std::cout << "[SceneRenderer] Particle compute shader loaded: " << filename << "\n";
        }
    };
    loadParticlePipeline("particle_emit.glsl",    m_ParticleEmitPipeline);
    loadParticlePipeline("particle_update.glsl",  m_ParticleUpdatePipeline);
    loadParticlePipeline("particle_compact.glsl", m_ParticleCompactPipeline);
}

void SceneRenderer::RenderScene(ECS::World& world,
                                 Framebuffer& targetFBO,
                                 const Camera& camera,
                                 float aspectRatio,
                                 float deltaTime) {
    m_DeltaTime = deltaTime;
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

    // 4. 执行绘制列表（内部创建/调整 HDR FBO，最后调用 CompositePass 输出到 targetFBO）
    FlushDrawList();
}

void SceneRenderer::BeginScene(const Camera& camera, float aspectRatio) {
    // 清空绘制列表
    m_OpaqueDrawList.clear();
    m_TransparentDrawList.clear();
    m_LightEntities.clear();
    m_WorldAABBs.clear();
    m_ParticleDrawList.clear();

    // 缓存相机数据
    m_ViewMatrix       = camera.GetViewMatrix();
    m_ProjectionMatrix = camera.GetProjectionMatrix(aspectRatio);
    m_CameraPosition   = camera.GetPosition();
    m_CameraNear       = camera.GetNearPlane();
    m_CameraFar        = camera.GetFarPlane();

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

    // 计算世界空间 AABB（与 DrawCommand 一一对应，供 BVH 和视锥裁剪使用）
    AABB worldAABB = submesh.BoundingBox.Transform(cmd.Transform);
    m_WorldAABBs.push_back(worldAABB);

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
                // 组件级材质优先；否则回退到 MeshSource 内嵌材质
                Ref<MaterialTable> effectiveMats = meshComp.Materials;
                if (!effectiveMats) {
                    const auto& defaultMats = meshSource->GetMaterials();
                    if (!defaultMats.empty()) {
                        effectiveMats = MaterialTable::Create(static_cast<uint32_t>(defaultMats.size()));
                        for (uint32_t i = 0; i < defaultMats.size(); ++i)
                            effectiveMats->SetMaterial(i, defaultMats[i]);
                    }
                }
                for (uint32_t i = 0; i < meshSource->GetSubmeshCount(); ++i) {
                    SubmitMesh(meshSource, i, transform.WorldMatrix, effectiveMats,
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

    // 收集粒子组件
    world.Each<ECS::TransformComponent, ECS::ParticleComponent>(
        [this](ECS::Entity /*entity*/,
               ECS::TransformComponent& transform,
               ECS::ParticleComponent& particle) {
            if (!particle.Playing) return;

            // 懒初始化 RuntimeSystem
            if (!particle.RuntimeSystem) {
                particle.RuntimeSystem = std::make_shared<ParticleSystem>(particle.MaxParticles);
            }

            // 计算本帧应发射的粒子数
            particle.EmitAccumulator += particle.EmitRate * m_DeltaTime;
            int emitCount = (int)particle.EmitAccumulator;
            particle.EmitAccumulator -= (float)emitCount;

            EmitParams params;
            params.Position      = glm::vec4(transform.WorldMatrix[3]);
            params.EmitDirection = glm::vec4(glm::normalize(particle.EmitDirection), 0.0f);
            params.ColorBegin    = particle.ColorBegin;
            params.ColorEnd      = particle.ColorEnd;
            params.Gravity       = glm::vec4(particle.Gravity, 0.0f);
            params.EmitSpread    = particle.EmitSpread;
            params.EmitRate      = particle.EmitRate;
            params.LifetimeMin   = particle.LifetimeMin;
            params.LifetimeMax   = particle.LifetimeMax;
            params.SpeedMin      = particle.SpeedMin;
            params.SpeedMax      = particle.SpeedMax;
            params.SizeBegin     = particle.SizeBegin;
            params.SizeEnd       = particle.SizeEnd;
            params.DeltaTime     = m_DeltaTime;
            params.MaxParticles  = particle.MaxParticles;
            params.EmitCount     = emitCount;

            ParticleDrawEntry entry;
            entry.System = particle.RuntimeSystem;
            entry.Params = params;
            m_ParticleDrawList.push_back(std::move(entry));
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

    const uint32_t viewportWidth  = m_TargetFramebuffer->GetWidth();
    const uint32_t viewportHeight = m_TargetFramebuffer->GetHeight();

    // 确保 HDR FBO 尺寸与目标一致
    EnsureHDRFramebuffer(viewportWidth, viewportHeight);
    EnsureSSAOResources(viewportWidth, viewportHeight);

    // 所有几何 Pass 渲染到内部 HDR FBO（GL_RGBA16F，保留 >1.0 的 HDR 值）
    m_HDRFramebuffer->Bind();
    RenderCommand::SetViewport(0, 0, m_HDRFramebuffer->GetWidth(),
                                      m_HDRFramebuffer->GetHeight());
    RenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    RenderCommand::Clear();
    RenderCommand::EnableDepthTest();

    // 清除实体 ID 附件（-1 表示无实体）
    m_HDRFramebuffer->ClearEntityID(-1);

    // 提取相机视锥，构建 BVH（当帧绘制列表已确定）
    m_CameraFrustum = Frustum::FromViewProjection(m_ProjectionMatrix * m_ViewMatrix);
    m_SceneBVH.Build(m_WorldAABBs);

    // 0-a. 方向光阴影 Pass（CSM）
    CalculateCascades(m_Cascades);
    UpdateShadowUBO();
    ShadowPass();

    // 0-b. 点光源阴影 Pass（填充 m_PointShadowAssignments）
    if (m_Settings.EnablePointShadows)
        PointShadowPass();

    // 0-c. 上传点光源 SSBO（读取 m_PointShadowAssignments，供后续所有 Pass 使用）
    if (m_Settings.EnableTiledLighting)
        UploadPointLightSSBO();

    bool hasSkybox = m_Environment.Skybox ||
                     (m_Environment.IBLEnvironment && m_Environment.IBLEnvironment->EnvMap);

    if (m_Settings.UseDeferredShading && m_GBuffer) {
        // === Tiled Deferred 路径 ===

        // 1. G-Buffer Pass：写入几何数据（位置由深度重建，无独立 DepthPrePass）
        GBufferPass();
        m_SceneDepthTexture = m_GBuffer->GetDepth();

        // 2. SSAO：直接读 GBuffer 法线（跳过 NormalPrePass，GBuffer 已有深度和法线）
        if (m_Settings.EnableSSAO) {
            SSAOPass();
            SSAOBlurPass();
        }

        // 3. Tiled Forward+ 光源剔除（读 GBuffer 深度）
        if (m_Settings.EnableTiledLighting)
            TiledLightCullPass();

        // 4. Deferred Lighting Pass → 输出 HDR 颜色
        m_HDRFramebuffer->Bind();
        RenderCommand::SetViewport(0, 0, viewportWidth, viewportHeight);
        glClear(GL_COLOR_BUFFER_BIT);  // 只清颜色，深度留给 blit
        DeferredLightingPass();

        // 4.5 SSR（Hi-Z 从 GBuffer 深度构建，在 HDR 颜色输出后、深度 blit 前执行）
        if (m_Settings.EnableSSR && m_HiZPipeline && m_SSRPipeline) {
            BackFaceDepthPass();
            HiZBuildPass();
            SSRPass();
        }

        // 5. 将 GBuffer 深度 blit 到 HDR FBO，供 SkyboxPass / TransparentPass 做深度测试
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_GBuffer->GetFBO());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_HDRFramebuffer->GetID());
        glBlitFramebuffer(0, 0, viewportWidth, viewportHeight,
                          0, 0, viewportWidth, viewportHeight,
                          GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, m_HDRFramebuffer->GetID());

        RenderCommand::EnableDepthTest();

        // 6. Grid / Wireframe 叠加（在 HDR FBO 上，GBuffer 深度已 blit 过来）
        if (m_Settings.ShowGrid && !m_Settings.Wireframe)
            GridPass();

        if (m_Settings.Wireframe)
            WireframeOverlayPass();

        // 7. 天空盒（深度 LEQUAL，在透明物体之前）
        if (m_Settings.ShowSkybox && hasSkybox)
            SkyboxPass();

        // 8. 透明物体（保持 Forward+）
        if (!m_TransparentDrawList.empty()) {
            SortTransparentDrawList();
            TransparentPass();
        }

        // 9. 粒子（加法混合，不写深度）
        if (!m_ParticleDrawList.empty())
            ParticlePass();

        m_HDRFramebuffer->Unbind();
    } else {
        // === Tiled Forward+ 路径（原有逻辑不变）===

        // 1. SSAO：法线预处理 → AO 计算 → 深度感知模糊
        if (m_Settings.EnableSSAO) {
            NormalPrePass();
            SSAOPass();
            SSAOBlurPass();
        }

        // 恢复 HDR FBO（ShadowPass / SSAO 会切换 FBO）
        m_HDRFramebuffer->Bind();
        RenderCommand::SetViewport(0, 0, viewportWidth, viewportHeight);

        // Wireframe 模式
        if (m_Settings.Wireframe)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        // 2. Pre-Depth Pass（Wireframe 下跳过）
        m_SceneDepthTexture = 0;
        if (m_Settings.EnableDepthPrepass && !m_Settings.Wireframe) {
            DepthPrePass();
            m_SceneDepthTexture = m_HDRFramebuffer->GetDepthAttachment();
        }

        // 2.5 Tiled Forward+ 光源剔除
        if (m_Settings.EnableTiledLighting) {
            TiledLightCullPass();
            m_HDRFramebuffer->Bind();
        }

        // 3. Grid
        if (m_Settings.ShowGrid && !m_Settings.Wireframe)
            GridPass();

        // 4. 不透明几何 Pass
        GeometryPass();

        // 5. 天空盒
        if (m_Settings.ShowSkybox && hasSkybox)
            SkyboxPass();

        // 6. 透明物体
        if (!m_TransparentDrawList.empty()) {
            SortTransparentDrawList();
            TransparentPass();
        }

        // 7. 粒子（加法混合，不写深度）
        if (!m_ParticleDrawList.empty())
            ParticlePass();

        if (m_Settings.Wireframe)
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        m_HDRFramebuffer->Unbind();
    }

    // 后处理阶段：全屏四边形，不需要深度测试，统一在此关闭
    glDisable(GL_DEPTH_TEST);

    // Bloom 三阶段（仅在 EnableBloom 时执行）
    if (m_Settings.EnableBloom) {
        uint32_t hw = m_TargetFramebuffer->GetWidth()  / 2;
        uint32_t hh = m_TargetFramebuffer->GetHeight() / 2;
        EnsureBloomTextures(hw, hh);
        BloomPrefilterPass();
        BloomDownsamplePass();
        BloomUpsamplePass();
    }

    // Tonemap HDR → target FBO（LDR，ImGui 显示用）
    CompositePass();

    ++m_FrameIndex;

    glEnable(GL_DEPTH_TEST);
}

void SceneRenderer::GridPass() {
    if (!m_Grid) return;

    auto gridShader = m_ShaderLibrary.Get("grid");
    if (!gridShader) return;

    m_Grid->SetGridSize(m_Settings.GridSize);
    m_Grid->SetGridCellSize(m_Settings.GridCellSize);

    // FBO 含整型附件 (EntityID, GL_R32I): 开启混合时绘制到整型附件会触发 GL_INVALID_OPERATION
    // 临时将 draw buffer 1 (整型) 设为 GL_NONE, 让 Grid 只写入 attachment 0 (float)
    GLenum gridBufs[] = { GL_COLOR_ATTACHMENT0, GL_NONE };
    glDrawBuffers(2, gridBufs);

    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    m_Grid->Draw(*gridShader, m_ViewMatrix, m_ProjectionMatrix, nearPlane, farPlane);

    // 恢复所有 draw buffers
    GLenum allBufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, allBufs);
}

void SceneRenderer::SkyboxPass() {
    // IBL 天空盒用 EnvMap (1024x1024, 清晰)，不用 RadianceMap (预过滤, 模糊)
    Ref<TextureCube> skyboxTex;
    if (m_Environment.IBLEnvironment && m_Environment.IBLEnvironment->EnvMap)
        skyboxTex = m_Environment.IBLEnvironment->EnvMap;
    else if (m_Environment.Skybox)
        skyboxTex = m_Environment.Skybox;
    else
        return;

    auto skyboxShader = m_ShaderLibrary.Get("skybox");
    if (!skyboxShader) return;
    if (!m_CubeMesh || !m_CubeMesh->GetVertexArray()) return;

    // 去除平移部分，保留旋转
    glm::mat4 view = glm::mat4(glm::mat3(m_ViewMatrix));

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    skyboxShader->Bind();
    skyboxShader->SetInt("u_EnvironmentMap", 0);
    skyboxShader->SetMat4("view", view);
    skyboxShader->SetMat4("projection", m_ProjectionMatrix);
    skyboxShader->SetFloat("u_Intensity", m_Environment.EnvironmentIntensity);
    skyboxShader->SetFloat("u_Lod", m_Settings.SkyboxLOD);

    skyboxTex->Bind(0);
    auto vao = m_CubeMesh->GetVertexArray();
    vao->Bind();

    const auto& submeshes = m_CubeMesh->GetSubmeshes();
    if (!submeshes.empty()) {
        const auto& submesh = submeshes[0];
        glDrawElementsBaseVertex(GL_TRIANGLES, submesh.IndexCount,
                                  GL_UNSIGNED_INT,
                                  (void*)(submesh.BaseIndex * sizeof(uint32_t)),
                                  submesh.BaseVertex);
    }
    vao->Unbind();

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void SceneRenderer::DepthPrePass() {
    if (m_OpaqueDrawList.empty()) return;

    auto shader = m_ShaderLibrary.Get("depth_prepass");
    if (!shader) return;

    // 渲染到当前绑定的 HDR FBO（与 GeometryPass 共用同一深度缓冲）
    // 屏蔽所有颜色写入，只写深度
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    RenderCommand::EnableDepthTest();
    RenderCommand::SetDepthMask(true);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);

    shader->Bind();

    // 使用 BVH 裁剪（与 GeometryPass 保持同一可见集合）
    std::vector<int> visibleIndices;
    m_SceneBVH.Query(m_CameraFrustum, visibleIndices);

    for (int idx : visibleIndices) {
        const auto& cmd = m_OpaqueDrawList[idx];
        if (!cmd.MeshSource) continue;
        auto vao = cmd.MeshSource->GetVertexArray();
        if (!vao) continue;

        shader->SetMat4("u_Model", cmd.Transform);

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

    shader->Unbind();

    // 恢复颜色写入
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void SceneRenderer::GeometryPass() {
    if (m_OpaqueDrawList.empty()) return;

    auto pbrShader = m_ShaderLibrary.Get("pbr");
    if (!pbrShader) {
        // fallback to lit shader
        pbrShader = m_ShaderLibrary.Get("lit");
        if (!pbrShader) return;
    }

    // 深度预写已完成时切换为 LEQUAL + 禁止深度写入，消除 overdraw
    const bool prepassDone = m_Settings.EnableDepthPrepass
                             && m_ShaderLibrary.Get("depth_prepass");
    RenderCommand::EnableDepthTest();
    if (prepassDone) {
        glDepthFunc(GL_LEQUAL);
        RenderCommand::SetDepthMask(false);
    } else {
        glDepthFunc(GL_LESS);
        RenderCommand::SetDepthMask(true);
    }
    glDisable(GL_CULL_FACE);

    pbrShader->Bind();
    m_OpaquePipeline->SetShader(pbrShader);

    // IBL 纹理绑定 (slots 6, 7, 8 - 不与材质贴图 0~5 冲突)
    if (m_Environment.IBLEnvironment && m_Environment.IBLEnvironment->IsValid()) {
        m_Environment.IBLEnvironment->IrradianceMap->Bind(TextureSlot::Irradiance);
        m_Environment.IBLEnvironment->RadianceMap->Bind(TextureSlot::Prefilter);
        pbrShader->SetInt("u_IrradianceMap", static_cast<int>(TextureSlot::Irradiance));
        pbrShader->SetInt("u_PrefilterMap",  static_cast<int>(TextureSlot::Prefilter));
        pbrShader->SetFloat("u_EnvironmentIntensity", m_Environment.EnvironmentIntensity);
    } else {
        // 无 IBL 时关闭，PBR shader 会回退到简单环境光
        pbrShader->SetFloat("u_EnvironmentIntensity", 0.0f);
    }
    // BRDF LUT 始终绑定 (使用预烘焙文件)
    auto brdfLut = IBLProcessor::GetBRDFLUT();
    if (brdfLut) {
        brdfLut->Bind(TextureSlot::BrdfLUT);
        pbrShader->SetInt("u_BrdfLUT", static_cast<int>(TextureSlot::BrdfLUT));
    }

    // 阴影贴图绑定（slot 9，sampler2DArrayShadow）
    if (m_ShadowMap && m_ShadowMap->IsValid()) {
        m_ShadowMap->BindForReading(SHADOW_MAP_SLOT);
        pbrShader->SetInt("u_ShadowMap", static_cast<int>(SHADOW_MAP_SLOT));
    }
    pbrShader->SetBool("u_SoftShadows",  m_Settings.SoftShadows);
    pbrShader->SetBool("u_ShowCascades", m_Settings.ShowCascades);

    // Tiled Forward+：绑定 SSBO 并传入 tile 数量
    if (m_Settings.EnableTiledLighting && m_PointLightSSBO) {
        m_PointLightSSBO->BindBase(SSBO_BINDING_POINT_LIGHTS);
        m_TileLightCountSSBO->BindBase(SSBO_BINDING_TILE_LIGHT_COUNTS);
        m_TileLightIndexSSBO->BindBase(SSBO_BINDING_TILE_LIGHT_INDICES);
        pbrShader->SetIVec2("u_TileCount", glm::ivec2(m_TilesX, m_TilesY));
    }

    // 点光源阴影数组（slot 11，samplerCubeArray）
    if (m_Settings.EnablePointShadows && m_PointShadowArray && m_PointShadowArray->IsValid()) {
        m_PointShadowArray->BindForReading(POINT_SHADOW_MAP_SLOT);
        pbrShader->SetInt("u_PointShadowMaps", static_cast<int>(POINT_SHADOW_MAP_SLOT));
    }

    // SSAO（slot 10）：禁用时绑白色纹理（1.0），ao *= 1.0 不改变结果，无需 branch
    glActiveTexture(GL_TEXTURE0 + SSAO_TEXTURE_SLOT);
    glBindTexture(GL_TEXTURE_2D,
        (m_Settings.EnableSSAO && m_SSAOBlurFBO)
            ? m_SSAOBlurFBO->GetColorAttachment()
            : Renderer::GetWhiteTexture()->GetID());
    pbrShader->SetInt("u_SSAOMap", static_cast<int>(SSAO_TEXTURE_SLOT));

    std::vector<int> visibleIndices;
    m_SceneBVH.Query(m_CameraFrustum, visibleIndices);

    m_CullingStats.TotalObjects   = static_cast<int>(m_OpaqueDrawList.size());
    m_CullingStats.VisibleObjects = static_cast<int>(visibleIndices.size());
    m_CullingStats.CulledObjects  = m_CullingStats.TotalObjects - m_CullingStats.VisibleObjects;
    m_CullingStats.BVHNodeCount   = m_SceneBVH.NodeCount();

    for (int idx : visibleIndices) {
        const auto& cmd = m_OpaqueDrawList[idx];
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

    // PrePass 改变了深度函数和深度写入，恢复默认值供后续 pass 使用
    if (prepassDone) {
        glDepthFunc(GL_LESS);
        RenderCommand::SetDepthMask(true);
    }

    glBindVertexArray(0);
    glUseProgram(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void SceneRenderer::GBufferPass() {
    auto gbufferShader = m_ShaderLibrary.Get("gbuffer");
    if (!gbufferShader) {
        std::cerr << "[SceneRenderer] gbuffer shader not found!" << std::endl;
        return;
    }

    m_GBuffer->Bind();
    RenderCommand::SetViewport(0, 0, m_GBuffer->GetWidth(), m_GBuffer->GetHeight());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    m_GBuffer->ClearEntityID();

    if (m_OpaqueDrawList.empty()) {
        m_GBuffer->Unbind();
        return;
    }

    RenderCommand::EnableDepthTest();
    glDepthFunc(GL_LESS);
    RenderCommand::SetDepthMask(true);
    glDisable(GL_CULL_FACE);

    gbufferShader->Bind();

    std::vector<int> visibleIndices;
    m_SceneBVH.Query(m_CameraFrustum, visibleIndices);

    m_CullingStats.TotalObjects   = static_cast<int>(m_OpaqueDrawList.size());
    m_CullingStats.VisibleObjects = static_cast<int>(visibleIndices.size());
    m_CullingStats.CulledObjects  = m_CullingStats.TotalObjects - m_CullingStats.VisibleObjects;
    m_CullingStats.BVHNodeCount   = m_SceneBVH.NodeCount();

    for (int idx : visibleIndices) {
        const auto& cmd = m_OpaqueDrawList[idx];
        if (!cmd.MeshSource) continue;

        auto vao = cmd.MeshSource->GetVertexArray();
        if (!vao) continue;

        auto material = GetMaterialForDrawCommand(cmd);
        BindPBRMaterial(*gbufferShader, material);

        gbufferShader->SetMat4("u_Model", cmd.Transform);
        gbufferShader->SetMat3("u_NormalMatrix", cmd.NormalMatrix);
        gbufferShader->SetInt("u_EntityID", cmd.EntityID);

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

    gbufferShader->Unbind();
    m_GBuffer->Unbind();
}

void SceneRenderer::DeferredLightingPass() {
    auto shader = m_ShaderLibrary.Get("deferred_lighting");
    if (!shader) {
        std::cerr << "[SceneRenderer] deferred_lighting shader not found!" << std::endl;
        return;
    }

    RenderCommand::DisableDepthTest();

    shader->Bind();

    // G-Buffer 纹理：slot 0-3
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetBaseColorAO());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetNormalRoughMetal());
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetEmissiveShadingID());
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetDepth());

    shader->SetInt("u_GBufBaseColorAO",      0);
    shader->SetInt("u_GBufNormalRoughMetal",  1);
    shader->SetInt("u_GBufEmissiveShadingID", 2);
    shader->SetInt("u_GBufDepth",             3);

    // InvViewProjection（重建世界坐标）
    glm::mat4 invVP = glm::inverse(m_ProjectionMatrix * m_ViewMatrix);
    shader->SetMat4("u_InvViewProjection", invVP);

    // IBL（slot 6, 7, 8）
    if (m_Environment.IBLEnvironment && m_Environment.IBLEnvironment->IsValid()) {
        m_Environment.IBLEnvironment->IrradianceMap->Bind(TextureSlot::Irradiance);
        m_Environment.IBLEnvironment->RadianceMap->Bind(TextureSlot::Prefilter);
        shader->SetInt("u_IrradianceMap", static_cast<int>(TextureSlot::Irradiance));
        shader->SetInt("u_PrefilterMap",  static_cast<int>(TextureSlot::Prefilter));
        shader->SetFloat("u_EnvironmentIntensity", m_Environment.EnvironmentIntensity);
    } else {
        shader->SetFloat("u_EnvironmentIntensity", 0.0f);
    }
    auto brdfLut = IBLProcessor::GetBRDFLUT();
    if (brdfLut) {
        brdfLut->Bind(TextureSlot::BrdfLUT);
        shader->SetInt("u_BrdfLUT", static_cast<int>(TextureSlot::BrdfLUT));
    }

    // 阴影贴图（slot 9）
    if (m_ShadowMap && m_ShadowMap->IsValid()) {
        m_ShadowMap->BindForReading(SHADOW_MAP_SLOT);
        shader->SetInt("u_ShadowMap", static_cast<int>(SHADOW_MAP_SLOT));
    }
    shader->SetBool("u_SoftShadows",  m_Settings.SoftShadows);
    shader->SetBool("u_ShowCascades", m_Settings.ShowCascades);

    // SSAO（slot 10）
    glActiveTexture(GL_TEXTURE0 + SSAO_TEXTURE_SLOT);
    glBindTexture(GL_TEXTURE_2D,
        (m_Settings.EnableSSAO && m_SSAOBlurFBO)
            ? m_SSAOBlurFBO->GetColorAttachment()
            : Renderer::GetWhiteTexture()->GetID());
    shader->SetInt("u_SSAOMap", static_cast<int>(SSAO_TEXTURE_SLOT));

    // 点光源阴影（slot 11）
    if (m_Settings.EnablePointShadows && m_PointShadowArray && m_PointShadowArray->IsValid()) {
        m_PointShadowArray->BindForReading(POINT_SHADOW_MAP_SLOT);
        shader->SetInt("u_PointShadowMaps", static_cast<int>(POINT_SHADOW_MAP_SLOT));
    }

    // Tiled Forward+ SSBO
    if (m_Settings.EnableTiledLighting && m_PointLightSSBO) {
        m_PointLightSSBO->BindBase(SSBO_BINDING_POINT_LIGHTS);
        m_TileLightCountSSBO->BindBase(SSBO_BINDING_TILE_LIGHT_COUNTS);
        m_TileLightIndexSSBO->BindBase(SSBO_BINDING_TILE_LIGHT_INDICES);
        shader->SetIVec2("u_TileCount", glm::ivec2(m_TilesX, m_TilesY));
    }

    Renderer::DrawFullscreenQuad();

    shader->Unbind();

    RenderCommand::EnableDepthTest();
}

void SceneRenderer::WireframeOverlayPass() {
    if (m_OpaqueDrawList.empty()) return;

    auto shader = m_ShaderLibrary.Get("wireframe");
    if (!shader) return;

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    RenderCommand::EnableDepthTest();
    glDepthFunc(GL_LEQUAL);
    RenderCommand::SetDepthMask(false);
    glDisable(GL_CULL_FACE);

    shader->Bind();
    shader->SetMat4("u_View",       m_ViewMatrix);
    shader->SetMat4("u_Projection", m_ProjectionMatrix);
    shader->SetVec3("u_Color",      glm::vec3(0.8f));

    for (const auto& cmd : m_OpaqueDrawList) {
        if (!cmd.MeshSource) continue;
        auto vao = cmd.MeshSource->GetVertexArray();
        if (!vao) continue;

        shader->SetMat4("u_Model",    cmd.Transform);
        shader->SetInt ("u_EntityID", cmd.EntityID);

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

    shader->Unbind();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDepthFunc(GL_LESS);
    RenderCommand::SetDepthMask(true);
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

void SceneRenderer::ParticlePass() {
    if (m_ParticleDrawList.empty()) return;

    auto renderShader = m_ShaderLibrary.Get("particle_render");
    if (!renderShader) return;

    // === Compute：Emit → Update → Compact ===
    for (auto& entry : m_ParticleDrawList) {
        if (!entry.System) continue;
        entry.System->Simulate(entry.Params,
            m_ParticleEmitPipeline,
            m_ParticleUpdatePipeline,
            m_ParticleCompactPipeline);
    }

    // === Render：加法混合 Billboard ===
    m_HDRFramebuffer->Bind();

    RenderCommand::EnableDepthTest();
    RenderCommand::SetDepthMask(false);          // 不写深度（粒子间不互相遮挡）
    RenderCommand::EnableBlending();
    RenderCommand::SetBlendFunc(GL_SRC_ALPHA, GL_ONE);  // 加法混合
    glDisable(GL_CULL_FACE);

    renderShader->Bind();
    renderShader->SetMat4("u_ViewProjection",
        m_ProjectionMatrix * m_ViewMatrix);
    renderShader->SetMat4("u_View", m_ViewMatrix);
    renderShader->SetInt("u_HasTexture", 0);

    for (auto& entry : m_ParticleDrawList) {
        if (!entry.System) continue;
        entry.System->BindForRender();
        glDrawArraysIndirect(GL_TRIANGLES, nullptr);
    }

    // 恢复状态
    glBindVertexArray(0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    RenderCommand::SetDepthMask(true);
    RenderCommand::DisableBlending();
    RenderCommand::EnableDepthTest();

    renderShader->Unbind();
}

void SceneRenderer::EnsureTiledLightBuffers(int numTilesX, int numTilesY) {
    int numTiles = numTilesX * numTilesY;
    size_t countSize = sizeof(int) * numTiles;
    size_t indexSize = sizeof(int) * numTiles * MAX_LIGHTS_PER_TILE;

    if (!m_TileLightCountSSBO || m_TileLightCountSSBO->GetSize() < countSize)
        m_TileLightCountSSBO = StorageBuffer::Create(countSize);

    if (!m_TileLightIndexSSBO || m_TileLightIndexSSBO->GetSize() < indexSize)
        m_TileLightIndexSSBO = StorageBuffer::Create(indexSize);
}

void SceneRenderer::UploadPointLightSSBO() {
    int count = std::min(m_Environment.PointLightCount, MAX_POINT_LIGHTS);
    std::vector<PointLightGPU> gpuLights(count);
    for (int i = 0; i < count; ++i) {
        const auto& l = m_Environment.PointLights[i];

        // 与 PointLightComponent::GetRange() 一致：
        // 找到衰减函数 1/(c+l*d+q*d²) 降至 threshold 时的距离
        // threshold = 5/256，解二次方程：d = (-L + sqrt(L²-4Q(C-bright/thresh))) / 2Q
        float maxBright = std::max({l.Diffuse.r, l.Diffuse.g, l.Diffuse.b});
        float radius    = 10.0f;  // 保底值
        if (l.Quadratic > 0.0f) {
            constexpr float threshold = 5.0f / 256.0f;
            float disc = l.Linear * l.Linear
                       - 4.0f * l.Quadratic * (l.Constant - maxBright / threshold);
            if (disc >= 0.0f)
                radius = (-l.Linear + std::sqrt(disc)) / (2.0f * l.Quadratic);
        }

        gpuLights[i].PosRadius      = glm::vec4(l.Position, radius);
        gpuLights[i].ColorIntensity = glm::vec4(l.Diffuse, 1.0f);

        // shadowSlot: 在 m_PointShadowAssignments[slot]=lightIndex 中反向查找
        // O(MAX_SHADOW_POINT_LIGHTS=4)，完全可接受
        float shadowSlot = -1.0f;
        float farPlane   = l.ShadowFarPlane > 0.0f ? l.ShadowFarPlane
                                                    : m_Settings.PointShadowFarPlane;
        if (m_Settings.EnablePointShadows && l.CastShadows) {
            for (int s = 0; s < MAX_SHADOW_POINT_LIGHTS; s++) {
                if (m_PointShadowAssignments[s] == i) {
                    shadowSlot = static_cast<float>(s);
                    break;
                }
            }
        }

        gpuLights[i].Params = glm::vec4(
            0.5f,        // x = falloff
            shadowSlot,  // y = shadowSlot (-1 = 无阴影)
            farPlane,    // z = per-light farPlane
            0.0f
        );
    }
    if (count > 0)
        m_PointLightSSBO->SetData(gpuLights.data(), sizeof(PointLightGPU) * count);
}

void SceneRenderer::TiledLightCullPass() {
    if (!m_TileLightCullPipeline || !m_TileLightCullPipeline->IsValid()) return;
    if (!m_HDRFramebuffer) return;
    if (m_SceneDepthTexture == 0) return;  // DepthPrePass 未执行

    uint32_t W = m_HDRFramebuffer->GetWidth();
    uint32_t H = m_HDRFramebuffer->GetHeight();
    m_TilesX = (W + TILE_SIZE - 1) / TILE_SIZE;
    m_TilesY = (H + TILE_SIZE - 1) / TILE_SIZE;

    EnsureTiledLightBuffers(m_TilesX, m_TilesY);

    // 绑定 SSBO（点光源数据已在 FlushDrawList 开头上传）
    m_PointLightSSBO->BindBase(SSBO_BINDING_POINT_LIGHTS);
    m_TileLightCountSSBO->BindBase(SSBO_BINDING_TILE_LIGHT_COUNTS);
    m_TileLightIndexSSBO->BindBase(SSBO_BINDING_TILE_LIGHT_INDICES);

    m_TileLightCullPipeline->Bind();
    // CameraData UBO（binding=0）由 glBindBufferBase 全局生效，无需手动绑定
    m_TileLightCullPipeline->SetIVec2("u_ScreenSize",      glm::ivec2(W, H));
    m_TileLightCullPipeline->SetInt  ("u_PointLightCount", m_Environment.PointLightCount);
    m_TileLightCullPipeline->SetFloat("u_NearPlane",       m_CameraNear);
    m_TileLightCullPipeline->SetFloat("u_FarPlane",        m_CameraFar);

    // 绑定场景深度纹理（m_SceneDepthTexture 由 FlushDrawList 在 DepthPrePass 后赋值）
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_SceneDepthTexture);
    m_TileLightCullPipeline->SetInt("u_DepthTex", 0);

    m_TileLightCullPipeline->DispatchAndWait(
        static_cast<uint32_t>(m_TilesX),
        static_cast<uint32_t>(m_TilesY),
        1
    );
    m_TileLightCullPipeline->Unbind();
}

void SceneRenderer::UpdateLightingUBO() {
    LightingUBO lightData = {};

    // 方向光
    lightData.DirLightDirection = glm::vec4(m_Environment.DirLight.Direction, 1.0f);
    lightData.DirLightColor     = glm::vec4(m_Environment.DirLight.Diffuse, 0.0f);

    // 点光源数量（数据已在 UploadPointLightSSBO 中上传至 SSBO）
    lightData.LightCounts.x = std::min(m_Environment.PointLightCount, MAX_POINT_LIGHTS);

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

void SceneRenderer::CalculateCascades(CascadeData* outCascades) {
    const uint32_t count  = m_Settings.ShadowCascadeCount;
    const float    near   = m_CameraNear;
    const float    far    = glm::min(m_CameraFar, m_Settings.ShadowDistance);
    const float    lambda = m_Settings.CascadeSplitLambda;
    const float    ratio  = far / near;

    // 光源方向（单位向量，从光源指向场景）
    glm::vec3 lightDir = glm::normalize(m_Environment.DirLight.Direction);
    glm::vec3 up = (glm::abs(lightDir.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);

    // 计算相机视锥的 8 个世界空间角点（用于后续插值出每个 cascade 的子视锥）
    glm::mat4 invVP = glm::inverse(m_ProjectionMatrix * m_ViewMatrix);
    glm::vec4 ndcCorners[8] = {
        {-1,-1,-1,1}, { 1,-1,-1,1}, { 1, 1,-1,1}, {-1, 1,-1,1},  // near plane
        {-1,-1, 1,1}, { 1,-1, 1,1}, { 1, 1, 1,1}, {-1, 1, 1,1},  // far plane
    };
    glm::vec3 worldCorners[8];
    for (int j = 0; j < 8; j++) {
        glm::vec4 ws = invVP * ndcCorners[j];
        worldCorners[j] = glm::vec3(ws) / ws.w;
    }

    // log-uniform 混合分割（GPU Gems 3, Ch.10）
    float splits[4] = {};
    for (uint32_t i = 0; i < count; i++) {
        float p         = float(i + 1) / float(count);
        float logSplit  = near * glm::pow(ratio, p);
        float uniSplit  = near + (far - near) * p;
        splits[i]       = lambda * logSplit + (1.0f - lambda) * uniSplit;
    }

    // 为每个 cascade 计算正交投影
    for (uint32_t i = 0; i < count; i++) {
        float prevSplit = (i == 0) ? near : splits[i - 1];
        float thisSplit = splits[i];

        // 在完整视锥角点上插值，得到该 cascade 子视锥的 8 个角点
        float prevFrac = (prevSplit - near) / (far - near);
        float thisFrac = (thisSplit - near) / (far - near);
        glm::vec3 corners[8];
        for (int j = 0; j < 4; j++) {
            glm::vec3 ray  = worldCorners[j + 4] - worldCorners[j];
            corners[j]     = worldCorners[j] + ray * prevFrac;
            corners[j + 4] = worldCorners[j] + ray * thisFrac;
        }

        // 包围球方法（参考 Hazel）：
        // 求子视锥 8 个角点的中心，再找最大半径，得到一个不随相机旋转而改变的球。
        // 正交投影覆盖 [-radius, radius] x [-radius, radius]，大小固定，
        // texel-snap 可以完全消除平移和旋转引起的阴影抖动。
        glm::vec3 center(0.0f);
        for (auto& c : corners) center += c;
        center /= 8.0f;

        float radius = 0.0f;
        for (auto& c : corners)
            radius = glm::max(radius, glm::length(c - center));
        // 向上取整到 1/16，压制浮点误差引起的微小尺寸抖动
        radius = std::ceil(radius * 16.0f) / 16.0f;

        // 光源眼睛放在球体边界（center 沿光源反方向 radius 处），near=0 恰好在眼睛位置，
        // far 延伸额外 zFarExtend 以捕获球外的 shadow caster
        constexpr float zFarExtend = 50.0f;
        glm::mat4 lightView  = glm::lookAt(center - lightDir * radius, center, up);
        glm::mat4 lightOrtho = glm::ortho(-radius, radius, -radius, radius,
                                           0.0f, 2.0f * radius + zFarExtend);

        // Texel-snap：把光源原点对齐到 shadow map texel 网格，消除相机移动时的阴影抖动
        glm::mat4 lightVP = lightOrtho * lightView;
        glm::vec4 origin  = lightVP * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        float     res     = float(m_Settings.ShadowResolution);
        origin *= res * 0.5f;
        glm::vec4 rounded = glm::round(origin);
        glm::vec4 snapOff = (rounded - origin) * 2.0f / res;
        snapOff.z = 0.0f;
        snapOff.w = 0.0f;
        lightOrtho[3] += snapOff;

        outCascades[i].ViewProj      = lightOrtho * lightView;
        outCascades[i].SplitDepth    = thisSplit;
        outCascades[i].CascadeFrustum = Frustum::FromViewProjection(outCascades[i].ViewProj);
    }
}

void SceneRenderer::UpdateShadowUBO() {
    ShadowUBO data = {};
    const uint32_t count = m_Settings.ShadowCascadeCount;
    for (uint32_t i = 0; i < count && i < 4; i++) {
        data.LightSpaceMatrices[i] = m_Cascades[i].ViewProj;
        data.CascadeSplits[i]      = m_Cascades[i].SplitDepth;
    }
    data.MaxShadowDistance      = m_Settings.ShadowDistance;
    data.ShadowFade             = m_Settings.ShadowFade;
    data.CascadeCount           = static_cast<int>(count);
    data.CascadeTransitionFade  = m_Settings.CascadeTransitionFade;
    m_ShadowUBO->SetData(&data, sizeof(data));
}

void SceneRenderer::ShadowPass() {
    if (!m_ShadowMap || !m_ShadowMap->IsValid()) return;

    auto shadowShader = m_ShaderLibrary.Get("shadow_depth");
    if (!shadowShader) return;

    const uint32_t count = m_Settings.ShadowCascadeCount;
    const uint32_t res   = m_Settings.ShadowResolution;

    shadowShader->Bind();
    m_ShadowPipeline->ApplyRenderState();

    for (uint32_t c = 0; c < count; c++) {
        m_ShadowMap->BindForWriting(c);
        glViewport(0, 0, res, res);
        glClear(GL_DEPTH_BUFFER_BIT);

        shadowShader->SetMat4("u_LightSpaceMatrix", m_Cascades[c].ViewProj);

        for (int idx = 0; idx < static_cast<int>(m_OpaqueDrawList.size()); idx++) {
            const auto& cmd = m_OpaqueDrawList[idx];
            if (!cmd.MeshSource) continue;
            auto vao = cmd.MeshSource->GetVertexArray();
            if (!vao) continue;

            shadowShader->SetMat4("u_Transform", cmd.Transform);

            const auto& submeshes = cmd.MeshSource->GetSubmeshes();
            if (cmd.SubmeshIndex >= submeshes.size()) continue;
            const auto& submesh = submeshes[cmd.SubmeshIndex];

            vao->Bind();
            glDrawElementsBaseVertex(GL_TRIANGLES, submesh.IndexCount,
                GL_UNSIGNED_INT,
                (void*)(submesh.BaseIndex * sizeof(uint32_t)),
                submesh.BaseVertex);
            vao->Unbind();
        }
    }

    m_ShadowMap->Unbind();
    shadowShader->Unbind();
}

void SceneRenderer::PointShadowPass() {
    if (!m_PointShadowArray || !m_PointShadowArray->IsValid()) return;

    auto shader = m_ShaderLibrary.Get("point_shadow");
    if (!shader) return;

    // 重置所有 slot
    for (int i = 0; i < MAX_SHADOW_POINT_LIGHTS; i++)
        m_PointShadowAssignments[i] = -1;

    // 只为 CastShadows=true 的灯分配 slot（按顺序，最多 MAX_SHADOW_POINT_LIGHTS 个）
    int slot = 0;
    for (int i = 0; i < m_Environment.PointLightCount && slot < MAX_SHADOW_POINT_LIGHTS; i++) {
        if (m_Environment.PointLights[i].CastShadows)
            m_PointShadowAssignments[slot++] = i;
    }
    if (slot == 0) return;  // 无需投影的灯

    shader->Bind();
    m_ShadowPipeline->ApplyRenderState();

    const uint32_t res = m_Settings.PointShadowResolution;

    // OpenGL 右手系 Cubemap 面的目标方向和 Up 向量
    static const glm::vec3 faceTargets[6] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };
    static const glm::vec3 faceUps[6] = {
        { 0,-1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1},
        { 0,-1, 0}, { 0,-1, 0}
    };

    for (int s = 0; s < MAX_SHADOW_POINT_LIGHTS; s++) {
        int lightIndex = m_PointShadowAssignments[s];
        if (lightIndex < 0) break;  // slots are filled front-to-back

        const auto&    light    = m_Environment.PointLights[lightIndex];
        const glm::vec3 lightPos = light.Position;
        const float    farPlane  = light.ShadowFarPlane > 0.0f
                                   ? light.ShadowFarPlane
                                   : m_Settings.PointShadowFarPlane;

        const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, farPlane);

        shader->SetVec3 ("u_LightPos",  lightPos);
        shader->SetFloat("u_FarPlane",  farPlane);

        for (int face = 0; face < 6; face++) {
            m_PointShadowArray->BindFaceForWriting(s, face);
            glViewport(0, 0, res, res);
            glClear(GL_DEPTH_BUFFER_BIT);

            glm::mat4 view = glm::lookAt(lightPos,
                                         lightPos + faceTargets[face],
                                         faceUps[face]);
            shader->SetMat4("u_LightSpaceMatrix", proj * view);

            for (const auto& cmd : m_OpaqueDrawList) {
                if (!cmd.MeshSource) continue;
                auto vao = cmd.MeshSource->GetVertexArray();
                if (!vao) continue;

                shader->SetMat4("u_Model", cmd.Transform);

                const auto& submeshes = cmd.MeshSource->GetSubmeshes();
                if (cmd.SubmeshIndex >= submeshes.size()) continue;
                const auto& submesh = submeshes[cmd.SubmeshIndex];

                vao->Bind();
                glDrawElementsBaseVertex(GL_TRIANGLES, submesh.IndexCount,
                    GL_UNSIGNED_INT,
                    (void*)(submesh.BaseIndex * sizeof(uint32_t)),
                    submesh.BaseVertex);
                vao->Unbind();
            }
        }
    }

    m_PointShadowArray->Unbind();
    shader->Unbind();
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
    m_CubeMesh = AssetManager::Get().GetAsset<MeshSource>(cubeHandle);

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

    // 如果没有方向光实体，使用默认方向光（PBR 需要足够亮度，建议场景中设置 3-5 强度）
    if (!foundDirLight) {
        m_Environment.DirLight.Direction = glm::vec3(-0.2f, -1.0f, -0.3f);
        m_Environment.DirLight.Ambient = glm::vec3(0.1f);
        m_Environment.DirLight.Diffuse = glm::vec3(1.0f);   // 默认漫反射光
        m_Environment.DirLight.Specular = glm::vec3(1.0f);  // 默认镜面反射光
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
                m_Environment.PointLights[idx].Constant      = light.Constant;
                m_Environment.PointLights[idx].Linear        = light.Linear;
                m_Environment.PointLights[idx].Quadratic     = light.Quadratic;
                m_Environment.PointLights[idx].CastShadows   = light.CastShadows;
                m_Environment.PointLights[idx].ShadowFarPlane = light.ShadowFarPlane;
            }
        }
    );
}

bool SceneRenderer::LoadEnvironment(const std::filesystem::path& hdrPath) {
    auto processor = IBLProcessor::Create();
    if (!processor->Load(hdrPath)) {
        std::cerr << "[SceneRenderer] 环境贴图加载失败: " << hdrPath << std::endl;
        return false;
    }

    m_Environment.IBLEnvironment = Ref<Environment>(new Environment(
        processor->GetEnvMap(),
        processor->GetRadianceMap(),
        processor->GetIrradianceMap()
    ));

    // 启用天空盒并重置 LOD
    m_Settings.ShowSkybox = true;
    m_Settings.SkyboxLOD = 0.0f;

    std::cout << "[SceneRenderer] 环境加载完成: " << hdrPath.filename().string() << std::endl;
    return true;
}

void SceneRenderer::EnsureHDRFramebuffer(uint32_t width, uint32_t height) {
    if (m_HDRFramebuffer &&
        m_HDRFramebuffer->GetWidth()  == width &&
        m_HDRFramebuffer->GetHeight() == height) {
        return;  // 尺寸未变，无需重建
    }

    FramebufferSpec spec;
    spec.Width                = width;
    spec.Height               = height;
    spec.ColorFormat          = GL_RGBA16F;   // HDR，存储 >1.0 的线性值
    spec.HasColorAttachment   = true;
    spec.HasDepthAttachment   = true;
    spec.DepthAsTexture       = false;
    spec.HasEntityIDAttachment = true;        // 鼠标拾取用
    spec.Samples              = 1;

    m_HDRFramebuffer = Framebuffer::Create(spec);
    std::cout << "[SceneRenderer] HDR Framebuffer created: " << width << "x" << height << std::endl;

    // G-Buffer 与 HDR FBO 始终保持同尺寸
    if (!m_GBuffer || m_GBuffer->GetWidth() != width || m_GBuffer->GetHeight() != height)
        m_GBuffer = GBuffer::Create({ width, height });

    // SSR 资源与 HDR FBO 同尺寸一起维护
    EnsureSSRResources(width, height);
}

void SceneRenderer::EnsureSSRResources(uint32_t width, uint32_t height) {
    const int w = static_cast<int>(width);
    const int h = static_cast<int>(height);

    // SSR 输出尺寸：半分辨率时为 w/2 × h/2，全分辨率时与 viewport 一致
    const int ssrW = m_Settings.SSRHalfResolution ? std::max(w / 2, 1) : w;
    const int ssrH = m_Settings.SSRHalfResolution ? std::max(h / 2, 1) : h;

    // Hi-Z：R32F，完整 mip 链，GL_NEAREST_MIPMAP_NEAREST（禁止插值，保证 MIN 语义）
    // mip 数 = floor(log2(max(w,h))) + 1
    int mipCount = 1;
    {
        int dim = std::max(w, h);
        while (dim > 1) { dim >>= 1; ++mipCount; }
    }

    if (m_HiZTex == 0 || m_HiZTexWidth != w || m_HiZTexHeight != h) {
        if (m_HiZTex) glDeleteTextures(1, &m_HiZTex);
        glGenTextures(1, &m_HiZTex);
        glBindTexture(GL_TEXTURE_2D, m_HiZTex);
        // glTexStorage2D 一次性分配所有 mip 层，格式不可变（immutable）
        // 工程注意：必须用 glTexStorage2D 而非 glTexImage2D，
        //   否则 glBindImageTexture 针对特定 mip 的绑定在部分驱动上不稳定
        glTexStorage2D(GL_TEXTURE_2D, mipCount, GL_R32F, w, h);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_HiZTexWidth  = w;
        m_HiZTexHeight = h;
        m_HiZMipCount  = mipCount;
        std::cout << "[SceneRenderer] Hi-Z texture created: " << w << "x" << h
                  << " (" << mipCount << " mips)\n";
    }

    // SSR 输出：RGBA16F，单 mip（半分辨率时为 ssrW × ssrH）
    if (m_SSROutputTex == 0 || m_SSROutputWidth != ssrW || m_SSROutputHeight != ssrH) {
        if (m_SSROutputTex) glDeleteTextures(1, &m_SSROutputTex);
        glGenTextures(1, &m_SSROutputTex);
        glBindTexture(GL_TEXTURE_2D, m_SSROutputTex);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, ssrW, ssrH);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_SSROutputWidth  = ssrW;
        m_SSROutputHeight = ssrH;
    }

    // 场景颜色金字塔（ConeTracing 用）：RGBA16F，全分辨率，完整 mip 链
    // 每帧在 SSRPass 中从 HDR color attachment 拷贝 mip0，再 glGenerateMipmap
    if (m_SSRColorPyramid == 0 || m_SSRColorPyramidWidth != w || m_SSRColorPyramidHeight != h) {
        if (m_SSRColorPyramid) glDeleteTextures(1, &m_SSRColorPyramid);

        int pyrMips = 1;
        { int dim = std::max(w, h); while (dim > 1) { dim >>= 1; ++pyrMips; } }

        glGenTextures(1, &m_SSRColorPyramid);
        glBindTexture(GL_TEXTURE_2D, m_SSRColorPyramid);
        glTexStorage2D(GL_TEXTURE_2D, pyrMips, GL_RGBA16F, w, h);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_SSRColorPyramidWidth  = w;
        m_SSRColorPyramidHeight = h;
        m_SSRColorPyramidMips   = pyrMips;
    }
}

void SceneRenderer::EnsureBackFaceDepthTex(int w, int h)
{
    if (m_BackFaceDepthTex && m_BackFaceDepthW == w && m_BackFaceDepthH == h) return;

    if (m_BackFaceDepthFBO) { glDeleteFramebuffers(1, &m_BackFaceDepthFBO); m_BackFaceDepthFBO = 0; }
    if (m_BackFaceDepthTex) { glDeleteTextures(1, &m_BackFaceDepthTex);     m_BackFaceDepthTex = 0; }

    glGenTextures(1, &m_BackFaceDepthTex);
    glBindTexture(GL_TEXTURE_2D, m_BackFaceDepthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_BackFaceDepthFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_BackFaceDepthFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_BackFaceDepthTex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_BackFaceDepthW = w;
    m_BackFaceDepthH = h;
}

void SceneRenderer::BackFaceDepthPass()
{
    EnsureBackFaceDepthTex(m_HiZTexWidth, m_HiZTexHeight);

    glBindFramebuffer(GL_FRAMEBUFFER, m_BackFaceDepthFBO);
    glViewport(0, 0, m_HiZTexWidth, m_HiZTexHeight);
    glClear(GL_DEPTH_BUFFER_BIT);

    if (m_OpaqueDrawList.empty()) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_HDRFramebuffer->GetID());
        return;
    }
    auto shader = m_ShaderLibrary.Get("depth_prepass");
    if (!shader) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_HDRFramebuffer->GetID());
        return;
    }
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    RenderCommand::EnableDepthTest();
    RenderCommand::SetDepthMask(true);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    shader->Bind();
    for (const auto& cmd : m_OpaqueDrawList) {
        if (!cmd.MeshSource) continue;
        auto vao = cmd.MeshSource->GetVertexArray();
        if (!vao) continue;
        shader->SetMat4("u_Model", cmd.Transform);
        const auto& submeshes = cmd.MeshSource->GetSubmeshes();
        if (cmd.SubmeshIndex >= submeshes.size()) continue;
        const auto& sm = submeshes[cmd.SubmeshIndex];
        vao->Bind();
        glDrawElementsBaseVertex(GL_TRIANGLES, sm.IndexCount, GL_UNSIGNED_INT,
            (void*)(sm.BaseIndex * sizeof(uint32_t)), sm.BaseVertex);
        vao->Unbind();
    }
    shader->Unbind();

    glCullFace(GL_BACK);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // 恢复 HDR FBO（SSRPass 为 compute，不需要 FBO，但后续 pass 需要）
    glBindFramebuffer(GL_FRAMEBUFFER, m_HDRFramebuffer->GetID());
}

void SceneRenderer::HiZBuildPass() {
    if (!m_HiZPipeline || !m_HiZPipeline->IsValid() || !m_HiZTex) return;
    if (!m_Settings.UseDeferredShading || !m_GBuffer) return;

    m_HiZPipeline->Bind();

    const int w = m_HiZTexWidth;
    const int h = m_HiZTexHeight;

    // Pass 0：从 GBuffer 深度（DEPTH24_STENCIL8）拷贝到 HiZ mip 0
    // DEPTH24_STENCIL8 在 GL_DEPTH_STENCIL_TEXTURE_MODE=GL_DEPTH_COMPONENT（默认）下
    // 可通过 sampler2D + texelFetch 读取 .r 深度分量
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetDepth());
    m_HiZPipeline->SetInt("u_InputDepth", 0);
    m_HiZPipeline->SetInt("u_SrcMip",     0);
    m_HiZPipeline->SetIVec2("u_SrcSize",  glm::ivec2(w, h));
    m_HiZPipeline->SetInt("u_IsFirstPass", 1);
    m_HiZPipeline->BindImageTexture(0, m_HiZTex, 0, GL_WRITE_ONLY, GL_R32F);
    m_HiZPipeline->Dispatch((w + 7) / 8, (h + 7) / 8, 1);

    // 每次 Dispatch 后必须 barrier，确保写入对后续 texelFetch 可见
    ComputePipeline::Wait(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                          GL_TEXTURE_FETCH_BARRIER_BIT);

    // Pass 1..N：2×2 MIN 下采样（对 HiZ 自身逐级构建）
    glBindTexture(GL_TEXTURE_2D, m_HiZTex);
    m_HiZPipeline->SetInt("u_IsFirstPass", 0);

    for (int mip = 1; mip < m_HiZMipCount; ++mip) {
        const int srcW = std::max(1, w >> (mip - 1));
        const int srcH = std::max(1, h >> (mip - 1));
        const int dstW = std::max(1, w >> mip);
        const int dstH = std::max(1, h >> mip);

        m_HiZPipeline->SetInt("u_SrcMip",  mip - 1);
        m_HiZPipeline->SetIVec2("u_SrcSize", glm::ivec2(srcW, srcH));
        m_HiZPipeline->BindImageTexture(0, m_HiZTex, mip, GL_WRITE_ONLY, GL_R32F);
        m_HiZPipeline->Dispatch((dstW + 7) / 8, (dstH + 7) / 8, 1);

        ComputePipeline::Wait(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                              GL_TEXTURE_FETCH_BARRIER_BIT);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    m_HiZPipeline->Unbind();
}

void SceneRenderer::SSRPass() {
    if (!m_SSRPipeline || !m_SSRPipeline->IsValid()) return;
    if (!m_HiZTex) return;
    if (!m_Settings.UseDeferredShading || !m_GBuffer) return;

    // 响应运行时半分辨率切换（EnsureHDRFramebuffer 只在 resize 时触发）
    EnsureSSRResources(m_HiZTexWidth, m_HiZTexHeight);
    if (!m_SSROutputTex) return;

    const int w = m_SSROutputWidth;
    const int h = m_SSROutputHeight;

    m_SSRPipeline->Bind();

    // GBuffer 输入
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetDepth());
    m_SSRPipeline->SetInt("u_GDepth", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetNormalRoughMetal());
    m_SSRPipeline->SetInt("u_GNormal", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_GBuffer->GetBaseColorAO());
    m_SSRPipeline->SetInt("u_GBaseColor", 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_HiZTex);
    m_SSRPipeline->SetInt("u_HiZ", 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_HDRFramebuffer->GetColorAttachment());
    m_SSRPipeline->SetInt("u_SceneColor", 4);

    // 场景颜色金字塔：从 HDR color attachment 拷贝 mip0，再生成完整 mip 链
    // glCopyImageSubData：GPU-to-GPU 直接拷贝，无需 framebuffer blit
    glCopyImageSubData(
        m_HDRFramebuffer->GetColorAttachment(), GL_TEXTURE_2D, 0, 0, 0, 0,
        m_SSRColorPyramid,                      GL_TEXTURE_2D, 0, 0, 0, 0,
        m_HiZTexWidth, m_HiZTexHeight, 1);
    glBindTexture(GL_TEXTURE_2D, m_SSRColorPyramid);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, m_SSRColorPyramid);
    m_SSRPipeline->SetInt("u_SceneColorMip",      5);
    m_SSRPipeline->SetInt("u_SceneColorMipCount", m_SSRColorPyramidMips);

    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, m_BackFaceDepthTex ? m_BackFaceDepthTex : m_HiZTex);
    m_SSRPipeline->SetInt("u_BackFaceDepth", 6);

    // 相机矩阵
    m_SSRPipeline->SetMat4("u_View",          m_ViewMatrix);
    m_SSRPipeline->SetMat4("u_Projection",    m_ProjectionMatrix);
    m_SSRPipeline->SetMat4("u_InvProjection", glm::inverse(m_ProjectionMatrix));
    // u_Resolution 始终为全分辨率（HiZ 和 GBuffer 均为全分辨率，射线坐标系基于此）
    m_SSRPipeline->SetVec2("u_Resolution",    glm::vec2(m_HiZTexWidth, m_HiZTexHeight));
    m_SSRPipeline->SetFloat("u_Near",         m_CameraNear);
    m_SSRPipeline->SetFloat("u_Far",          m_CameraFar);

    // SSR 参数
    m_SSRPipeline->SetInt  ("u_MaxSteps",        m_Settings.SSRMaxSteps);
    m_SSRPipeline->SetFloat("u_DepthTolerance",  m_Settings.SSRDepthTolerance);
    m_SSRPipeline->SetFloat("u_FadeStart",       m_Settings.SSRFadeStart);
    m_SSRPipeline->SetFloat("u_FacingFade",      m_Settings.SSRFacingFade);
    m_SSRPipeline->SetInt  ("u_HiZMipCount",     m_HiZMipCount);
    m_SSRPipeline->SetInt  ("u_FrameIndex",      m_FrameIndex);

    // 输出 image2D
    m_SSRPipeline->BindImageTexture(0, m_SSROutputTex, 0, GL_WRITE_ONLY, GL_RGBA16F);

    m_SSRPipeline->Dispatch((w + 7) / 8, (h + 7) / 8, 1);
    ComputePipeline::Wait(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                          GL_TEXTURE_FETCH_BARRIER_BIT);

    glBindTexture(GL_TEXTURE_2D, 0);
    m_SSRPipeline->Unbind();
}

void SceneRenderer::EnsureBloomTextures(uint32_t width, uint32_t height) {
    if (m_BloomPrefilterFBO &&
        !m_BloomMips.empty() &&
        m_BloomPrefilterFBO->GetWidth()  == width &&
        m_BloomPrefilterFBO->GetHeight() == height)
        return;

    auto makeBloomFBO = [](uint32_t w, uint32_t h) -> Ref<Framebuffer> {
        FramebufferSpec spec;
        spec.Width                 = std::max(w, 1u);
        spec.Height                = std::max(h, 1u);
        spec.ColorFormat           = GL_RGBA16F;
        spec.HasColorAttachment    = true;
        spec.HasDepthAttachment    = false;
        spec.HasEntityIDAttachment = false;
        return Framebuffer::Create(spec);
    };

    // Prefilter FBO（W/2 × H/2）
    m_BloomPrefilterFBO = makeBloomFBO(width, height);

    // Downsample mip 链（从 W/4 × H/4 开始，每级减半）
    m_BloomMips.clear();
    uint32_t w = width  / 2;
    uint32_t h = height / 2;
    for (int i = 0; i < MAX_BLOOM_MIPS && w >= 2 && h >= 2; ++i) {
        m_BloomMips.push_back(makeBloomFBO(w, h));
        w /= 2;
        h /= 2;
    }
}

void SceneRenderer::BloomPrefilterPass() {
    auto shader = m_ShaderLibrary.Get("bloom_prefilter");
    if (!shader || !m_BloomPrefilterFBO || !m_HDRFramebuffer) return;

    m_BloomPrefilterFBO->Bind();
    RenderCommand::SetViewport(0, 0,
        m_BloomPrefilterFBO->GetWidth(),
        m_BloomPrefilterFBO->GetHeight());

    shader->Bind();
    shader->SetInt  ("u_Texture",   0);
    shader->SetFloat("u_Threshold", m_Settings.BloomThreshold);
    shader->SetFloat("u_Knee",      m_Settings.BloomKnee);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_HDRFramebuffer->GetColorAttachment());

    Renderer::DrawFullscreenQuad();

    shader->Unbind();
    m_BloomPrefilterFBO->Unbind();
}

void SceneRenderer::BloomDownsamplePass() {
    auto shader = m_ShaderLibrary.Get("bloom_downsample");
    if (!shader || m_BloomMips.empty()) return;

    shader->Bind();
    shader->SetInt("u_Texture", 0);
    glActiveTexture(GL_TEXTURE0);

    GLuint srcTex = m_BloomPrefilterFBO->GetColorAttachment();
    for (auto& mipFBO : m_BloomMips) {
        mipFBO->Bind();
        RenderCommand::SetViewport(0, 0, mipFBO->GetWidth(), mipFBO->GetHeight());
        glBindTexture(GL_TEXTURE_2D, srcTex);
        Renderer::DrawFullscreenQuad();
        srcTex = mipFBO->GetColorAttachment();
        mipFBO->Unbind();
    }

    shader->Unbind();
}

void SceneRenderer::BloomUpsamplePass() {
    auto shader = m_ShaderLibrary.Get("bloom_upsample");
    if (!shader || m_BloomMips.empty()) return;

    // 加法混合：将上采样结果叠加到目标 mip 已有的下采样内容上
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    shader->Bind();
    shader->SetInt  ("u_Texture",      0);
    shader->SetFloat("u_FilterRadius", 1.0f);
    glActiveTexture(GL_TEXTURE0);

    // 从最小 mip 反向上采样叠加到较大 mip
    // mip[N-1] → 叠加到 mip[N-2]
    // mip[N-2] → 叠加到 mip[N-3]
    // ...
    // mip[0]   → 叠加到 prefilter
    int n = static_cast<int>(m_BloomMips.size());
    for (int i = n - 1; i >= 0; --i) {
        // 源：较小的 mip
        GLuint srcTex = m_BloomMips[i]->GetColorAttachment();
        // 目标：较大的 mip 或 prefilter
        Ref<Framebuffer>& dstFBO = (i == 0) ? m_BloomPrefilterFBO : m_BloomMips[i - 1];

        dstFBO->Bind();
        RenderCommand::SetViewport(0, 0, dstFBO->GetWidth(), dstFBO->GetHeight());
        glBindTexture(GL_TEXTURE_2D, srcTex);
        Renderer::DrawFullscreenQuad();
        dstFBO->Unbind();
    }

    shader->Unbind();
    glDisable(GL_BLEND);
}

void SceneRenderer::CopyDepthToTarget(Framebuffer& target) {
    if (!m_HDRFramebuffer) return;

    uint32_t w = target.GetWidth();
    uint32_t h = target.GetHeight();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_HDRFramebuffer->GetID());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target.GetID());
    glBlitFramebuffer(0, 0, m_HDRFramebuffer->GetWidth(), m_HDRFramebuffer->GetHeight(),
                      0, 0, w, h,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneRenderer::CompositePass() {
    auto compositeShader = m_ShaderLibrary.Get("scene_composite");
    if (!compositeShader) {
        std::cerr << "[SceneRenderer] scene_composite shader not found!" << std::endl;
        return;
    }

    // 写到外部 TargetFramebuffer（GL_RGBA8，ImGui 显示用）
    m_TargetFramebuffer->Bind();
    RenderCommand::SetViewport(0, 0, m_TargetFramebuffer->GetWidth(),
                                      m_TargetFramebuffer->GetHeight());

    compositeShader->Bind();
    compositeShader->SetInt  ("u_Texture",    0);
    compositeShader->SetInt  ("u_EntityIDTex", 1);
    compositeShader->SetFloat("u_Exposure",   m_Settings.Exposure);

    // Bloom：intensity=0 时 shader 直接 × 0，无需额外 bool
    bool bloomReady = m_Settings.EnableBloom && m_BloomPrefilterFBO;
    compositeShader->SetInt  ("u_BloomTex",       2);
    compositeShader->SetFloat("u_BloomIntensity", bloomReady ? m_Settings.BloomIntensity : 0.0f);

    // 描边
    compositeShader->SetInt  ("u_SelectedEntityID",
        m_Settings.EnableOutline ? m_SelectedEntityID : -1);
    compositeShader->SetVec4 ("u_OutlineColor",  m_Settings.OutlineColor);
    compositeShader->SetFloat("u_OutlineWidth",
        static_cast<float>(m_Settings.OutlineWidth));

    // SSR：intensity=0 时 shader 直接 × 0，无需额外 bool
    bool ssrReady = m_Settings.EnableSSR && m_SSROutputTex;
    compositeShader->SetInt  ("u_SSRTex",       3);
    compositeShader->SetFloat("u_SSRIntensity", ssrReady ? m_Settings.SSRIntensity : 0.0f);

    // Deferred 路径的 EntityID 来自 GBuffer，Forward 路径来自 HDR FBO
    GLuint entityIDTex = (m_Settings.UseDeferredShading && m_GBuffer)
        ? m_GBuffer->GetEntityID()
        : m_HDRFramebuffer->GetEntityIDAttachment();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_HDRFramebuffer->GetColorAttachment());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, entityIDTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, bloomReady
        ? m_BloomPrefilterFBO->GetColorAttachment()
        : Renderer::GetBlackTexture()->GetID());
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, ssrReady
        ? m_SSROutputTex
        : Renderer::GetBlackTexture()->GetID());

    Renderer::DrawFullscreenQuad();

    compositeShader->Unbind();
    m_TargetFramebuffer->Unbind();
}

void SceneRenderer::GenerateSSAOKernel() {
    // 固定种子保证每次启动采样核一致
    std::default_random_engine rng(42u);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    m_SSAOKernel.resize(64);
    for (int i = 0; i < 64; ++i) {
        // 切线空间半球样本：xy ∈ [-1,1]，z ∈ [0,1]（朝向法线方向）
        glm::vec3 sample(
            dist(rng) * 2.0f - 1.0f,
            dist(rng) * 2.0f - 1.0f,
            dist(rng)
        );
        sample = glm::normalize(sample) * dist(rng);

        // 优化 [3]：幂次分布 r = mix(0.1, 1.0, (i/N)²)
        // 使更多样本集中在近处，近处几何体对 AO 贡献最大
        float scale = float(i) / 64.0f;
        scale = glm::mix(0.1f, 1.0f, scale * scale);
        m_SSAOKernel[i] = sample * scale;
    }
}

void SceneRenderer::EnsureSSAOResources(uint32_t width, uint32_t height) {
    if (m_GNormalFBO &&
        m_GNormalFBO->GetWidth()  == width &&
        m_GNormalFBO->GetHeight() == height)
        return;

    // 法线 G-Buffer：RGBA16F 颜色 + 可采样深度纹理
    // SSAOPass 需要同时读取法线颜色和深度，因此深度必须是纹理而非 Renderbuffer
    {
        FramebufferSpec spec;
        spec.Width                 = width;
        spec.Height                = height;
        spec.ColorFormat           = GL_RGBA16F;
        spec.HasColorAttachment    = true;
        spec.HasDepthAttachment    = true;
        spec.DepthAsTexture        = true;
        spec.HasEntityIDAttachment = false;
        m_GNormalFBO = Framebuffer::Create(spec);
    }

    // 原始 AO：R16F 单通道
    {
        FramebufferSpec spec;
        spec.Width                 = width;
        spec.Height                = height;
        spec.ColorFormat           = GL_R16F;
        spec.HasColorAttachment    = true;
        spec.HasDepthAttachment    = false;
        spec.HasEntityIDAttachment = false;
        m_SSAOFbo = Framebuffer::Create(spec);
    }

    // 模糊后 AO：同 R16F
    {
        FramebufferSpec spec;
        spec.Width                 = width;
        spec.Height                = height;
        spec.ColorFormat           = GL_R16F;
        spec.HasColorAttachment    = true;
        spec.HasDepthAttachment    = false;
        spec.HasEntityIDAttachment = false;
        m_SSAOBlurFBO = Framebuffer::Create(spec);
    }

    std::cout << "[SceneRenderer] SSAO resources created: " << width << "x" << height << std::endl;
}

void SceneRenderer::NormalPrePass() {
    if (m_OpaqueDrawList.empty() || !m_GNormalFBO) return;

    auto shader = m_ShaderLibrary.Get("ssao_prepass");
    if (!shader) return;

    m_GNormalFBO->Bind();
    RenderCommand::SetViewport(0, 0, m_GNormalFBO->GetWidth(), m_GNormalFBO->GetHeight());
    RenderCommand::SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    RenderCommand::Clear();
    RenderCommand::EnableDepthTest();
    RenderCommand::SetDepthMask(true);
    glDisable(GL_CULL_FACE);

    shader->Bind();

    // 遍历所有不透明物体写入视图空间法线和深度
    // 不做视锥裁剪：法线预处理的 overhead 很低，且 SSAO 需要屏幕边缘的遮挡信息
    for (const auto& cmd : m_OpaqueDrawList) {
        if (!cmd.MeshSource) continue;
        auto vao = cmd.MeshSource->GetVertexArray();
        if (!vao) continue;

        shader->SetMat4("u_Model",        cmd.Transform);
        shader->SetMat3("u_NormalMatrix", cmd.NormalMatrix);

        const auto& submeshes = cmd.MeshSource->GetSubmeshes();
        if (cmd.SubmeshIndex >= submeshes.size()) continue;
        const auto& submesh = submeshes[cmd.SubmeshIndex];

        vao->Bind();
        glDrawElementsBaseVertex(GL_TRIANGLES, submesh.IndexCount,
            GL_UNSIGNED_INT,
            (void*)(submesh.BaseIndex * sizeof(uint32_t)),
            submesh.BaseVertex);
        vao->Unbind();
    }

    shader->Unbind();
    m_GNormalFBO->Unbind();
}

void SceneRenderer::SSAOPass() {
    bool deferred = m_Settings.UseDeferredShading && m_GBuffer;
    if (!m_SSAOFbo || (!deferred && !m_GNormalFBO)) return;

    auto shader = m_ShaderLibrary.Get("ssao");
    if (!shader) return;

    m_SSAOFbo->Bind();
    RenderCommand::SetViewport(0, 0, m_SSAOFbo->GetWidth(), m_SSAOFbo->GetHeight());
    glDisable(GL_DEPTH_TEST);

    shader->Bind();

    // 上传采样核（切线空间，64 个，C++ 已做幂次分布）
    int kernelSize = std::min(m_Settings.SSAOKernelSize, static_cast<int>(m_SSAOKernel.size()));
    for (int i = 0; i < kernelSize; ++i)
        shader->SetVec3("u_Samples[" + std::to_string(i) + "]", m_SSAOKernel[i]);
    shader->SetInt  ("u_KernelSize", kernelSize);
    shader->SetFloat("u_Radius",     m_Settings.SSAORadius);
    shader->SetFloat("u_Bias",       m_Settings.SSAOBias);
    shader->SetBool ("u_UseGBufferNormals", deferred);

    shader->SetInt("u_GNormal", 0);
    shader->SetInt("u_GDepth",  1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, deferred
        ? m_GBuffer->GetNormalRoughMetal()
        : m_GNormalFBO->GetColorAttachment());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, deferred
        ? m_GBuffer->GetDepth()
        : m_GNormalFBO->GetDepthAttachment());

    Renderer::DrawFullscreenQuad();

    shader->Unbind();
    m_SSAOFbo->Unbind();
    glEnable(GL_DEPTH_TEST);
}

void SceneRenderer::SSAOBlurPass() {
    bool deferred = m_Settings.UseDeferredShading && m_GBuffer;
    if (!m_SSAOBlurFBO || !m_SSAOFbo || (!deferred && !m_GNormalFBO)) return;

    auto shader = m_ShaderLibrary.Get("ssao_blur");
    if (!shader) return;

    m_SSAOBlurFBO->Bind();
    RenderCommand::SetViewport(0, 0, m_SSAOBlurFBO->GetWidth(), m_SSAOBlurFBO->GetHeight());
    glDisable(GL_DEPTH_TEST);

    shader->Bind();
    shader->SetInt  ("u_AOInput",       0);
    shader->SetInt  ("u_Depth",         1);
    shader->SetFloat("u_BlurSharpness", m_Settings.SSAOBlurSharpness);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_SSAOFbo->GetColorAttachment());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, deferred
        ? m_GBuffer->GetDepth()
        : m_GNormalFBO->GetDepthAttachment());

    Renderer::DrawFullscreenQuad();

    shader->Unbind();
    m_SSAOBlurFBO->Unbind();
    glEnable(GL_DEPTH_TEST);
}

} // namespace GLRenderer
