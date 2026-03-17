#pragma once

// World.h - ECS 世界（Registry 包装器）
// World 是 EnTT Registry 的包装器，管理所有实体和组件。
// 提供实体创建、查询、遍历等高级接口。
//
// 设计原则：
// 1. 每个 World 是独立的，可以有多个 World（如游戏世界、UI世界）
// 2. 实体只在其所属的 World 中有效
// 3. 使用 View 进行高效的组件查询
//
// 使用示例：
//   World world;
//
//   // 创建实体
//   Entity player = world.CreateEntity("Player");
//   player.AddComponent<TransformComponent>();
//
//   // 查询组件
//   world.Each<TransformComponent, MeshComponent>(
//       [](Entity entity, TransformComponent& t, MeshComponent& m) {
//           // 处理所有同时拥有 Transform 和 Mesh 的实体
//       });

#include "Entity.h"
#include "Components.h"
#include "core/UUID.h"
#include <entt/entt.hpp>
#include <string>
#include <functional>

namespace GLRenderer {
namespace ECS {

// World - ECS 世界
class World {
public:
    World(const std::string& name = "World");
    ~World();

    // 禁止拷贝
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // 支持移动
    World(World&&) = default;
    World& operator=(World&&) = default;

    // 实体管理
    /// 创建新实体（自动生成 UUID）
    Entity CreateEntity(const std::string& name = "Entity");

    /// 创建带标签的实体（自动生成 UUID）
    Entity CreateEntity(const std::string& name, const std::string& tag);

    /// 使用指定 UUID 创建实体（用于反序列化）
    Entity CreateEntityWithUUID(UUID uuid, const std::string& name);

    /// 销毁实体
    void DestroyEntity(Entity entity);

    /// 销毁所有实体
    void Clear();

    /// 检查实体是否有效
    bool IsValid(Entity entity) const;
    bool IsValid(entt::entity handle) const;

    // 实体查找
    /// 按名称查找实体（返回第一个匹配的）
    Entity FindByName(const std::string& name);

    /// 按标签查找所有实体
    std::vector<Entity> FindByTag(const std::string& tag);

    /// 查找具有特定组件的所有实体
    template<typename... Components>
    std::vector<Entity> FindWithComponents() {
        std::vector<Entity> result;
        auto view = m_Registry.view<Components...>();
        for (auto entity : view) {
            result.emplace_back(entity, this);
        }
        return result;
    }

    // 组件查询 (View API)
    /// 获取组件视图
    /// 用于高效遍历拥有特定组件的实体
    template<typename... Components>
    auto View() {
        return m_Registry.view<Components...>();
    }

    template<typename... Components>
    auto View() const {
        return m_Registry.view<Components...>();
    }

    /// 遍历拥有特定组件的实体
    /// callback 签名: void(Entity, Component&...)
    template<typename... Components, typename Func>
    void Each(Func&& callback) {
        auto view = m_Registry.view<Components...>();
        for (auto entity : view) {
            callback(Entity(entity, this), view.template get<Components>(entity)...);
        }
    }

    /// 遍历拥有特定组件的实体 (const 版本)
    template<typename... Components, typename Func>
    void Each(Func&& callback) const {
        auto view = m_Registry.view<Components...>();
        for (auto entity : view) {
            callback(Entity(entity, const_cast<World*>(this)),
                     view.template get<Components>(entity)...);
        }
    }

    // 组件组 (Group API)
    /// 获取组件组
    /// Group 比 View 更快，但需要预先定义拥有关系
    template<typename... Owned, typename... Get, typename... Exclude>
    auto Group(entt::get_t<Get...> = {}, entt::exclude_t<Exclude...> = {}) {
        return m_Registry.group<Owned...>(entt::get<Get...>, entt::exclude<Exclude...>);
    }

    // 排序
    /// 按组件排序实体（用于渲染排序等）
    template<typename Component, typename Compare>
    void Sort(Compare&& compare) {
        m_Registry.sort<Component>(std::forward<Compare>(compare));
    }

    /// 按深度排序（用于透明物体渲染）
    void SortByDepth(const glm::vec3& cameraPosition);

    // Registry 访问
    /// 获取底层 Registry（高级用法）
    entt::registry& GetRegistry() { return m_Registry; }
    const entt::registry& GetRegistry() const { return m_Registry; }

    // 统计信息
    /// 获取实体数量
    size_t GetEntityCount() const { return m_Registry.storage<entt::entity>()->size(); }

    /// 获取特定组件的数量
    template<typename T>
    size_t GetComponentCount() const {
        return m_Registry.view<T>().size();
    }

    /// 获取世界名称
    const std::string& GetName() const { return m_Name; }

    // 信号/事件（可选，用于监听组件变化）
    /// 注册组件添加回调
    template<typename Component>
    void OnComponentAdded(std::function<void(Entity, Component&)> callback) {
        m_Registry.on_construct<Component>().connect<&World::OnComponentConstruct<Component>>(this);
        m_OnComponentAdded<Component> = std::move(callback);
    }

    /// 注册组件移除回调
    template<typename Component>
    void OnComponentRemoved(std::function<void(Entity, Component&)> callback) {
        m_Registry.on_destroy<Component>().connect<&World::OnComponentDestroy<Component>>(this);
        m_OnComponentRemoved<Component> = std::move(callback);
    }

private:
    template<typename Component>
    void OnComponentConstruct(entt::registry& registry, entt::entity entity) {
        if (m_OnComponentAdded<Component>) {
            m_OnComponentAdded<Component>(Entity(entity, this), registry.get<Component>(entity));
        }
    }

    template<typename Component>
    void OnComponentDestroy(entt::registry& registry, entt::entity entity) {
        if (m_OnComponentRemoved<Component>) {
            m_OnComponentRemoved<Component>(Entity(entity, this), registry.get<Component>(entity));
        }
    }

    std::string m_Name;
    entt::registry m_Registry;

    // 组件事件回调存储
    template<typename T>
    static inline std::function<void(Entity, T&)> m_OnComponentAdded;
    template<typename T>
    static inline std::function<void(Entity, T&)> m_OnComponentRemoved;
};

// Entity 内联实现
inline bool Entity::IsValid() const {
    return m_World != nullptr && m_World->IsValid(m_Handle);
}

inline entt::registry& Entity::GetRegistry() {
    return m_World->GetRegistry();
}

inline const entt::registry& Entity::GetRegistry() const {
    return m_World->GetRegistry();
}

} // namespace ECS
} // namespace GLRenderer
