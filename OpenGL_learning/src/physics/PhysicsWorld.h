#pragma once

#include <unordered_map>
#include <entt/entt.hpp>
#include "core/Ref.h"
#include "physics/FluidSimulation.h"
#include "physics/EmitterFluidSimulation.h"

namespace GLRenderer {

class FluidSimulation;

// PhysicsWorld：持有所有运行时物理模拟实例，提供统一的 Step() 入口。
// 由 PhysicsSystem 拥有，通过 ECS World 的组件懒创建各 Simulation。
// Phase 12-H：新增 EmitterFluidSimulation（所有发射器共享的独立 PBF 仿真池）。
class PhysicsWorld {
public:
    PhysicsWorld()  = default;
    ~PhysicsWorld() = default;

    // 推进一个子步（dt 已由调用方除以 substeps）
    // 同时驱动主仿真和发射器仿真的 Step
    void Step(float dt);

    // 注册 / 移除流体模拟实例（由 PhysicsSystem 调用）
    void RegisterFluid(entt::entity entity, const Ref<FluidSimulation>& sim);
    void UnregisterFluid(entt::entity entity);

    // 清除全部模拟（ExitPlayMode 时由 PlayWorld 析构自动触发）
    void Clear();

    int GetFluidCount() const { return static_cast<int>(m_FluidSims.size()); }

    // 发射器仿真（Phase 12-H）
    void CreateEmitterSim(int maxParticles  = 16384,
                          float radius      = 0.007f,
                          glm::vec3 bmin    = {-4.0f, 0.0f, -4.0f},
                          glm::vec3 bmax    = { 4.0f, 8.0f,  4.0f});
    const Ref<EmitterFluidSimulation>& GetEmitterSim() const { return m_EmitterSim; }
    bool                               HasEmitterSim() const { return m_EmitterSim != nullptr; }

    void SetSceneGBuffer(GLuint depthTex, GLuint normalTex,
                         const glm::mat4& viewProj, const glm::mat4& invViewProj);

private:
    std::unordered_map<entt::entity, Ref<FluidSimulation>> m_FluidSims;
    Ref<EmitterFluidSimulation>                             m_EmitterSim;
};

} // namespace GLRenderer
