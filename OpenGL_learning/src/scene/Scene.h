#pragma once

#include "Entity.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>

namespace GLRenderer {

// ============================================================================
// Scene - 场景类
// ============================================================================
// 管理一组实体，提供实体的创建、查找、遍历功能。
//
// 设计思路：
// 1. Scene 拥有所有 Entity 的所有权（通过 shared_ptr）
// 2. 提供按名称和 ID 查找实体的能力
// 3. 维护场景树的根节点列表
// 4. 提供遍历场景中所有实体的接口
//
// 使用示例：
//   Scene scene("MainScene");
//   auto cube = scene.CreateEntity("Cube");
//   cube->GetTransform().SetPosition(0, 1, 0);
//   scene.Update(deltaTime);
// ============================================================================

class Scene {
public:
    using EntityCallback = std::function<void(Entity::Ptr)>;

    Scene(const std::string& name = "Scene");
    ~Scene();

    // 禁止拷贝
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // ========================================================================
    // 基本属性
    // ========================================================================

    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    // ========================================================================
    // 实体管理
    // ========================================================================

    // 创建新实体
    Entity::Ptr CreateEntity(const std::string& name = "Entity");

    // 添加已有实体到场景
    void AddEntity(Entity::Ptr entity);

    // 从场景移除实体
    void RemoveEntity(Entity::Ptr entity);
    void RemoveEntity(uint32_t id);

    // 销毁实体（移除并删除）
    void DestroyEntity(Entity::Ptr entity);
    void DestroyEntity(uint32_t id);

    // ========================================================================
    // 实体查找
    // ========================================================================

    // 按 ID 查找
    Entity::Ptr FindEntityByID(uint32_t id) const;

    // 按名称查找（返回第一个匹配的）
    Entity::Ptr FindEntityByName(const std::string& name) const;

    // 按名称查找所有匹配的
    std::vector<Entity::Ptr> FindEntitiesByName(const std::string& name) const;

    // 查找具有特定组件的所有实体
    template<typename T>
    std::vector<Entity::Ptr> FindEntitiesWithComponent() const {
        std::vector<Entity::Ptr> result;
        for (const auto& [id, entity] : m_Entities) {
            if (entity->HasComponent<T>()) {
                result.push_back(entity);
            }
        }
        return result;
    }

    // ========================================================================
    // 遍历
    // ========================================================================

    // 遍历所有实体
    void ForEachEntity(const EntityCallback& callback) const;

    // 遍历所有根实体（没有父节点的实体）
    void ForEachRootEntity(const EntityCallback& callback) const;

    // 获取所有实体
    const std::unordered_map<uint32_t, Entity::Ptr>& GetAllEntities() const { return m_Entities; }

    // 获取根实体列表
    const std::vector<Entity::Ptr>& GetRootEntities() const { return m_RootEntities; }

    // ========================================================================
    // 场景更新
    // ========================================================================

    void Update(float deltaTime);

    // ========================================================================
    // 统计信息
    // ========================================================================

    size_t GetEntityCount() const { return m_Entities.size(); }

private:
    void RefreshRootEntities();

    std::string m_Name;
    std::unordered_map<uint32_t, Entity::Ptr> m_Entities;
    std::vector<Entity::Ptr> m_RootEntities;  // 缓存根实体
    bool m_RootsDirty = true;
};

// ============================================================================
// SceneManager - 场景管理器
// ============================================================================
// 单例模式，管理多个场景，控制当前活动场景。
//
// 设计思路：
// 1. 支持多场景（如主场景、UI场景、Loading场景）
// 2. 提供场景切换接口
// 3. 统一场景更新入口
//
// 使用示例：
//   SceneManager::Get().CreateScene("MainLevel");
//   SceneManager::Get().SetActiveScene("MainLevel");
//   SceneManager::Get().Update(deltaTime);
// ============================================================================

class SceneManager {
public:
    static SceneManager& Get();

    // 禁止拷贝
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // ========================================================================
    // 场景管理
    // ========================================================================

    // 创建新场景
    Scene* CreateScene(const std::string& name);

    // 获取场景
    Scene* GetScene(const std::string& name) const;

    // 移除场景
    void RemoveScene(const std::string& name);

    // ========================================================================
    // 活动场景
    // ========================================================================

    Scene* GetActiveScene() const { return m_ActiveScene; }
    void SetActiveScene(const std::string& name);
    void SetActiveScene(Scene* scene);

    // ========================================================================
    // 更新
    // ========================================================================

    void Update(float deltaTime);

private:
    SceneManager() = default;
    ~SceneManager() = default;

    std::unordered_map<std::string, std::unique_ptr<Scene>> m_Scenes;
    Scene* m_ActiveScene = nullptr;
};

} // namespace GLRenderer

using GLRenderer::Scene;
using GLRenderer::SceneManager;
