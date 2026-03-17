#include "World.h"
#include <algorithm>
#include <iostream>

namespace GLRenderer {
namespace ECS {

// 构造/析构
World::World(const std::string& name)
    : m_Name(name)
{
    std::cout << "[ECS] World '" << m_Name << "' created" << std::endl;
}

World::~World() {
    Clear();
    std::cout << "[ECS] World '" << m_Name << "' destroyed" << std::endl;
}

// 实体管理
Entity World::CreateEntity(const std::string& name) {
    entt::entity handle = m_Registry.create();
    Entity entity(handle, this);

    // 自动添加核心组件
    entity.AddComponent<IDComponent>();      // UUID 自动生成
    entity.AddComponent<TagComponent>(name);

    return entity;
}

Entity World::CreateEntity(const std::string& name, const std::string& tag) {
    entt::entity handle = m_Registry.create();
    Entity entity(handle, this);

    // 自动添加核心组件
    entity.AddComponent<IDComponent>();      // UUID 自动生成
    entity.AddComponent<TagComponent>(name, tag);

    return entity;
}

Entity World::CreateEntityWithUUID(UUID uuid, const std::string& name) {
    entt::entity handle = m_Registry.create();
    Entity entity(handle, this);

    // 使用指定的 UUID
    entity.AddComponent<IDComponent>(uuid);
    entity.AddComponent<TagComponent>(name);

    return entity;
}

void World::DestroyEntity(Entity entity) {
    if (IsValid(entity)) {
        // 如果有层级关系，先处理子实体
        if (entity.HasComponent<HierarchyComponent>()) {
            auto& hierarchy = entity.GetComponent<HierarchyComponent>();
            for (auto child : hierarchy.Children) {
                DestroyEntity(Entity(child, this));
            }
        }

        // 如果有父实体，从父实体的子列表中移除
        if (entity.HasComponent<TransformComponent>()) {
            auto& transform = entity.GetComponent<TransformComponent>();
            if (transform.Parent != entt::null && IsValid(transform.Parent)) {
                Entity parent(transform.Parent, this);
                if (parent.HasComponent<HierarchyComponent>()) {
                    parent.GetComponent<HierarchyComponent>().RemoveChild(entity.GetHandle());
                }
            }
        }

        m_Registry.destroy(entity.GetHandle());
    }
}

void World::Clear() {
    m_Registry.clear();
}

bool World::IsValid(Entity entity) const {
    return IsValid(entity.GetHandle());
}

bool World::IsValid(entt::entity handle) const {
    return handle != entt::null && m_Registry.valid(handle);
}

// 实体查找
Entity World::FindByName(const std::string& name) {
    auto view = m_Registry.view<TagComponent>();
    for (auto entity : view) {
        if (view.get<TagComponent>(entity).Name == name) {
            return Entity(entity, this);
        }
    }
    return Entity();  // 返回无效实体
}

std::vector<Entity> World::FindByTag(const std::string& tag) {
    std::vector<Entity> result;
    auto view = m_Registry.view<TagComponent>();
    for (auto entity : view) {
        if (view.get<TagComponent>(entity).Tag == tag) {
            result.emplace_back(entity, this);
        }
    }
    return result;
}

// 排序
void World::SortByDepth(const glm::vec3& cameraPosition) {
    // 按到相机的距离排序 TransformComponent
    m_Registry.sort<TransformComponent>(
        [&cameraPosition](const TransformComponent& a, const TransformComponent& b) {
            float distA = glm::distance(a.Position, cameraPosition);
            float distB = glm::distance(b.Position, cameraPosition);
            return distA > distB;  // 从远到近
        }
    );
}

} // namespace ECS
} // namespace GLRenderer
