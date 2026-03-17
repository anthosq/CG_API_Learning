#pragma once

// Components.h - ECS 组件定义
//
// 组件设计原则 (Data-Oriented Design)：
// 1. 组件是纯数据结构 (POD)，不包含逻辑
// 2. 组件应该小而专注，只存储必要数据
// 3. 避免组件之间的引用，使用 EntityID 代替
// 4. 使用值类型而非指针（利用 Cache Locality）
//
// EnTT 组件特点：
// - 组件存储在连续内存中 (Sparse Set)
// - 相同类型组件在遍历时有极好的缓存局部性
// - 支持任意类型作为组件（包括基本类型）
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <vector>
#include <entt/entt.hpp>
#include "core/UUID.h"

namespace GLRenderer {

// 前向声明
class Shader;
class Texture;
class VertexArray;
class Mesh;

namespace ECS {

// ============================================================================
// 核心组件
// ============================================================================

/// ID 组件 - 存储实体的唯一标识符
/// 每个实体都应该有一个 IDComponent，用于序列化和跨帧引用
struct IDComponent {
    UUID ID;

    IDComponent() = default;
    IDComponent(const UUID& uuid) : ID(uuid) {}
    IDComponent(uint64_t uuid) : ID(uuid) {}

    operator uint64_t() const { return static_cast<uint64_t>(ID); }
};

/// 标签组件 - 用于标识和分类实体
struct TagComponent {
    std::string Name = "Entity";
    std::string Tag = "Untagged";    // 分类标签 (如 "Player", "Enemy", "Static")
    bool Active = true;              // 是否激活

    TagComponent() = default;
    TagComponent(const std::string& name) : Name(name) {}
    TagComponent(const std::string& name, const std::string& tag)
        : Name(name), Tag(tag) {}
};

/// 变换组件 - 存储位置、旋转、缩放
struct TransformComponent {
    glm::vec3 Position = glm::vec3(0.0f);
    glm::quat Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // 单位四元数
    glm::vec3 Scale = glm::vec3(1.0f);

    // 父实体 ID（用于层级关系）
    entt::entity Parent = entt::null;

    // 缓存的矩阵（由 TransformSystem 更新）
    glm::mat4 LocalMatrix = glm::mat4(1.0f);
    glm::mat4 WorldMatrix = glm::mat4(1.0f);
    bool Dirty = true;

    TransformComponent() = default;
    TransformComponent(const glm::vec3& pos) : Position(pos) {}
    TransformComponent(const glm::vec3& pos, const glm::vec3& eulerAngles)
        : Position(pos)
        , Rotation(glm::quat(glm::radians(eulerAngles))) {}

    // 便捷方法
    void SetEulerAngles(const glm::vec3& eulerDegrees) {
        Rotation = glm::quat(glm::radians(eulerDegrees));
        Dirty = true;
    }

    glm::vec3 GetEulerAngles() const {
        return glm::degrees(glm::eulerAngles(Rotation));
    }

    glm::vec3 GetForward() const {
        return glm::normalize(Rotation * glm::vec3(0.0f, 0.0f, -1.0f));
    }

    glm::vec3 GetRight() const {
        return glm::normalize(Rotation * glm::vec3(1.0f, 0.0f, 0.0f));
    }

    glm::vec3 GetUp() const {
        return glm::normalize(Rotation * glm::vec3(0.0f, 1.0f, 0.0f));
    }
};

/// 层级关系组件 - 存储子实体列表
struct HierarchyComponent {
    std::vector<entt::entity> Children;

    void AddChild(entt::entity child) {
        Children.push_back(child);
    }

    void RemoveChild(entt::entity child) {
        Children.erase(
            std::remove(Children.begin(), Children.end(), child),
            Children.end()
        );
    }
};

// 渲染组件
/// 网格渲染组件 - 引用网格资源
struct MeshComponent {
    VertexArray* VAO = nullptr;      // 顶点数组（非拥有）
    uint32_t VertexCount = 0;
    uint32_t IndexCount = 0;
    bool UseIndices = false;

    MeshComponent() = default;
    MeshComponent(VertexArray* vao, uint32_t vertexCount)
        : VAO(vao), VertexCount(vertexCount) {}
};

/// 材质组件 - PBR 材质属性
struct MaterialComponent {
    // 纹理引用（非拥有）
    Texture* AlbedoMap = nullptr;
    Texture* NormalMap = nullptr;
    Texture* MetallicMap = nullptr;
    Texture* RoughnessMap = nullptr;
    Texture* AOMap = nullptr;

    // 标量属性
    glm::vec3 Albedo = glm::vec3(1.0f);
    float Metallic = 0.0f;
    float Roughness = 0.5f;
    float AO = 1.0f;

    // 旧式 Phong 属性（向后兼容）
    Texture* DiffuseMap = nullptr;
    Texture* SpecularMap = nullptr;
    float Shininess = 32.0f;

    // 着色器引用
    Shader* ShaderProgram = nullptr;

    MaterialComponent() = default;
};

/// 可见性组件 - 用于剔除系统
struct VisibilityComponent {
    bool Visible = true;
    bool CastShadow = true;
    bool ReceiveShadow = true;
    uint32_t RenderLayer = 0;  // 渲染层掩码
};

/// 包围盒组件 - 用于剔除和碰撞
struct BoundsComponent {
    glm::vec3 LocalMin = glm::vec3(-0.5f);
    glm::vec3 LocalMax = glm::vec3(0.5f);

    // 世界空间包围盒（由系统更新）
    glm::vec3 WorldMin = glm::vec3(-0.5f);
    glm::vec3 WorldMax = glm::vec3(0.5f);

    glm::vec3 GetCenter() const {
        return (WorldMin + WorldMax) * 0.5f;
    }

    glm::vec3 GetExtents() const {
        return (WorldMax - WorldMin) * 0.5f;
    }

    float GetRadius() const {
        return glm::length(GetExtents());
    }
};

// 光照组件
/// 方向光组件
struct DirectionalLightComponent {
    glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 1.0f;

    // 阴影参数
    bool CastShadows = true;
    float ShadowBias = 0.005f;
};

/// 点光源组件
struct PointLightComponent {
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 1.0f;

    // 衰减参数
    float Constant = 1.0f;
    float Linear = 0.09f;
    float Quadratic = 0.032f;

    // 阴影参数
    bool CastShadows = false;
    float ShadowNearPlane = 0.1f;
    float ShadowFarPlane = 25.0f;

    // 计算光照范围
    float GetRange() const {
        // 当衰减到 5/256 时认为超出范围
        float threshold = 5.0f / 256.0f;
        return (-Linear + std::sqrt(Linear * Linear - 4.0f * Quadratic *
               (Constant - Intensity / threshold))) / (2.0f * Quadratic);
    }
};

/// 聚光灯组件
struct SpotLightComponent {
    glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 1.0f;

    // 锥角（度）
    float InnerCutoff = 12.5f;
    float OuterCutoff = 17.5f;

    // 衰减参数
    float Constant = 1.0f;
    float Linear = 0.09f;
    float Quadratic = 0.032f;

    // 阴影参数
    bool CastShadows = false;
};

// 行为组件
/// 旋转器组件 - 每帧自动旋转
struct RotatorComponent {
    glm::vec3 Axis = glm::vec3(0.0f, 1.0f, 0.0f);  // 旋转轴
    float Speed = 45.0f;                            // 度/秒

    RotatorComponent() = default;
    RotatorComponent(const glm::vec3& axis, float speed)
        : Axis(glm::normalize(axis)), Speed(speed) {}
};

/// 浮动组件 - 上下浮动效果
struct FloatingComponent {
    float Amplitude = 0.5f;   // 浮动幅度
    float Frequency = 1.0f;   // 浮动频率 (Hz)
    float Phase = 0.0f;       // 初始相位

    glm::vec3 BasePosition = glm::vec3(0.0f);  // 基准位置
    float ElapsedTime = 0.0f;

    FloatingComponent() = default;
    FloatingComponent(float amplitude, float frequency)
        : Amplitude(amplitude), Frequency(frequency) {}
};

/// 轨道运动组件 - 围绕某点旋转
struct OrbiterComponent {
    entt::entity Target = entt::null;           // 围绕的目标实体
    glm::vec3 CenterPoint = glm::vec3(0.0f);    // 或固定中心点
    glm::vec3 Axis = glm::vec3(0.0f, 1.0f, 0.0f);
    float Radius = 5.0f;
    float Speed = 30.0f;                         // 度/秒
    float CurrentAngle = 0.0f;

    OrbiterComponent() = default;
    OrbiterComponent(const glm::vec3& center, float radius, float speed)
        : CenterPoint(center), Radius(radius), Speed(speed) {}
};

// 相机组件
/// 相机组件
struct CameraComponent {
    float FOV = 45.0f;
    float NearPlane = 0.1f;
    float FarPlane = 1000.0f;
    bool Primary = false;  // 是否为主相机

    // 正交投影参数
    bool Orthographic = false;
    float OrthoSize = 10.0f;

    // 缓存的矩阵
    glm::mat4 ViewMatrix = glm::mat4(1.0f);
    glm::mat4 ProjectionMatrix = glm::mat4(1.0f);
};

// 调试组件
/// 调试绘制组件 - 绘制辅助线/包围盒等
struct DebugDrawComponent {
    bool DrawBounds = false;
    bool DrawAxes = false;
    bool DrawWireframe = false;
    glm::vec3 Color = glm::vec3(0.0f, 1.0f, 0.0f);
};

} // namespace ECS
} // namespace GLRenderer
