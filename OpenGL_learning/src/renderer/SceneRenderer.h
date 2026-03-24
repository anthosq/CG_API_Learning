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
#include "graphics/ComputePipeline.h"
#include "graphics/StorageBuffer.h"
#include "core/Frustum.h"
#include "scene/BVH.h"

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

    // SSAO
    bool  EnableSSAO         = true;
    float SSAORadius         = 0.5f;   // 采样半径（视图空间，单位：米）
    float SSAOBias           = 0.025f; // self-occlusion 偏移
    int   SSAOKernelSize     = 64;     // 采样核数量（≤ 64）
    float SSAOBlurSharpness  = 1.0f;   // 双边模糊的深度敏感度

    // 阴影设置
    uint32_t ShadowCascadeCount  = 4;
    uint32_t ShadowResolution    = 2048;
    float    ShadowDistance      = 200.0f;
    float    ShadowFade          = 10.0f;
    float    CascadeSplitLambda  = 0.75f;   // 0=均匀, 1=纯对数
    float    CascadeTransitionFade = 5.0f;  // cascade 间过渡区宽度（世界单位）
    bool     SoftShadows         = true;    // Poisson PCF vs 硬阴影
    bool     ShowCascades        = false;   // debug 色调可视化
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
                     float aspectRatio);

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
    void TiledLightCullPass();     // Compute Shader：per-tile 光源剔除
    void GeometryPass();           // 不透明物体（从 tile 光源列表取光源）
    void TransparentPass();        // 透明物体

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
    void UpdateShadowUBO();
    void CalculateCascades(CascadeData* outCascades);

    void EnsureHDRFramebuffer(uint32_t width, uint32_t height);
    void EnsureBloomTextures(uint32_t width, uint32_t height);

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

    // === 阴影 ===
    Ref<ShadowMap>             m_ShadowMap;
    std::unique_ptr<Pipeline>  m_ShadowPipeline;
    CascadeData                m_Cascades[4];   // 当前帧计算结果，最多 4 个 cascade
    Frustum                    m_CameraFrustum;

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
    Ref<Framebuffer>           m_GNormalFBO;     // 法线预处理 FBO（RGBA16F normal + depth tex）
    Ref<Framebuffer>           m_SSAOFbo;        // 原始 AO（R16F）
    Ref<Framebuffer>           m_SSAOBlurFBO;    // 模糊后 AO（R16F）
    std::vector<glm::vec3>     m_SSAOKernel;     // 半球采样核（切线空间）
};

} // namespace GLRenderer
