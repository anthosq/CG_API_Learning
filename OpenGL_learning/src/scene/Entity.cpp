#include "Entity.h"
#include <algorithm>
#include <iostream>

namespace GLRenderer {

// 静态成员初始化
uint32_t Entity::s_NextID = 1;

// ============================================================================
// 构造与析构
// ============================================================================

Entity::Entity(const std::string& name)
    : m_ID(s_NextID++)
    , m_Name(name) {
}

Entity::~Entity() {
    // 移除所有组件（调用 OnDetach）
    for (auto& [typeIndex, component] : m_Components) {
        component->OnDetach();
    }
    m_Components.clear();

    // 清理子节点引用
    for (auto& child : m_Children) {
        if (child) {
            child->m_Parent.reset();
        }
    }
    m_Children.clear();
}

// ============================================================================
// 父子层级
// ============================================================================

void Entity::SetParent(Entity::Ptr parent) {
    // 获取当前 shared_ptr
    Entity::Ptr self = shared_from_this();

    // 从旧父节点移除
    if (auto oldParent = m_Parent.lock()) {
        oldParent->RemoveChild(self);
    }

    // 设置新父节点
    m_Parent = parent;

    // 添加到新父节点
    if (parent) {
        parent->AddChild(self);
    }

    // 同步 Transform 的父子关系
    if (parent) {
        m_Transform.SetParent(&parent->m_Transform);
    } else {
        m_Transform.SetParent(nullptr);
    }
}

void Entity::AddChild(Entity::Ptr child) {
    if (!child) return;

    // 检查是否已是子节点
    auto it = std::find(m_Children.begin(), m_Children.end(), child);
    if (it != m_Children.end()) {
        return;  // 已经是子节点
    }

    m_Children.push_back(child);
}

void Entity::RemoveChild(Entity::Ptr child) {
    if (!child) return;

    auto it = std::find(m_Children.begin(), m_Children.end(), child);
    if (it != m_Children.end()) {
        m_Children.erase(it);
    }
}

// ============================================================================
// 更新
// ============================================================================

void Entity::Update(float deltaTime) {
    if (!m_Active) return;

    // 更新所有启用的组件
    for (auto& [typeIndex, component] : m_Components) {
        if (component->IsEnabled()) {
            component->OnUpdate(deltaTime);
        }
    }

    // 递归更新子节点
    for (auto& child : m_Children) {
        if (child) {
            child->Update(deltaTime);
        }
    }
}

} // namespace GLRenderer
