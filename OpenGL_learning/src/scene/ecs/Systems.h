#pragma once

// Systems.h - ECS 系统定义
//
// 系统设计原则：
// 1. 系统包含逻辑，组件只包含数据
// 2. 系统通过 View/Group 查询组件，处理匹配的实体
// 3. 系统之间应该解耦，通过组件数据通信
// 4. 系统执行顺序很重要（如 Transform 系统应在渲染系统之前）
//
// 系统分类：
// - Update Systems: 每帧更新（物理、动画、AI）
// - Render Systems: 渲染相关（网格渲染、粒子、UI）
// - Event Systems: 事件驱动（碰撞响应、输入处理）

#include "World.h"
#include "Components.h"
#include <glm/gtc/matrix_transform.hpp>

namespace GLRenderer {
namespace ECS {

// System 基类（可选，EnTT 不强制使用）
class ISystem {
public:
    virtual ~ISystem() = default;

    virtual void OnCreate(World& world) {}
    virtual void OnDestroy(World& world) {}
    virtual void Update(World& world, float deltaTime) = 0;

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

protected:
    bool m_Enabled = true;
};

// TransformSystem - 更新变换矩阵
// 职责：
// 1. 计算本地变换矩阵 (TRS)
// 2. 根据父子关系计算世界变换矩阵
// 3. 更新包围盒的世界空间坐标
//

class TransformSystem : public ISystem {
public:
    void Update(World& world, float deltaTime) override {
        if (!m_Enabled) return;

        // 更新所有变换组件
        world.Each<TransformComponent>([](Entity entity, TransformComponent& transform) {
            if (transform.Dirty) {
                UpdateLocalMatrix(transform);
            }
        });

        // 更新世界矩阵（考虑层级关系）
        // 先处理没有父节点的实体
        world.Each<TransformComponent>([&world](Entity entity, TransformComponent& transform) {
            if (transform.Parent == entt::null) {
                UpdateWorldMatrixRecursive(world, entity, transform, glm::mat4(1.0f));
            }
        });

        // 更新包围盒
        world.Each<TransformComponent, BoundsComponent>(
            [](Entity entity, TransformComponent& transform, BoundsComponent& bounds) {
                UpdateBounds(transform, bounds);
            });
    }

private:
    static void UpdateLocalMatrix(TransformComponent& transform) {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.Position);
        glm::mat4 rotation = glm::toMat4(transform.Rotation);
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform.Scale);

        transform.LocalMatrix = translation * rotation * scale;
        transform.Dirty = false;
    }

    static void UpdateWorldMatrixRecursive(World& world, Entity entity,
                                           TransformComponent& transform,
                                           const glm::mat4& parentWorld) {
        transform.WorldMatrix = parentWorld * transform.LocalMatrix;

        // 递归更新子实体
        if (entity.HasComponent<HierarchyComponent>()) {
            auto& hierarchy = entity.GetComponent<HierarchyComponent>();
            for (auto childHandle : hierarchy.Children) {
                if (world.IsValid(childHandle)) {
                    Entity child(childHandle, &world);
                    if (child.HasComponent<TransformComponent>()) {
                        auto& childTransform = child.GetComponent<TransformComponent>();
                        UpdateWorldMatrixRecursive(world, child, childTransform, transform.WorldMatrix);
                    }
                }
            }
        }
    }

    static void UpdateBounds(const TransformComponent& transform, BoundsComponent& bounds) {
        // 变换包围盒的8个角点
        glm::vec3 corners[8] = {
            {bounds.LocalMin.x, bounds.LocalMin.y, bounds.LocalMin.z},
            {bounds.LocalMax.x, bounds.LocalMin.y, bounds.LocalMin.z},
            {bounds.LocalMax.x, bounds.LocalMax.y, bounds.LocalMin.z},
            {bounds.LocalMin.x, bounds.LocalMax.y, bounds.LocalMin.z},
            {bounds.LocalMin.x, bounds.LocalMin.y, bounds.LocalMax.z},
            {bounds.LocalMax.x, bounds.LocalMin.y, bounds.LocalMax.z},
            {bounds.LocalMax.x, bounds.LocalMax.y, bounds.LocalMax.z},
            {bounds.LocalMin.x, bounds.LocalMax.y, bounds.LocalMax.z}
        };

        bounds.WorldMin = glm::vec3(FLT_MAX);
        bounds.WorldMax = glm::vec3(-FLT_MAX);

        for (const auto& corner : corners) {
            glm::vec3 worldCorner = glm::vec3(transform.WorldMatrix * glm::vec4(corner, 1.0f));
            bounds.WorldMin = glm::min(bounds.WorldMin, worldCorner);
            bounds.WorldMax = glm::max(bounds.WorldMax, worldCorner);
        }
    }
};

// BehaviorSystem - 更新行为组件
class BehaviorSystem : public ISystem {
public:
    void Update(World& world, float deltaTime) override {
        if (!m_Enabled) return;

        // 更新旋转器
        world.Each<TransformComponent, RotatorComponent>(
            [deltaTime](Entity entity, TransformComponent& transform, RotatorComponent& rotator) {
                float angle = glm::radians(rotator.Speed * deltaTime);
                glm::quat rotation = glm::angleAxis(angle, rotator.Axis);
                transform.Rotation = rotation * transform.Rotation;
                transform.Dirty = true;
            });

        // 更新浮动组件
        world.Each<TransformComponent, FloatingComponent>(
            [deltaTime](Entity entity, TransformComponent& transform, FloatingComponent& floating) {
                floating.ElapsedTime += deltaTime;
                float offset = floating.Amplitude *
                               std::sin(2.0f * glm::pi<float>() * floating.Frequency *
                                        floating.ElapsedTime + floating.Phase);
                transform.Position.y = floating.BasePosition.y + offset;
                transform.Dirty = true;
            });

        // 更新 Z 轴振荡器
        world.Each<TransformComponent, OscillatorComponent>(
            [deltaTime](Entity entity, TransformComponent& transform, OscillatorComponent& osc) {
                if (osc._time == 0.0f)
                    osc._baseZ = transform.Position.z;
                osc._time += deltaTime;
                transform.Position.z = osc._baseZ +
                    osc.Amplitude * std::sin(2.0f * glm::pi<float>() * osc.Frequency * osc._time + osc.Phase);
                transform.Dirty = true;
            });

        // 更新轨道运动
        world.Each<TransformComponent, OrbiterComponent>(
            [&world, deltaTime](Entity entity, TransformComponent& transform, OrbiterComponent& orbiter) {
                orbiter.CurrentAngle += orbiter.Speed * deltaTime;

                // 获取中心点
                glm::vec3 center = orbiter.CenterPoint;
                if (orbiter.Target != entt::null && world.IsValid(orbiter.Target)) {
                    Entity target(orbiter.Target, &world);
                    if (target.HasComponent<TransformComponent>()) {
                        center = target.GetComponent<TransformComponent>().Position;
                    }
                }

                // 计算新位置
                float rad = glm::radians(orbiter.CurrentAngle);
                glm::vec3 offset(
                    std::cos(rad) * orbiter.Radius,
                    0.0f,
                    std::sin(rad) * orbiter.Radius
                );

                // 应用轴旋转
                glm::quat axisRotation = glm::rotation(glm::vec3(0, 1, 0), orbiter.Axis);
                offset = axisRotation * offset;

                transform.Position = center + offset;
                transform.Dirty = true;
            });
    }
};

// CameraSystem - 更新相机矩阵
class CameraSystem : public ISystem {
public:
    void Update(World& world, float deltaTime) override {
        if (!m_Enabled) return;

        world.Each<TransformComponent, CameraComponent>(
            [this](Entity entity, TransformComponent& transform, CameraComponent& camera) {
                // 计算视图矩阵
                glm::vec3 position = glm::vec3(transform.WorldMatrix[3]);
                glm::vec3 forward = -glm::vec3(transform.WorldMatrix[2]);  // -Z
                glm::vec3 up = glm::vec3(transform.WorldMatrix[1]);        // +Y

                camera.ViewMatrix = glm::lookAt(position, position + forward, up);

                // 计算投影矩阵
                if (camera.Orthographic) {
                    float aspect = m_AspectRatio;
                    float halfWidth = camera.OrthoSize * aspect * 0.5f;
                    float halfHeight = camera.OrthoSize * 0.5f;
                    camera.ProjectionMatrix = glm::ortho(
                        -halfWidth, halfWidth, -halfHeight, halfHeight,
                        camera.NearPlane, camera.FarPlane
                    );
                } else {
                    camera.ProjectionMatrix = glm::perspective(
                        glm::radians(camera.FOV),
                        m_AspectRatio,
                        camera.NearPlane,
                        camera.FarPlane
                    );
                }
            });
    }

    void SetAspectRatio(float aspectRatio) { m_AspectRatio = aspectRatio; }

private:
    float m_AspectRatio = 16.0f / 9.0f;
};

// SystemManager - 系统管理器
class SystemManager {
public:
    template<typename T, typename... Args>
    T* AddSystem(Args&&... args) {
        static_assert(std::is_base_of_v<ISystem, T>, "T must derive from ISystem");

        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = system.get();
        m_Systems.push_back(std::move(system));
        return ptr;
    }

    template<typename T>
    T* GetSystem() {
        for (auto& system : m_Systems) {
            if (auto* ptr = dynamic_cast<T*>(system.get())) {
                return ptr;
            }
        }
        return nullptr;
    }

    void OnCreate(World& world) {
        for (auto& system : m_Systems) {
            system->OnCreate(world);
        }
    }

    void OnDestroy(World& world) {
        for (auto& system : m_Systems) {
            system->OnDestroy(world);
        }
    }

    void Update(World& world, float deltaTime) {
        for (auto& system : m_Systems) {
            if (system->IsEnabled()) {
                system->Update(world, deltaTime);
            }
        }
    }

private:
    std::vector<std::unique_ptr<ISystem>> m_Systems;
};

} // namespace ECS
} // namespace GLRenderer
