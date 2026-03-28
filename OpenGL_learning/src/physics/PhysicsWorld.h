#pragma once

#include <unordered_map>
#include <entt/entt.hpp>
#include "core/Ref.h"
#include "physics/FluidSimulation.h"

namespace GLRenderer {

class FluidSimulation;

// PhysicsWorld：持有所有运行时物理模拟实例，提供统一的 Step() 入口。
// 由 PhysicsSystem 拥有，通过 ECS World 的组件懒创建各 Simulation。
// Phase 11-E 后将在此处理跨类型耦合（布料-流体、软体-碰撞）。
class PhysicsWorld {
public:
    PhysicsWorld()  = default;
    ~PhysicsWorld() = default;

    // 推进一个子步（dt 已由调用方除以 substeps）
    void Step(float dt);

    // 注册 / 移除流体模拟实例（由 PhysicsSystem 调用）
    void RegisterFluid(entt::entity entity, const Ref<FluidSimulation>& sim);
    void UnregisterFluid(entt::entity entity);

    // 清除全部模拟（ExitPlayMode 时由 PlayWorld 析构自动触发）
    void Clear();

    int GetFluidCount() const { return static_cast<int>(m_FluidSims.size()); }

private:
    std::unordered_map<entt::entity, Ref<FluidSimulation>> m_FluidSims;
};

} // namespace GLRenderer
