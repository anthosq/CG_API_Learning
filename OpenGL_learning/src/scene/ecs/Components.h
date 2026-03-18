#pragma once

// Components.h - ECS 组件定义
//
// 组件设计原则 (Data-Oriented Design)：
// 1. 组件是纯数据结构 (POD)，不包含逻辑
// 2. 组件应该小而专注，只存储必要数据
// 3. 避免组件之间的引用，使用 EntityID 或 AssetHandle 代替
// 4. 使用值类型而非指针（利用 Cache Locality）
//
// 资产引用原则：
// - 组件中使用 AssetHandle (UUID) 引用资产
// - 渲染时通过 AssetManager 查找实际资源
// - 避免存储原始指针，便于序列化和生命周期管理

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <entt/entt.hpp>
#include "core/UUID.h"
#include "asset/AssetTypes.h"

namespace GLRenderer {

// 前向声明
class VertexArray;

namespace ECS {

// 核心组件

struct IDComponent {
    UUID ID;

    IDComponent() = default;
    IDComponent(const UUID& uuid) : ID(uuid) {}
    IDComponent(uint64_t uuid) : ID(uuid) {}

    operator uint64_t() const { return static_cast<uint64_t>(ID); }
};

struct TagComponent {
    std::string Name = "Entity";
    std::string Tag = "Untagged";
    bool Active = true;

    TagComponent() = default;
    TagComponent(const std::string& name) : Name(name) {}
    TagComponent(const std::string& name, const std::string& tag)
        : Name(name), Tag(tag) {}
};

struct TransformComponent {
    glm::vec3 Position = glm::vec3(0.0f);
    glm::quat Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 Scale = glm::vec3(1.0f);

    entt::entity Parent = entt::null;

    glm::mat4 LocalMatrix = glm::mat4(1.0f);
    glm::mat4 WorldMatrix = glm::mat4(1.0f);
    bool Dirty = true;

    TransformComponent() = default;

    TransformComponent(const glm::vec3& pos) : Position(pos) {
        UpdateMatrices();
    }

    TransformComponent(const glm::vec3& pos, const glm::vec3& eulerAngles)
        : Position(pos)
        , Rotation(glm::quat(glm::radians(eulerAngles))) {
        UpdateMatrices();
    }

    void UpdateMatrices() {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), Position);
        glm::mat4 rotation = glm::toMat4(Rotation);
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), Scale);
        LocalMatrix = translation * rotation * scale;
        WorldMatrix = LocalMatrix;
        Dirty = false;
    }

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

struct MeshComponent {
    VertexArray* VAO = nullptr;
    uint32_t VertexCount = 0;
    uint32_t IndexCount = 0;
    bool UseIndices = false;

    bool HasMesh() const { return VAO != nullptr; }

    MeshComponent() = default;
    MeshComponent(VertexArray* vao, uint32_t vertexCount)
        : VAO(vao), VertexCount(vertexCount) {}
    MeshComponent(VertexArray* vao, uint32_t vertexCount, uint32_t indexCount)
        : VAO(vao), VertexCount(vertexCount), IndexCount(indexCount), UseIndices(true) {}
};

struct ModelComponent {
    AssetHandle Handle;
    std::string AssetPath;
    bool Visible = true;

    bool HasModel() const { return Handle.IsValid(); }

    ModelComponent() = default;
    ModelComponent(AssetHandle handle) : Handle(handle) {}
    ModelComponent(const std::string& path) : AssetPath(path) {}
    ModelComponent(AssetHandle handle, const std::string& path)
        : Handle(handle), AssetPath(path) {}
};

struct MaterialComponent {
    // PBR 纹理 (AssetHandle)
    AssetHandle AlbedoMap;
    AssetHandle NormalMap;
    AssetHandle MetallicMap;
    AssetHandle RoughnessMap;
    AssetHandle AOMap;

    // 标量属性
    glm::vec3 Albedo = glm::vec3(1.0f);
    float Metallic = 0.0f;
    float Roughness = 0.5f;
    float AO = 1.0f;

    // Phong 着色 (AssetHandle)
    AssetHandle DiffuseMapHandle;
    AssetHandle SpecularMapHandle;
    float Shininess = 32.0f;

    // 着色器
    AssetHandle ShaderHandle;

    bool HasDiffuseMap() const { return DiffuseMapHandle.IsValid(); }
    bool HasSpecularMap() const { return SpecularMapHandle.IsValid(); }

    MaterialComponent() = default;
};

struct VisibilityComponent {
    bool Visible = true;
    bool CastShadow = true;
    bool ReceiveShadow = true;
    uint32_t RenderLayer = 0;
};

struct BoundsComponent {
    glm::vec3 LocalMin = glm::vec3(-0.5f);
    glm::vec3 LocalMax = glm::vec3(0.5f);
    glm::vec3 WorldMin = glm::vec3(-0.5f);
    glm::vec3 WorldMax = glm::vec3(0.5f);

    glm::vec3 GetCenter() const { return (WorldMin + WorldMax) * 0.5f; }
    glm::vec3 GetExtents() const { return (WorldMax - WorldMin) * 0.5f; }
    float GetRadius() const { return glm::length(GetExtents()); }
};

// 光照组件

struct DirectionalLightComponent {
    glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 1.0f;
    bool CastShadows = true;
    float ShadowBias = 0.005f;
};

struct PointLightComponent {
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 1.0f;
    float Constant = 1.0f;
    float Linear = 0.09f;
    float Quadratic = 0.032f;
    bool CastShadows = false;
    float ShadowNearPlane = 0.1f;
    float ShadowFarPlane = 25.0f;

    float GetRange() const {
        float threshold = 5.0f / 256.0f;
        return (-Linear + std::sqrt(Linear * Linear - 4.0f * Quadratic *
               (Constant - Intensity / threshold))) / (2.0f * Quadratic);
    }
};

struct SpotLightComponent {
    glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 1.0f;
    float InnerCutoff = 12.5f;
    float OuterCutoff = 17.5f;
    float Constant = 1.0f;
    float Linear = 0.09f;
    float Quadratic = 0.032f;
    bool CastShadows = false;
};

// 行为组件

struct RotatorComponent {
    glm::vec3 Axis = glm::vec3(0.0f, 1.0f, 0.0f);
    float Speed = 45.0f;

    RotatorComponent() = default;
    RotatorComponent(const glm::vec3& axis, float speed)
        : Axis(glm::normalize(axis)), Speed(speed) {}
};

struct FloatingComponent {
    float Amplitude = 0.5f;
    float Frequency = 1.0f;
    float Phase = 0.0f;
    glm::vec3 BasePosition = glm::vec3(0.0f);
    float ElapsedTime = 0.0f;

    FloatingComponent() = default;
    FloatingComponent(float amplitude, float frequency)
        : Amplitude(amplitude), Frequency(frequency) {}
};

struct OrbiterComponent {
    entt::entity Target = entt::null;
    glm::vec3 CenterPoint = glm::vec3(0.0f);
    glm::vec3 Axis = glm::vec3(0.0f, 1.0f, 0.0f);
    float Radius = 5.0f;
    float Speed = 30.0f;
    float CurrentAngle = 0.0f;

    OrbiterComponent() = default;
    OrbiterComponent(const glm::vec3& center, float radius, float speed)
        : CenterPoint(center), Radius(radius), Speed(speed) {}
};

// 相机组件

struct CameraComponent {
    float FOV = 45.0f;
    float NearPlane = 0.1f;
    float FarPlane = 1000.0f;
    bool Primary = false;
    bool Orthographic = false;
    float OrthoSize = 10.0f;
    glm::mat4 ViewMatrix = glm::mat4(1.0f);
    glm::mat4 ProjectionMatrix = glm::mat4(1.0f);
};

// 精灵/Billboard 组件

struct SpriteComponent {
    AssetHandle TextureHandle;          // 纹理资源句柄
    glm::vec4 Color = glm::vec4(1.0f);  // 颜色叠加 (RGBA)
    glm::vec2 Size = glm::vec2(1.0f);   // Billboard 大小
    bool Billboard = true;              // 是否始终面向相机
    bool FixedSize = false;             // 是否固定屏幕大小（不随距离缩放）
    int SortOrder = 0;                  // 渲染排序顺序

    bool HasTexture() const { return TextureHandle.IsValid(); }

    SpriteComponent() = default;
    SpriteComponent(AssetHandle textureHandle, const glm::vec2& size = glm::vec2(1.0f))
        : TextureHandle(textureHandle), Size(size) {}
};

// 调试组件

struct DebugDrawComponent {
    bool DrawBounds = false;
    bool DrawAxes = false;
    bool DrawWireframe = false;
    glm::vec3 Color = glm::vec3(0.0f, 1.0f, 0.0f);
};

} // namespace ECS
} // namespace GLRenderer
