#pragma once

#include "scene/ecs/Systems.h"
#include "scene/ecs/Components.h"
#include "physics/PhysicsWorld.h"
#include "physics/FluidSimulation.h"
#include "core/Ref.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace GLRenderer {

// PhysicsSystem：ECS 侧物理驱动器。
//
// 职责：
//   1. 将 FluidComponent 的仿真域与 TransformComponent 同步
//      — Position = 域底面中心，Scale = 域宽/高/深（XYZ）
//      — BoundaryMin = {pos.x - w/2, pos.y, pos.z - d/2}
//      — BoundaryMax = {pos.x + w/2, pos.y + h, pos.z + d/2}
//   2. 若边界发生变化则重置仿真（自动重建 grid/SSBO）
//   3. 懒创建 FluidSimulation（首次 Update 时）
//   4. 驱动 PhysicsWorld::Step()，支持 substep 细分
//
// 生命周期：
//   - 由 EditorApp 注册到 SystemManager，仅在 IsPlaying() 时被调用
//   - PlayWorld 销毁时，FluidComponent::Runtime（Ref）引用计数归零，
//     GPU 资源自动释放，无需手动 Unregister
class PhysicsSystem : public ECS::ISystem {
public:
    void OnCreate(ECS::World& world) override {
        (void)world;
        std::cout << "[PhysicsSystem] OnCreate" << std::endl;
    }

    void OnDestroy(ECS::World& world) override {
        (void)world;
        m_PhysicsWorld.Clear();
        std::cout << "[PhysicsSystem] OnDestroy" << std::endl;
    }

    void Update(ECS::World& world, float deltaTime) override {
        if (!m_Enabled) return;

        // ① Transform → BoundaryMin/Max 同步
        // 有 TransformComponent 的流体实体：用 Position + Scale 驱动仿真域
        world.Each<ECS::TransformComponent, ECS::FluidComponent>(
            [&](ECS::Entity entity, ECS::TransformComponent& tr, ECS::FluidComponent& fluid) {
                const float hw = tr.Scale.x * 0.5f;
                const float hd = tr.Scale.z * 0.5f;
                const glm::vec3 newMin = { tr.Position.x - hw,
                                           tr.Position.y,
                                           tr.Position.z - hd };
                const glm::vec3 newMax = { tr.Position.x + hw,
                                           tr.Position.y + tr.Scale.y,
                                           tr.Position.z + hd };

                constexpr float kEps = 1e-4f;
                bool changed =
                    glm::any(glm::greaterThan(glm::abs(newMin - fluid.BoundaryMin), glm::vec3(kEps))) ||
                    glm::any(glm::greaterThan(glm::abs(newMax - fluid.BoundaryMax), glm::vec3(kEps)));

                if (changed) {
                    fluid.BoundaryMin = newMin;
                    fluid.BoundaryMax = newMax;
                    if (fluid.Runtime) {
                        // 边界变化 → 重建 grid，需重置模拟
                        m_PhysicsWorld.UnregisterFluid(entity.GetHandle());
                        fluid.Runtime = nullptr;
                    }
                }
            }
        );

        // ② 懒创建：为尚未初始化的 FluidComponent 构建 FluidSimulation
        world.Each<ECS::FluidComponent>([&](ECS::Entity entity, ECS::FluidComponent& fluid) {
            if (!fluid.Runtime) {
                fluid.Runtime = Ref<FluidSimulation>::Create(fluid);
                m_PhysicsWorld.RegisterFluid(entity.GetHandle(), fluid.Runtime);
            }
        });

        // ③ 固定步长驱动（与参考实现一致）
        // 参考实现 DELTA_TIME = 0.0016s，每帧恰好 1 步，不随帧率变化。
        // 低帧率时物理呈慢动作，但计算量不变 → 无性能雪崩。
        // MAX_CATCH_UP = 2：允许偶发卡顿后最多追 2 步，避免时间累计误差过大。
        constexpr float FIXED_PHYSICS_DT = 0.0016f;  // 参考实现固定步长
        constexpr int   MAX_CATCH_UP     = 2;         // 每帧最多物理步数
        const int steps = std::min(
            std::max(1, static_cast<int>(std::round(deltaTime / FIXED_PHYSICS_DT))),
            MAX_CATCH_UP);
        for (int i = 0; i < steps; ++i)
            m_PhysicsWorld.Step(FIXED_PHYSICS_DT);
    }

    PhysicsWorld& GetPhysicsWorld() { return m_PhysicsWorld; }

private:
    PhysicsWorld m_PhysicsWorld;
};

} // namespace GLRenderer
