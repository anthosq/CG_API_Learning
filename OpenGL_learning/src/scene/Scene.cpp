#include "Scene.h"
#include <iostream>
#include <algorithm>

namespace GLRenderer {

// ============================================================================
// Scene 实现
// ============================================================================

Scene::Scene(const std::string& name)
    : m_Name(name) {
    std::cout << "[Scene] 创建场景: " << name << std::endl;
}

Scene::~Scene() {
    std::cout << "[Scene] 销毁场景: " << m_Name
              << " (实体数: " << m_Entities.size() << ")" << std::endl;
    m_Entities.clear();
    m_RootEntities.clear();
}

// ============================================================================
// 实体管理
// ============================================================================

Entity::Ptr Scene::CreateEntity(const std::string& name) {
    auto entity = std::make_shared<Entity>(name);
    entity->m_Scene = this;
    m_Entities[entity->GetID()] = entity;
    m_RootsDirty = true;
    return entity;
}

void Scene::AddEntity(Entity::Ptr entity) {
    if (!entity) return;

    // 如果实体已在另一个场景中，先移除
    if (entity->m_Scene && entity->m_Scene != this) {
        entity->m_Scene->RemoveEntity(entity);
    }

    entity->m_Scene = this;
    m_Entities[entity->GetID()] = entity;
    m_RootsDirty = true;
}

void Scene::RemoveEntity(Entity::Ptr entity) {
    if (!entity) return;
    RemoveEntity(entity->GetID());
}

void Scene::RemoveEntity(uint32_t id) {
    auto it = m_Entities.find(id);
    if (it != m_Entities.end()) {
        it->second->m_Scene = nullptr;
        m_Entities.erase(it);
        m_RootsDirty = true;
    }
}

void Scene::DestroyEntity(Entity::Ptr entity) {
    if (!entity) return;

    // 递归销毁子实体
    auto children = entity->GetChildren();  // 拷贝列表，因为遍历时会修改
    for (auto& child : children) {
        DestroyEntity(child);
    }

    // 从父实体移除
    if (auto parent = entity->GetParent()) {
        parent->RemoveChild(entity);
    }

    // 从场景移除
    RemoveEntity(entity);
}

void Scene::DestroyEntity(uint32_t id) {
    auto entity = FindEntityByID(id);
    if (entity) {
        DestroyEntity(entity);
    }
}

// ============================================================================
// 实体查找
// ============================================================================

Entity::Ptr Scene::FindEntityByID(uint32_t id) const {
    auto it = m_Entities.find(id);
    if (it != m_Entities.end()) {
        return it->second;
    }
    return nullptr;
}

Entity::Ptr Scene::FindEntityByName(const std::string& name) const {
    for (const auto& [id, entity] : m_Entities) {
        if (entity->GetName() == name) {
            return entity;
        }
    }
    return nullptr;
}

std::vector<Entity::Ptr> Scene::FindEntitiesByName(const std::string& name) const {
    std::vector<Entity::Ptr> result;
    for (const auto& [id, entity] : m_Entities) {
        if (entity->GetName() == name) {
            result.push_back(entity);
        }
    }
    return result;
}

// ============================================================================
// 遍历
// ============================================================================

void Scene::ForEachEntity(const EntityCallback& callback) const {
    for (const auto& [id, entity] : m_Entities) {
        callback(entity);
    }
}

void Scene::ForEachRootEntity(const EntityCallback& callback) const {
    if (m_RootsDirty) {
        const_cast<Scene*>(this)->RefreshRootEntities();
    }
    for (const auto& entity : m_RootEntities) {
        callback(entity);
    }
}

void Scene::RefreshRootEntities() {
    m_RootEntities.clear();
    for (const auto& [id, entity] : m_Entities) {
        if (!entity->GetParent()) {
            m_RootEntities.push_back(entity);
        }
    }
    m_RootsDirty = false;
}

// ============================================================================
// 场景更新
// ============================================================================

void Scene::Update(float deltaTime) {
    // 只更新根实体（它们会递归更新子实体）
    ForEachRootEntity([deltaTime](Entity::Ptr entity) {
        entity->Update(deltaTime);
    });
}

// ============================================================================
// SceneManager 实现
// ============================================================================

SceneManager& SceneManager::Get() {
    static SceneManager instance;
    return instance;
}

Scene* SceneManager::CreateScene(const std::string& name) {
    // 检查是否已存在
    if (m_Scenes.find(name) != m_Scenes.end()) {
        std::cerr << "[SceneManager] 场景已存在: " << name << std::endl;
        return m_Scenes[name].get();
    }

    auto scene = std::make_unique<Scene>(name);
    Scene* ptr = scene.get();
    m_Scenes[name] = std::move(scene);

    // 如果没有活动场景，设置为活动场景
    if (!m_ActiveScene) {
        m_ActiveScene = ptr;
    }

    std::cout << "[SceneManager] 创建场景: " << name << std::endl;
    return ptr;
}

Scene* SceneManager::GetScene(const std::string& name) const {
    auto it = m_Scenes.find(name);
    if (it != m_Scenes.end()) {
        return it->second.get();
    }
    return nullptr;
}

void SceneManager::RemoveScene(const std::string& name) {
    auto it = m_Scenes.find(name);
    if (it != m_Scenes.end()) {
        // 如果是活动场景，清除引用
        if (m_ActiveScene == it->second.get()) {
            m_ActiveScene = nullptr;
        }
        m_Scenes.erase(it);
        std::cout << "[SceneManager] 移除场景: " << name << std::endl;
    }
}

void SceneManager::SetActiveScene(const std::string& name) {
    Scene* scene = GetScene(name);
    if (scene) {
        m_ActiveScene = scene;
        std::cout << "[SceneManager] 切换到场景: " << name << std::endl;
    } else {
        std::cerr << "[SceneManager] 场景不存在: " << name << std::endl;
    }
}

void SceneManager::SetActiveScene(Scene* scene) {
    m_ActiveScene = scene;
}

void SceneManager::Update(float deltaTime) {
    if (m_ActiveScene) {
        m_ActiveScene->Update(deltaTime);
    }
}

} // namespace GLRenderer
