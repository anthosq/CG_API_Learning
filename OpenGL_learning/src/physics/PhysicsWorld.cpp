#include "PhysicsWorld.h"
#include "FluidSimulation.h"
#include <iostream>

namespace GLRenderer {

void PhysicsWorld::Step(float dt) {
    for (auto& [entity, sim] : m_FluidSims) {
        if (sim)
            sim->Step(dt);
    }
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
}

} // namespace GLRenderer
