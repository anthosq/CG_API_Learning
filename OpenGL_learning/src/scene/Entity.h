#pragma once

#include "Transform.h"
#include <string>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <functional>

namespace GLRenderer {

// 前向声明
class Scene;

// ============================================================================
// Component - 组件基类
// ============================================================================
// 所有可附加到 Entity 的组件都应继承此类。
//
// 设计思路：
// 1. 组件是纯数据或行为的载体
// 2. 每种组件类型在 Entity 上只能有一个实例
// 3. 组件通过 Entity 访问其他组件和 Transform
// ============================================================================

class Entity;  // 前向声明

class Component {
public:
    virtual ~Component() = default;

    // 生命周期方法（子类可选重写）
    virtual void OnAttach() {}      // 附加到 Entity 时调用
    virtual void OnDetach() {}      // 从 Entity 移除时调用
    virtual void OnUpdate(float deltaTime) {}  // 每帧更新

    // 获取所属 Entity
    Entity* GetEntity() const { return m_Entity; }

    // 获取 Transform（便捷方法）
    Transform& GetTransform() const;

    // 启用/禁用
    bool IsEnabled() const { return m_Enabled; }
    void SetEnabled(bool enabled) { m_Enabled = enabled; }

protected:
    friend class Entity;
    Entity* m_Entity = nullptr;
    bool m_Enabled = true;
};

// ============================================================================
// Entity - 场景实体
// ============================================================================
// 场景中的基本对象，由 Transform 和多个 Component 组成。
//
// 设计思路：
// 1. 每个 Entity 都有一个 Transform（位置、旋转、缩放）
// 2. 可以附加任意类型的组件
// 3. 支持父子层级（通过 Transform 实现）
// 4. 使用类型索引管理组件，保证类型安全
//
// 使用示例：
//   auto entity = std::make_shared<Entity>("MyEntity");
//   entity->GetTransform().SetPosition(1, 2, 3);
//   entity->AddComponent<MeshRenderer>(mesh, material);
//   auto renderer = entity->GetComponent<MeshRenderer>();
// ============================================================================

class Entity : public std::enable_shared_from_this<Entity> {
public:
    using Ptr = std::shared_ptr<Entity>;
    using WeakPtr = std::weak_ptr<Entity>;

    Entity(const std::string& name = "Entity");
    ~Entity();

    // 禁止拷贝
    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;

    // ========================================================================
    // 基本属性
    // ========================================================================

    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    uint32_t GetID() const { return m_ID; }

    bool IsActive() const { return m_Active; }
    void SetActive(bool active) { m_Active = active; }

    // ========================================================================
    // Transform 访问
    // ========================================================================

    Transform& GetTransform() { return m_Transform; }
    const Transform& GetTransform() const { return m_Transform; }

    // ========================================================================
    // 组件操作
    // ========================================================================

    // 添加组件
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

        std::type_index typeIndex(typeid(T));

        // 检查是否已存在
        if (m_Components.find(typeIndex) != m_Components.end()) {
            std::cerr << "[Entity] 组件已存在: " << typeid(T).name() << std::endl;
            return nullptr;
        }

        // 创建并添加组件
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = component.get();
        ptr->m_Entity = this;
        m_Components[typeIndex] = std::move(component);

        // 调用生命周期方法
        ptr->OnAttach();

        return ptr;
    }

    // 获取组件
    template<typename T>
    T* GetComponent() {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

        std::type_index typeIndex(typeid(T));
        auto it = m_Components.find(typeIndex);
        if (it != m_Components.end()) {
            return static_cast<T*>(it->second.get());
        }
        return nullptr;
    }

    template<typename T>
    const T* GetComponent() const {
        return const_cast<Entity*>(this)->GetComponent<T>();
    }

    // 检查是否有组件
    template<typename T>
    bool HasComponent() const {
        return GetComponent<T>() != nullptr;
    }

    // 移除组件
    template<typename T>
    void RemoveComponent() {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

        std::type_index typeIndex(typeid(T));
        auto it = m_Components.find(typeIndex);
        if (it != m_Components.end()) {
            it->second->OnDetach();
            m_Components.erase(it);
        }
    }

    // ========================================================================
    // 父子层级
    // ========================================================================

    void SetParent(Entity::Ptr parent);
    Entity::Ptr GetParent() const { return m_Parent.lock(); }

    const std::vector<Entity::Ptr>& GetChildren() const { return m_Children; }
    void AddChild(Entity::Ptr child);
    void RemoveChild(Entity::Ptr child);

    // ========================================================================
    // 更新
    // ========================================================================

    void Update(float deltaTime);

    // ========================================================================
    // Scene 关联
    // ========================================================================

    Scene* GetScene() const { return m_Scene; }

private:
    friend class Scene;

    static uint32_t s_NextID;

    uint32_t m_ID;
    std::string m_Name;
    bool m_Active = true;

    Transform m_Transform;

    std::unordered_map<std::type_index, std::unique_ptr<Component>> m_Components;

    Entity::WeakPtr m_Parent;
    std::vector<Entity::Ptr> m_Children;

    Scene* m_Scene = nullptr;
};

// ============================================================================
// 内联实现
// ============================================================================

inline Transform& Component::GetTransform() const {
    return m_Entity->GetTransform();
}

} // namespace GLRenderer

using GLRenderer::Entity;
using GLRenderer::Component;
