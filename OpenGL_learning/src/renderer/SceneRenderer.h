#pragma once

// SceneRenderer - 场景渲染器
//
// 架构设计 (参考 Hazel Engine):
// 1. Submit 阶段: BeginScene() -> SubmitMesh() -> EndScene()
// 2. Flush 阶段: FlushDrawList() 执行各个渲染 Pass
//
// 所有网格统一通过 MeshSource 渲染

#include "Renderer.h"
#include "RenderTypes.h"
#include "graphics/Shader.h"
#include "graphics/Framebuffer.h"
#include "graphics/Camera.h"
#include "graphics/Texture.h"
#include "graphics/Buffer.h"
#include "graphics/UniformBuffer.h"
#include "graphics/MeshSource.h"
#include "graphics/MeshFactory.h"
#include "scene/ecs/ECS.h"
#include "scene/Light.h"
#include "scene/Grid.h"
#include "asset/MaterialAsset.h"
#include "renderer/Environment.h"
#include "graphics/IBLProcessor.h"
#include "graphics/ShadowMap.h"
#include "graphics/PointShadowMapArray.h"
#include "graphics/GBuffer.h"
#include "graphics/ComputePipeline.h"
#include "graphics/StorageBuffer.h"
#include "core/Frustum.h"
#include "scene/BVH.h"
#include "renderer/ParticleSystem.h"
#include "physics/FluidSimulation.h"

#include <memory>
#include <vector>

namespace GLRenderer {

// 场景渲染设置
struct SceneRenderSettings {
    bool ShowGrid    = true;
    bool ShowSkybox  = true;
    bool Wireframe   = false;

    // 网格设置
    float GridSize     = 100.0f;
    float GridCellSize = 1.0f;

    // 环境设置
    glm::vec3 AmbientColor = glm::vec3(0.1f);

    // 天空盒
    float SkyboxLOD = 0.0f;

    // Pre-Depth Pass：在 GeometryPass 前写满深度，消除 PBR overdraw
    bool EnableDepthPrepass  = true;

    // Tiled Forward+ 光源剔除
    bool EnableTiledLighting = true;

    // 后处理
    float Exposure = 1.0f;  // 曝光值，传给 CompositePass

    // Bloom
    bool  EnableBloom     = true;
    float BloomThreshold  = 1.0f;   // 亮度阈值
    float BloomKnee       = 0.1f;   // 软过渡宽度（相对于 threshold 的比例）
    float BloomIntensity  = 0.8f;   // 最终叠加强度（CompositePass 使用）

    // 描边
    bool      EnableOutline = true;
    glm::vec4 OutlineColor  = glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);
    int       OutlineWidth  = 2;

    // 屏幕空间流体渲染（Phase 12）
    bool  EnableFluidRendering      = true;
    bool  ShowFluidParticles        = false;   // 粒子调试视图：按速度大小着色的点精灵
    int   FluidBlurRadius           = 15;      // NRF 1D 核半径（像素）
    float FluidBlurSigmaS           = 8.0f;    // 空间高斯 σ（像素）
    float FluidNRFThreshold         = 0.02f;   // NRF 深度阈值（NDC，区分前后表面，典型 0.01~0.05）
    int   FluidNRFCleanupRadius     = 3;       // 2D Cleanup pass 核半径（像素，典型 2~5）
    float FluidRefractStrength      = 0.025f;  // 折射 UV 偏移强度（参考：0.025）
    float FluidRenderRadiusScale    = 2.0f;    // 渲染粒子半径缩放（参考：2.0，使点精灵覆盖粒子间隙）
    // ThicknessScale 说明：
    //   参考实现片元输出 dz * 0.05（无量纲）；我们输出 dz * 2r = dz * 0.02m（米制）。
    //   等效缩放：0.05 / 0.02 = 2.5，使有效厚度与参考一致。
    float FluidThicknessScale       = 2.5f;   // 厚度缩放（参考等效 2.5：0.05/0.02m）
    float FluidMinThickness         = 0.04f;  // 厚度下限（原始单位，低于此值的外围像素不渲染，消除透明光晕）
    // 消光系数：按物理水体校准（厚 0.2m 时红光吸收 ~70%，蓝光几乎不变）
    // 与参考等效：extinction_ours = extinction_ref × (ref_thickness / our_thickness) = 0.741 × 2.5
    glm::vec3 FluidWaterColor  = {0.259f, 0.518f, 0.957f};  // 仅用于 UI 显示参考色，shader 已不使用
    glm::vec3 FluidExtinction  = {0.741f, 0.482f, 0.043f};  // 与参考一致（ThicknessScale 已补偿单位）

    // SSR 屏幕空间反射（仅 Deferred 路径）
    bool  EnableSSR          = false;
    bool  SSRHalfResolution  = true;    // 半分辨率：~4x 性能提升，粗糙面几乎无质量损失
    int   SSRMaxSteps        = 64;
    float SSRDepthTolerance  = 0.15f;   // 命中深度容差（线性，单位米）
    float SSRFadeStart       = 0.8f;    // 边缘淡出起始比例
    float SSRFacingFade      = 0.2f;    // 掠射角容差
    float SSRIntensity       = 1.0f;    // 整体强度（0=关闭，不需要单独 bool）

    // SSAO
    bool  EnableSSAO         = true;
    float SSAORadius         = 0.5f;   // 采样半径（视图空间，单位：米）
    float SSAOBias           = 0.025f; // self-occlusion 偏移
    int   SSAOKernelSize     = 64;     // 采样核数量（≤ 64）
    float SSAOBlurSharpness  = 1.0f;   // 双边模糊的深度敏感度

    // 方向光阴影设置
    uint32_t ShadowCascadeCount  = 4;
    uint32_t ShadowResolution    = 2048;
    float    ShadowDistance      = 200.0f;
    float    ShadowFade          = 10.0f;
    float    CascadeSplitLambda  = 0.75f;   // 0=均匀, 1=纯对数
    float    CascadeTransitionFade = 5.0f;  // cascade 间过渡区宽度（世界单位）
    bool     SoftShadows         = true;    // Poisson PCF vs 硬阴影
    bool     ShowCascades        = false;   // debug 色调可视化

    // 点光源阴影设置（Omnidirectional Cubemap Shadow Array）
    bool     EnablePointShadows    = true;
    uint32_t PointShadowResolution = 512;
    float    PointShadowFarPlane   = 50.0f;

    // 渲染路径
    // false = Tiled Forward+（默认）
    // true  = Tiled Deferred（GBufferPass + DeferredLightingPass）
    bool UseDeferredShading = false;
};

// 场景环境数据
struct SceneEnvironment {
    DirectionalLight DirLight;

    PointLight PointLights[MAX_POINT_LIGHTS];
    int PointLightCount = 0;

    SpotLight Spotlight;
    bool SpotlightEnabled = false;

    Ref<TextureCube> Skybox;          // 旧接口兼容, 优先使用 IBLEnvironment
    Ref<Environment> IBLEnvironment;  // IBL 数据 (RadianceMap + IrradianceMap)
    float EnvironmentIntensity = 1.0f;
};

// 视锥裁剪统计（每帧更新）
struct CullingStats {
    int TotalObjects   = 0;  // 提交的不透明 DrawCommand 总数
    int VisibleObjects = 0;  // BVH 查询后通过视锥的数量
    int CulledObjects  = 0;  // 被裁剪掉的数量
    int BVHNodeCount   = 0;  // 当前 BVH 节点总数
};

// 光源实体信息（用于可视化和拾取）
struct LightEntityInfo {
    glm::vec3 Position;
    glm::vec3 Color;
    int EntityID = -1;
    int LightType = 0;  // 0: Point, 1: Directional, 2: Spot
};

class SceneRenderer {
public:
    SceneRenderer();
    ~SceneRenderer();

    // 禁止拷贝
    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    // 初始化/关闭
    void Initialize();
    void Shutdown();

    // === Submit API ===
    void BeginScene(const Camera& camera, float aspectRatio);
    void EndScene();

    // 统一提交接口 - 所有网格通过 MeshSource 渲染
    void SubmitMesh(Ref<MeshSource> meshSource,
                    uint32_t submeshIndex,
                    const glm::mat4& transform,
                    const Ref<MaterialTable>& materialTable = nullptr,
                    int entityID = -1);

    // 提交 StaticMesh (会遍历所有 submesh)
    // materialTable: 组件级材质覆盖，为空时使用 StaticMesh 默认材质
    void SubmitStaticMesh(const Ref<StaticMesh>& staticMesh,
                          Ref<MeshSource> meshSource,
                          const Ref<MaterialTable>& materialTable,
                          const glm::mat4& transform,
                          int entityID = -1);

    // === 旧 API: 兼容现有代码 ===
    void RenderScene(ECS::World& world,
                     Framebuffer& targetFBO,
                     const Camera& camera,
                     float aspectRatio,
                     float deltaTime = 0.0f);

    // 设置
    SceneRenderSettings& GetSettings() { return m_Settings; }
    const SceneRenderSettings& GetSettings() const { return m_Settings; }

    SceneEnvironment& GetEnvironment() { return m_Environment; }
    const SceneEnvironment& GetEnvironment() const { return m_Environment; }

    // 裁剪统计（上一帧结果）
    const CullingStats& GetCullingStats() const { return m_CullingStats; }

    // SSAO 调试：返回模糊后 AO 纹理的 GL handle（0 = 尚未创建）
    uint32_t GetSSAODebugTexture() const {
        return m_SSAOBlurFBO ? m_SSAOBlurFBO->GetColorAttachment() : 0;
    }

    // 着色器库访问
    ShaderLibrary& GetShaderLibrary() { return m_ShaderLibrary; }

    // 加载着色器
    void LoadShaders(const std::filesystem::path& shaderDir);

    // 设置默认纹理
    void SetDefaultDiffuse(Ref<Texture2D> texture) { m_DefaultDiffuse = texture; }
    void SetDefaultSpecular(Ref<Texture2D> texture) { m_DefaultSpecular = texture; }

    // 设置目标 Framebuffer（接收最终 tonemapped LDR 输出）
    void SetTargetFramebuffer(Framebuffer* fbo) { m_TargetFramebuffer = fbo; }

    // 选中实体（用于描边）
    void SetSelectedEntityID(int id) { m_SelectedEntityID = id; }

    // 环境贴图 - 从 HDR 文件加载并执行 IBL 预处理
    bool LoadEnvironment(const std::filesystem::path& hdrPath);

    // 将 HDR FBO 的深度缓冲 blit 到目标 FBO（供 editor overlay 使用，使 billboard 受场景深度遮挡）
    void CopyDepthToTarget(Framebuffer& target);

private:
    void FlushDrawList();

    void GridPass();
    void SkyboxPass();
    void DepthPrePass();           // 仅写深度（消除 GeometryPass overdraw）
    void HiZBuildPass();           // 从场景深度构建 Hi-Z MIN mip 链（SSR 加速结构）
    void BackFaceDepthPass();      // 正面剔除渲染背面深度，供 SSR 厚度测试使用
    void SSRPass();                // 屏幕空间反射（线性步进，仅 Deferred 路径）
    void TiledLightCullPass();     // Compute Shader：per-tile 光源剔除
    void GeometryPass();               // 不透明物体（Tiled Forward+ 路径）
    void WireframeOverlayPass();       // 线框叠加（Deferred 路径，深度缓冲已有值）
    void GBufferPass();            // 不透明物体写入 G-Buffer（Deferred 路径）
    void DeferredLightingPass();   // 全屏 quad 读 G-Buffer 做 PBR 光照（Deferred 路径）
    void TransparentPass();        // 透明物体（两种路径均为 Forward+）
    void ParticlePass();           // GPU 粒子：Compute Simulate + Billboard Render

    // SSAO
    void NormalPrePass();        // 渲染视图空间法线到 m_GNormalFBO
    void SSAOPass();             // 计算原始 AO → m_SSAOFbo
    void SSAOBlurPass();         // 深度感知模糊 → m_SSAOBlurFBO
    void EnsureSSAOResources(uint32_t width, uint32_t height);
    void GenerateSSAOKernel();   // 在 Initialize() 中调用一次

    void BloomPrefilterPass();   // Bloom 第一步：提取亮部
    void BloomDownsamplePass();  // Bloom 第二步：逐级下采样
    void BloomUpsamplePass();    // Bloom 第三步：逐级上采样并叠加
    void CompositePass();

    void SyncLightsFromECS(ECS::World& world);
    void CollectDrawCommandsFromECS(ECS::World& world);
    void SortTransparentDrawList();
    void CreateDefaultResources();
    void UpdateLightingUBO();
    void UploadPointLightSSBO();                              // 上传点光源数据到 SSBO
    void EnsureTiledLightBuffers(int numTilesX, int numTilesY); // 按需扩容 SSBO

    // 阴影
    void ShadowPass();
    void PointShadowPass();       // 点光源 Omnidirectional Shadow Pass
    void UpdateShadowUBO();
    void CalculateCascades(CascadeData* outCascades);

    void EnsureHDRFramebuffer(uint32_t width, uint32_t height);
    void EnsureBloomTextures(uint32_t width, uint32_t height);
    void EnsureSSRResources(uint32_t width, uint32_t height);
    void EnsureBackFaceDepthTex(int w, int h);

    // PBR 渲染
    void BindPBRMaterial(Shader& shader, const Ref<MaterialAsset>& material);
    Ref<MaterialAsset> GetMaterialForDrawCommand(const DrawCommand& cmd);

private:
    bool m_Initialized = false;

    // 设置
    SceneRenderSettings m_Settings;
    SceneEnvironment m_Environment;

    // 着色器库
    ShaderLibrary m_ShaderLibrary;

    // 默认资源
    std::unique_ptr<Grid> m_Grid;
    Ref<MeshSource> m_CubeMesh;  // 用于天空盒等

    Ref<Texture2D> m_DefaultDiffuse;
    Ref<Texture2D> m_DefaultSpecular;
    Ref<MaterialAsset> m_DefaultMaterial;

    // === 绘制列表 ===
    std::vector<DrawCommand> m_OpaqueDrawList;
    std::vector<DrawCommand> m_TransparentDrawList;
    std::vector<LightEntityInfo> m_LightEntities;

    // 粒子绘制列表（每帧从 ECS 收集）
    struct ParticleDrawEntry {
        Ref<ParticleSystem> System;
        EmitParams Params;
    };
    std::vector<ParticleDrawEntry> m_ParticleDrawList;
    float m_DeltaTime = 0.0f;  // RenderScene 时更新，ParticlePass 使用

    // 粒子 Compute Pipeline（与 TiledLightCull / HiZ / SSR 同样用 ComputePipeline 加载）
    Ref<ComputePipeline> m_ParticleEmitPipeline;
    Ref<ComputePipeline> m_ParticleUpdatePipeline;
    Ref<ComputePipeline> m_ParticleCompactPipeline;

    // 与 m_OpaqueDrawList 一一对应的世界空间 AABB（用于 BVH 和视锥裁剪）
    std::vector<AABB> m_WorldAABBs;

    // === Pipeline ===
    std::unique_ptr<Pipeline> m_OpaquePipeline;
    std::unique_ptr<Pipeline> m_TransparentPipeline;

    // === 当前帧数据 ===
    Framebuffer* m_TargetFramebuffer = nullptr;
    glm::mat4 m_ViewMatrix;
    glm::mat4 m_ProjectionMatrix;
    glm::vec3 m_CameraPosition;
    float     m_CameraNear = 0.1f;
    float     m_CameraFar  = 1000.0f;

    // === Uniform Buffer Objects ===
    std::unique_ptr<UniformBuffer> m_CameraUBO;
    std::unique_ptr<UniformBuffer> m_LightingUBO;
    std::unique_ptr<UniformBuffer> m_ShadowUBO;

    // === Tiled Forward+ ===
    Ref<ComputePipeline>  m_TileLightCullPipeline;
    Ref<StorageBuffer>    m_PointLightSSBO;        // PointLightGPU[]
    Ref<StorageBuffer>    m_TileLightCountSSBO;    // int[numTiles]
    Ref<StorageBuffer>    m_TileLightIndexSSBO;    // int[numTiles * MAX_LIGHTS_PER_TILE]
    int                   m_TilesX = 0;
    int                   m_TilesY = 0;
    // 当前帧场景深度纹理，由 FlushDrawList 在 DepthPrePass 后赋值。
    // Deferred 迁移后改为指向 GBuffer 深度附件，TiledLightCullPass 无需感知来源。
    GLuint                m_SceneDepthTexture = 0;

    // === 阴影 ===
    Ref<ShadowMap>             m_ShadowMap;
    std::unique_ptr<Pipeline>  m_ShadowPipeline;
    CascadeData                m_Cascades[4];   // 当前帧计算结果，最多 4 个 cascade
    Frustum                    m_CameraFrustum;

    // 点光源阴影
    Ref<PointShadowMapArray>   m_PointShadowArray;
    // m_PointShadowAssignments[slot] = 环境中 PointLights[] 的索引（-1 = 该 slot 未使用）
    int                        m_PointShadowAssignments[MAX_SHADOW_POINT_LIGHTS] = {};

    BVH                        m_SceneBVH;

    // === 后处理 ===
    // 所有几何/天空盒/网格 Pass 均渲染到此 HDR FBO（GL_RGBA16F + EntityID + Depth）
    // CompositePass 从此处读取并写到 m_TargetFramebuffer（GL_RGBA8，ImGui 显示）
    Ref<Framebuffer>           m_HDRFramebuffer;

    // Bloom FBO 链（GL_RGBA16F，无深度/EntityID）
    // [0] = Prefilter 输出（W/2 × H/2）
    // [1] = W/4 × H/4
    // [2] = W/8 × H/8  ...最多 MAX_BLOOM_MIPS 级
    // Upsample 反向叠加后，[0] 即为最终 Bloom 贴图
    static constexpr int MAX_BLOOM_MIPS = 8;
    Ref<Framebuffer>           m_BloomPrefilterFBO;
    std::vector<Ref<Framebuffer>> m_BloomMips;

    int                        m_SelectedEntityID = -1;

    CullingStats               m_CullingStats;

    // === SSAO ===
    Ref<Framebuffer>           m_GNormalFBO;     // 法线预处理 FBO（RGBA16F normal + depth tex，Forward+ 路径使用）
    Ref<Framebuffer>           m_SSAOFbo;        // 原始 AO（R16F）
    Ref<Framebuffer>           m_SSAOBlurFBO;    // 模糊后 AO（R16F）
    std::vector<glm::vec3>     m_SSAOKernel;     // 半球采样核（切线空间）

    // === SSR ===
    // Hi-Z 纹理：R32F，完整 mip 链，GL_NEAREST 过滤，MIN 下采样（见 hiz_build.glsl）
    // 使用原始 GLuint 而非 Texture2D，因为需要精确控制 mip 数量与 glTexStorage2D
    GLuint m_HiZTex       = 0;
    int    m_HiZMipCount  = 0;
    int    m_HiZTexWidth  = 0;
    int    m_HiZTexHeight = 0;

    // SSR 输出纹理：RGBA16F，无 mip，rgb=反射色，a=confidence
    GLuint m_SSROutputTex    = 0;
    int    m_SSROutputWidth  = 0;
    int    m_SSROutputHeight = 0;

    Ref<ComputePipeline> m_HiZPipeline;  // hiz_build.glsl
    Ref<ComputePipeline> m_SSRPipeline;  // ssr.glsl

    // 场景颜色金字塔（ConeTracing 用）：RGBA16F，全分辨率，完整 mip 链
    GLuint m_SSRColorPyramid       = 0;
    int    m_SSRColorPyramidWidth  = 0;
    int    m_SSRColorPyramidHeight = 0;
    int    m_SSRColorPyramidMips   = 0;

    // 背面深度贴图：GL_DEPTH_COMPONENT32F，全分辨率，正面剔除渲染
    GLuint m_BackFaceDepthTex = 0;
    GLuint m_BackFaceDepthFBO = 0;
    int    m_BackFaceDepthW   = 0;
    int    m_BackFaceDepthH   = 0;

    int m_FrameIndex = 0;  // 每帧递增，用于 SSR 时间抖动

    // === Deferred Shading ===
    Ref<GBuffer>               m_GBuffer;        // 几何缓冲区（Deferred 路径使用）

    // === 屏幕空间流体渲染（Phase 12）===
    struct FluidDrawEntry {
        GLuint positionSSBO  = 0;
        GLuint velocitySSBO  = 0;
        int    particleCount = 0;
        float  particleRadius = 0.05f;
        glm::vec3 boundaryMin = {-0.5f, 0.0f, -0.5f};
        glm::vec3 boundaryMax = { 0.5f, 2.0f,  0.5f};
    };
    std::vector<FluidDrawEntry> m_FluidDrawList;

    Ref<Framebuffer>  m_FluidDepthFBO;           // 球面深度（深度附件）+ 原始厚度（颜色附件 R32F）
    Ref<Framebuffer>  m_FluidBlurFBO[2];         // NRF ping-pong，最终平滑深度在 [0]
    Ref<Framebuffer>  m_FluidThicknessFBO;       // 经过空间高斯扩散的厚度图（R32F）
    Ref<Framebuffer>  m_FluidNormalFBO;          // 视空间法线
    Ref<Framebuffer>  m_FluidSceneColorFBO;      // 场景颜色快照（ShadePass 前 blit，打破 feedback loop）
    GLuint            m_FluidEmptyVAO = 0; // 无属性 VAO（point sprite 用）

    void FluidDepthPass();
    void FluidBlurPass();
    void FluidNormalPass();
    void FluidShadePass();
    void FluidParticleDebugPass();
    void EnsureFluidResources(uint32_t w, uint32_t h);
};

} // namespace GLRenderer
