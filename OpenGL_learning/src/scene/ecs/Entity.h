#pragma once

// Entity.h - EnTT Entity 包装器
// 提供面向对象风格的实体操作接口，封装 EnTT 的底层 API。
// 这是一个轻量级句柄类，可以安全地按值传递。
//
// 使用示例：
//   Entity entity = world.CreateEntity("Player");
//   entity.AddComponent<TransformComponent>(glm::vec3(0, 1, 0));
//   entity.AddComponent<MeshComponent>(mesh);
//
//   if (entity.HasComponent<MeshComponent>()) {
//       auto& mesh = entity.GetComponent<MeshComponent>();
//   }

#include <entt/entt.hpp>
#include <string>
#include <cassert>

namespace GLRenderer {
namespace ECS {

// 前向声明
class World;

// Entity - 实体句柄

class Entity {
public:
    Entity() = default;
    Entity(entt::entity handle, World* world)
        : m_Handle(handle), m_World(world) {}

    Entity(const Entity&) = default;
    Entity& operator=(const Entity&) = default;

    // 组件操作

    /// 添加组件
    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        assert(!HasComponent<T>() && "Entity already has this component!");
        return GetRegistry().emplace<T>(m_Handle, std::forward<Args>(args)...);
    }

    /// 添加或替换组件
    template<typename T, typename... Args>
    T& AddOrReplaceComponent(Args&&... args) {
        return GetRegistry().emplace_or_replace<T>(m_Handle, std::forward<Args>(args)...);
    }

    /// 获取组件引用
    template<typename T>
    T& GetComponent() {
        assert(HasComponent<T>() && "Entity does not have this component!");
        return GetRegistry().get<T>(m_Handle);
    }

    /// 获取组件引用 (const)
    template<typename T>
    const T& GetComponent() const {
        assert(HasComponent<T>() && "Entity does not have this component!");
        return GetRegistry().get<T>(m_Handle);
    }

    /// 尝试获取组件（可能返回 nullptr）
    template<typename T>
    T* TryGetComponent() {
        return GetRegistry().try_get<T>(m_Handle);
    }

    template<typename T>
    const T* TryGetComponent() const {
        return GetRegistry().try_get<T>(m_Handle);
    }

    /// 检查是否拥有组件
    template<typename T>
    bool HasComponent() const {
        return GetRegistry().all_of<T>(m_Handle);
    }

    /// 检查是否拥有所有指定组件
    template<typename... T>
    bool HasAllComponents() const {
        return GetRegistry().all_of<T...>(m_Handle);
    }

    /// 检查是否拥有任意指定组件
    template<typename... T>
    bool HasAnyComponent() const {
        return GetRegistry().any_of<T...>(m_Handle);
    }

    /// 移除组件
    template<typename T>
    void RemoveComponent() {
        assert(HasComponent<T>() && "Entity does not have this component!");
        GetRegistry().remove<T>(m_Handle);
    }

    /// 安全移除组件（如果存在）
    template<typename T>
    bool TryRemoveComponent() {
        if (HasComponent<T>()) {
            GetRegistry().remove<T>(m_Handle);
            return true;
        }
        return false;
    }

    // 实体状态

    /// 检查实体是否有效
    bool IsValid() const;

    /// 获取原始句柄
    entt::entity GetHandle() const { return m_Handle; }

    /// 获取实体 ID（用于调试）
    uint32_t GetID() const { return static_cast<uint32_t>(m_Handle); }

    /// 获取所属的 World
    World* GetWorld() const { return m_World; }

    // 运算符重载
    /// 隐式转换为 entt::entity
    operator entt::entity() const { return m_Handle; }

    /// 隐式转换为 bool（检查有效性）
    operator bool() const { return IsValid(); }

    /// 相等比较
    bool operator==(const Entity& other) const {
        return m_Handle == other.m_Handle && m_World == other.m_World;
    }

    bool operator!=(const Entity& other) const {
        return !(*this == other);
    }

    /// 用于容器排序
    bool operator<(const Entity& other) const {
        return m_Handle < other.m_Handle;
    }

private:
    entt::registry& GetRegistry();
    const entt::registry& GetRegistry() const;

    entt::entity m_Handle = entt::null;
    World* m_World = nullptr;
};

} // namespace ECS
} // namespace GLRenderer
