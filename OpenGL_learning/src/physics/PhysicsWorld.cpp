#include "PhysicsWorld.h"
#include "FluidSimulation.h"
#include "EmitterFluidSimulation.h"
#include <iostream>

namespace GLRenderer {

void PhysicsWorld::Step(float dt) {
    for (auto& [entity, sim] : m_FluidSims) {
        if (sim)
            sim->Step(dt);
    }
    if (m_EmitterSim && m_EmitterSim->IsReady())
        m_EmitterSim->Step(dt);
}

void PhysicsWorld::RegisterFluid(entt::entity entity, const Ref<FluidSimulation>& sim) {
    m_FluidSims[entity] = sim;
    std::cout << "[PhysicsWorld] Registered FluidSimulation for entity "
              << static_cast<uint32_t>(entity) << std::endl;
}

void PhysicsWorld::UnregisterFluid(entt::entity entity) {
    m_FluidSims.erase(entity);
}

void PhysicsWorld::Clear() {
    m_FluidSims.clear();
    m_EmitterSim.Reset();
}

void PhysicsWorld::CreateEmitterSim(int maxParticles, float radius,
                                     glm::vec3 bmin, glm::vec3 bmax) {
    m_EmitterSim = Ref<EmitterFluidSimulation>::Create(maxParticles, radius, bmin, bmax);
}

void PhysicsWorld::SetSceneGBuffer(GLuint depthTex, GLuint normalTex,
                                    const glm::mat4& viewProj,
                                    const glm::mat4& invViewProj) {
    for (auto& [entity, sim] : m_FluidSims)
        if (sim) sim->SetSceneGBuffer(depthTex, normalTex, viewProj, invViewProj);
    if (m_EmitterSim)
        m_EmitterSim->SetSceneGBuffer(depthTex, normalTex, viewProj, invViewProj);
}

} // namespace GLRenderer
