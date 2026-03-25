#pragma once

#include "graphics/Shader.h"
#include "graphics/Buffer.h"
#include "graphics/MeshSource.h"
#include "RenderCommand.h"
#include "asset/AssetTypes.h"
#include "asset/MaterialAsset.h"
#include "core/Ref.h"
#include "core/Frustum.h"
#include <glm/glm.hpp>

namespace GLRenderer {

// UBO 数据结构 (std140 布局)

// 相机 UBO (绑定点 0)
// std140 对齐规则:
// - mat4: 64 bytes
// - vec3: 16 bytes (按 vec4 对齐)
struct CameraUBO {
    glm::mat4 ViewProjection;  // offset 0,   size 64
    glm::mat4 View;            // offset 64,  size 64
    glm::mat4 Projection;      // offset 128, size 64
    glm::vec4 Position;        // offset 192, size 16 (w 分量未使用，用于对齐)
};  // Total: 208 bytes

constexpr int MAX_POINT_LIGHTS    = 256;  // Tiled Forward+ 支持大量点光源
constexpr int TILE_SIZE           = 16;   // 屏幕 tile 边长（像素）
constexpr int MAX_LIGHTS_PER_TILE = 128;  // 每 tile 最多可见光源数

// 光照 UBO (绑定点 1), std140 布局
// 点光源数据已迁移至 SSBO（PointLightSSBO），此处只保留方向光/聚光灯/元数据
struct LightingUBO {
    // 方向光
    glm::vec4 DirLightDirection;      // xyz = 方向, w = 强度
    glm::vec4 DirLightColor;          // xyz = 颜色, w = shadowAmount

    // 聚光灯 (单个)
    glm::vec4 SpotLightPosRange;          // xyz = 位置, w = 范围
    glm::vec4 SpotLightDirAngle;          // xyz = 方向, w = 角度(度)
    glm::vec4 SpotLightColorIntensity;    // xyz = 颜色, w = 强度
    glm::vec4 SpotLightParams;            // x = falloff, y = angleAttenuation

    // 元数据
    glm::ivec4 LightCounts;           // x = 点光源数量, y = 聚光灯启用(0/1)
    glm::vec4 AmbientColor;           // xyz = 环境光颜色, w = 强度
};

// 点光源 GPU 数据（std430，写入 SSBO）
struct PointLightGPU {
    glm::vec4 PosRadius;        // xyz = 世界空间位置, w = 影响半径
    glm::vec4 ColorIntensity;   // xyz = 颜色, w = 强度
    glm::vec4 Params;           // x = falloff 指数, y = shadowSlot(-1=不投影), z = farPlane, w = 未使用
};

// SSBO 绑定点
constexpr uint32_t SSBO_BINDING_POINT_LIGHTS       = 0;  // PointLightGPU[]
constexpr uint32_t SSBO_BINDING_TILE_LIGHT_COUNTS  = 1;  // int[numTiles]
constexpr uint32_t SSBO_BINDING_TILE_LIGHT_INDICES = 2;  // int[numTiles * MAX_LIGHTS_PER_TILE]

// Shadow UBO (绑定点 3), std140 布局
// 注意 std140 规则: mat4=64B, vec4=16B, float/int=4B, 结构体末尾须 16B 对齐
struct ShadowUBO {
    glm::mat4 LightSpaceMatrices[4];  // offset   0, size 256 (4×64)
    glm::vec4 CascadeSplits;          // offset 256, size  16 (xyzw = split[0..3])
    float     MaxShadowDistance;      // offset 272, size   4
    float     ShadowFade;             // offset 276, size   4
    int       CascadeCount;           // offset 280, size   4
    float     CascadeTransitionFade;  // offset 284, size   4  (cascade 过渡区宽度，世界单位)
};  // 总计 288 bytes

// UBO 绑定点常量
constexpr uint32_t UBO_BINDING_CAMERA   = 0;
constexpr uint32_t UBO_BINDING_LIGHTING = 1;
constexpr uint32_t UBO_BINDING_MATERIAL = 2;  // 预留给材质 UBO
constexpr uint32_t UBO_BINDING_SHADOW   = 3;  // 阴影数据

// 阴影贴图纹理槽（sampler2DArray，4 个 cascade 共用）
constexpr uint32_t SHADOW_MAP_SLOT          = 9;
// SSAO 纹理槽（PBR GeometryPass 读取 blurred AO）
constexpr uint32_t SSAO_TEXTURE_SLOT        = 10;
// 点光源阴影数组槽（samplerCubeArray，MAX_SHADOW_POINT_LIGHTS 个 cubemap）
constexpr uint32_t POINT_SHADOW_MAP_SLOT    = 11;
// 最多支持投影的点光源数量
constexpr int      MAX_SHADOW_POINT_LIGHTS  = 4;

// 单个 cascade 的 CPU 端数据（CalculateCascades 输出，不上传 GPU）
struct CascadeData {
    glm::mat4 ViewProj;         // 光源空间 view-projection（写入 ShadowUBO）
    Frustum   CascadeFrustum;   // 该 cascade 的裁剪视锥（BVH Query 用）
    float     SplitDepth;       // 相机空间切分深度（写入 ShadowUBO::CascadeSplits）
};

// 前向声明已移除 - 统一使用 MeshSource

enum class BlendMode {
    None,
    Alpha,
    Additive
};

enum class DepthCompareOp {
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always
};

struct PipelineSpecification {
    Ref<Shader> ShaderProgram;
    DepthCompareOp DepthOp = DepthCompareOp::Less;
    bool DepthTest = true;
    bool DepthWrite = true;
    bool BackfaceCulling = false;  // 默认禁用，让 Pass 自己决定
    BlendMode Blend = BlendMode::None;
    bool Wireframe = false;

    // 不透明物体（禁用背面剔除以兼容各种模型）
    static PipelineSpecification Opaque() {
        PipelineSpecification spec;
        spec.DepthTest = true;
        spec.DepthWrite = true;
        spec.BackfaceCulling = false;  // 禁用，避免 FBX 面法线问题
        spec.Blend = BlendMode::None;
        return spec;
    }

    // 透明物体
    static PipelineSpecification Transparent() {
        PipelineSpecification spec;
        spec.DepthTest = true;
        spec.DepthWrite = false;
        spec.BackfaceCulling = false;
        spec.Blend = BlendMode::Alpha;
        return spec;
    }

    // 阴影
    static PipelineSpecification Shadow() {
        PipelineSpecification spec;
        spec.DepthTest = true;
        spec.DepthWrite = true;
        spec.BackfaceCulling = false;
        spec.Blend = BlendMode::None;
        return spec;
    }

    // Billboard（始终面向相机的 2D 图标）
    static PipelineSpecification Billboard() {
        PipelineSpecification spec;
        spec.DepthTest = true;
        spec.DepthWrite = false;  // 不写入深度
        spec.BackfaceCulling = false;
        spec.Blend = BlendMode::Alpha;
        return spec;
    }
};

class Pipeline {
public:
    Pipeline() = default;
    explicit Pipeline(const PipelineSpecification& spec) : m_Spec(spec) {}

    // 完整绑定所有状态（用于需要完全控制状态的场景）
    void Bind() const {
        if (m_Spec.ShaderProgram) {
            m_Spec.ShaderProgram->Bind();
        }
        ApplyRenderState();
    }

    // 仅应用渲染状态，不绑定着色器
    void ApplyRenderState() const {
        // 深度测试
        if (m_Spec.DepthTest) {
            RenderCommand::EnableDepthTest();
            GLenum depthFunc = GL_LESS;
            switch (m_Spec.DepthOp) {
                case DepthCompareOp::Never:          depthFunc = GL_NEVER; break;
                case DepthCompareOp::Less:           depthFunc = GL_LESS; break;
                case DepthCompareOp::Equal:          depthFunc = GL_EQUAL; break;
                case DepthCompareOp::LessOrEqual:    depthFunc = GL_LEQUAL; break;
                case DepthCompareOp::Greater:        depthFunc = GL_GREATER; break;
                case DepthCompareOp::NotEqual:       depthFunc = GL_NOTEQUAL; break;
                case DepthCompareOp::GreaterOrEqual: depthFunc = GL_GEQUAL; break;
                case DepthCompareOp::Always:         depthFunc = GL_ALWAYS; break;
            }
            RenderCommand::SetDepthFunc(depthFunc);
        } else {
            RenderCommand::DisableDepthTest();
        }

        // 深度写入
        RenderCommand::SetDepthMask(m_Spec.DepthWrite);

        // 背面剔除 - 仅在明确需要时设置
        if (m_Spec.BackfaceCulling) {
            RenderCommand::EnableFaceCulling();
            RenderCommand::SetCullFace(GL_BACK);
        } else {
            RenderCommand::DisableFaceCulling();
        }

        // 混合模式
        switch (m_Spec.Blend) {
            case BlendMode::None:
                RenderCommand::DisableBlending();
                break;
            case BlendMode::Alpha:
                RenderCommand::EnableBlending();
                RenderCommand::SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case BlendMode::Additive:
                RenderCommand::EnableBlending();
                RenderCommand::SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
                break;
        }

        // 线框模式
        RenderCommand::SetPolygonMode(GL_FRONT_AND_BACK, m_Spec.Wireframe ? GL_LINE : GL_FILL);
    }

    // 注意：不再提供 Unbind()，每个 Pass 应自己管理状态恢复

    const Ref<Shader>& GetShader() const { return m_Spec.ShaderProgram; }
    void SetShader(const Ref<Shader>& shader) { m_Spec.ShaderProgram = shader; }

    PipelineSpecification& GetSpecification() { return m_Spec; }
    const PipelineSpecification& GetSpecification() const { return m_Spec; }

private:
    PipelineSpecification m_Spec;
};

// 统一绘制命令 - 所有网格都通过 MeshSource 渲染
struct DrawCommand {
    // 网格数据
    Ref<MeshSource> MeshSource;
    uint32_t SubmeshIndex = 0;

    // 变换
    glm::mat4 Transform = glm::mat4(1.0f);
    glm::mat3 NormalMatrix = glm::mat3(1.0f);

    // 材质表 (组件级覆盖，为空时使用 MeshSource/StaticMesh 默认材质)
    Ref<MaterialTable> Materials;

    // 实体 ID (用于拾取)
    int EntityID = -1;

    // 排序键 (透明物体用)
    float DistanceToCamera = 0.0f;

    // 排序比较 (由远到近，用于透明物体)
    bool operator<(const DrawCommand& other) const {
        return DistanceToCamera > other.DistanceToCamera;
    }
};

} // namespace GLRenderer
