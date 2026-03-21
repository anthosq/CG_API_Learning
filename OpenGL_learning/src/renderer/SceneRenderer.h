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

#include <memory>
#include <vector>

namespace GLRenderer {

// 场景渲染设置
struct SceneRenderSettings {
    bool ShowGrid = true;
    bool ShowSkybox = true;
    bool EnableShadows = false;
    bool Wireframe = false;

    // 网格设置
    float GridSize = 100.0f;
    float GridCellSize = 1.0f;

    // 环境设置
    glm::vec3 AmbientColor = glm::vec3(0.1f);
};

// 场景环境数据
struct SceneEnvironment {
    DirectionalLight DirLight;

    PointLight PointLights[MAX_POINT_LIGHTS];
    int PointLightCount = 0;

    SpotLight Spotlight;
    bool SpotlightEnabled = false;

    Ref<TextureCube> Skybox;
    float EnvironmentIntensity = 1.0f;
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
                    const Ref<MaterialTable>& materials = nullptr,
                    int entityID = -1);

    // 提交 StaticMesh (会遍历所有 submesh)
    void SubmitStaticMesh(const Ref<StaticMesh>& staticMesh,
                          Ref<MeshSource> meshSource,
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

    // 着色器库访问
    ShaderLibrary& GetShaderLibrary() { return m_ShaderLibrary; }

    // 加载着色器
    void LoadShaders(const std::filesystem::path& shaderDir);

    // 设置默认纹理
    void SetDefaultDiffuse(Ref<Texture2D> texture) { m_DefaultDiffuse = texture; }
    void SetDefaultSpecular(Ref<Texture2D> texture) { m_DefaultSpecular = texture; }

    // 设置目标 Framebuffer
    void SetTargetFramebuffer(Framebuffer* fbo) { m_TargetFramebuffer = fbo; }

private:
    void FlushDrawList();

    void GridPass();
    void SkyboxPass();
    void GeometryPass();         // 不透明物体
    void TransparentPass();      // 透明物体

    void SyncLightsFromECS(ECS::World& world);
    void CollectDrawCommandsFromECS(ECS::World& world);
    void SortTransparentDrawList();
    void CreateDefaultResources();
    void UpdateLightingUBO();

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

    // === Pipeline ===
    std::unique_ptr<Pipeline> m_OpaquePipeline;
    std::unique_ptr<Pipeline> m_TransparentPipeline;

    // === 当前帧数据 ===
    Framebuffer* m_TargetFramebuffer = nullptr;
    glm::mat4 m_ViewMatrix;
    glm::mat4 m_ProjectionMatrix;
    glm::vec3 m_CameraPosition;

    // === Uniform Buffer Objects ===
    std::unique_ptr<UniformBuffer> m_CameraUBO;
    std::unique_ptr<UniformBuffer> m_LightingUBO;
};

} // namespace GLRenderer
