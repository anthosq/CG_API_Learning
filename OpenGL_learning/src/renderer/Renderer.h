#pragma once

#include "RenderCommand.h"
#include "graphics/Camera.h"
#include "graphics/Shader.h"
#include "graphics/Framebuffer.h"
#include "graphics/Texture.h"
#include "graphics/Mesh.h"
#include "scene/Light.h"
#include "core/Ref.h"
#include <functional>
#include <vector>

namespace GLRenderer {

// RendererStats - 渲染统计信息
struct RendererStats {
    uint32_t DrawCalls = 0;
    uint32_t Triangles = 0;
    uint32_t Vertices = 0;

    void Reset() {
        DrawCalls = 0;
        Triangles = 0;
        Vertices = 0;
    }
};

// SceneData - 场景渲染数据
struct SceneData {
    glm::mat4 ViewMatrix;
    glm::mat4 ProjectionMatrix;
    glm::mat4 ViewProjectionMatrix;
    glm::vec3 CameraPosition;
    float NearPlane;
    float FarPlane;
};

// Renderer - 渲染器核心类
class Renderer {
public:
    // 生命周期

    static void Init();
    static void Shutdown();

    // 帧管理

    // 开始新的一帧渲染
    static void BeginFrame(const Camera& camera, float aspectRatio);
    static void EndFrame();

    // 场景数据访问

    static const SceneData& GetSceneData() { return s_SceneData; }
    static const glm::mat4& GetViewMatrix() { return s_SceneData.ViewMatrix; }
    static const glm::mat4& GetProjectionMatrix() { return s_SceneData.ProjectionMatrix; }
    static const glm::mat4& GetViewProjectionMatrix() { return s_SceneData.ViewProjectionMatrix; }

    // 渲染辅助方法

    // 设置着色器的通用 uniform（view, projection, viewPos）
    static void SetupShaderMatrices(Shader& shader);
    static void SetupShaderMatrices(Shader& shader, const glm::mat4& model);

    // 设置光照 uniform
    static void SetupDirectionalLight(Shader& shader, const DirectionalLight& light);
    static void SetupPointLights(Shader& shader, const PointLight* lights, int count);
    static void SetupSpotLight(Shader& shader, const SpotLight& light);
    static void DisableSpotLight(Shader& shader);  // 禁用聚光灯（设置零贡献）

    // 绘制方法

    // 绘制全屏四边形（用于后处理）
    static void DrawFullscreenQuad();

    // 提交网格绘制（支持实体 ID 用于鼠标拾取）
    static void SubmitMesh(Mesh& mesh, Shader& shader, const glm::mat4& transform, int entityID = -1);
    static void SubmitModel(Model& model, Shader& shader, const glm::mat4& transform, int entityID = -1);

    // 默认纹理访问
    static Ref<Texture2D> GetWhiteTexture();
    static Ref<Texture2D> GetBlackTexture();
    static Ref<Texture2D> GetNormalTexture();
    static Ref<Texture2D> GetBRDFLutTexture();
    static Ref<TextureCube> GetBlackCubeTexture();

    // 统计信息
    static const RendererStats& GetStats() { return s_Stats; }
    static void ResetStats() { s_Stats.Reset(); }

    // 记录绘制调用
    static void RecordDrawCall(uint32_t vertexCount, uint32_t indexCount = 0);

private:
    static void CreateFullscreenQuad();

    static SceneData s_SceneData;
    static RendererStats s_Stats;

    // 全屏四边形资源
    static std::unique_ptr<VertexArray> s_FullscreenQuadVAO;
    static bool s_Initialized;
};

}

using GLRenderer::Renderer;
using GLRenderer::RendererStats;
using GLRenderer::SceneData;
