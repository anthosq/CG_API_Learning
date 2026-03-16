#pragma once

// ============================================================================
// AssetManager.h - 集中式资产管理器
// ============================================================================
//
// AssetManager 负责：
// 1. 资产的导入和注册
// 2. 资产生命周期管理
// 3. 资产缓存和去重
// 4. 默认资产提供
//

#include "AssetHandle.h"
#include "AssetTypes.h"
#include "utils/GLCommon.h"
#include "graphics/Texture.h"

#include "Mesh.h"

#include <unordered_map>
#include <memory>
#include <filesystem>
#include <iostream>

namespace GLRenderer {

// ============================================================================
// AssetManager
// ============================================================================

class AssetManager : public NonCopyable {
public:
    // 获取单例实例
    static AssetManager& Get();

    // ========================================================================
    // 生命周期
    // ========================================================================

    void Initialize(const std::filesystem::path& assetDirectory);
    void Shutdown();

    // ========================================================================
    // 资产导入
    // ========================================================================

    // 导入纹理（如果已存在则返回现有句柄）
    TextureHandle ImportTexture(const std::filesystem::path& path);

    // 导入模型
    ModelHandle ImportModel(const std::filesystem::path& path);

    // ========================================================================
    // 资产访问
    // ========================================================================

    // 获取纹理
    Texture* GetTexture(AssetID id);
    Texture* GetTexture(TextureHandle handle) { return GetTexture(handle.GetID()); }

    // 获取模型
    Model* GetModel(AssetID id);
    Model* GetModel(ModelHandle handle) { return GetModel(handle.GetID()); }

    // ========================================================================
    // 资产查询
    // ========================================================================

    // 获取资产元数据
    const AssetMetadata* GetMetadata(AssetID id) const;

    // 获取指定类型的所有资产 ID
    std::vector<AssetID> GetAssetsOfType(AssetType type) const;

    // 通过路径查找资产
    AssetID FindAssetByPath(const std::filesystem::path& path) const;

    // 检查资产是否已加载
    bool IsLoaded(AssetID id) const;

    // ========================================================================
    // 默认资产
    // ========================================================================

    TextureHandle GetDefaultTexture();
    TextureHandle GetWhiteTexture();
    TextureHandle GetBlackTexture();

    // ========================================================================
    // 目录操作
    // ========================================================================

    const std::filesystem::path& GetAssetDirectory() const { return m_AssetDirectory; }

    // 扫描目录并注册资产（不立即加载）
    void ScanDirectory(const std::filesystem::path& directory);

    // ========================================================================
    // 统计
    // ========================================================================

    size_t GetTotalAssetCount() const { return m_Registry.size(); }
    size_t GetLoadedTextureCount() const { return m_Textures.size(); }
    size_t GetLoadedModelCount() const { return m_Models.size(); }

private:
    AssetManager() = default;
    ~AssetManager();

    // 生成新的资产 ID
    AssetID GenerateAssetID();

    // 创建默认资产
    void CreateDefaultAssets();

    // 资产注册表
    std::unordered_map<AssetID, AssetMetadata> m_Registry;

    // 已加载的资产
    std::unordered_map<AssetID, std::unique_ptr<Texture>> m_Textures;
    std::unordered_map<AssetID, std::unique_ptr<Model>> m_Models;

    // 路径到 ID 的映射（用于去重）
    std::unordered_map<std::string, AssetID> m_PathToID;

    // 资产根目录
    std::filesystem::path m_AssetDirectory;

    // ID 计数器
    AssetID m_NextID = 1;

    // 默认资产句柄
    TextureHandle m_DefaultTexture;
    TextureHandle m_WhiteTexture;
    TextureHandle m_BlackTexture;

    bool m_Initialized = false;
};

// ============================================================================
// AssetHandle 模板实现
// ============================================================================

template<>
inline Texture* AssetHandle<Texture>::Get() const {
    return AssetManager::Get().GetTexture(m_ID);
}

template<>
inline Texture* AssetHandle<Texture>::TryGet() const {
    if (!IsValid()) return nullptr;
    return AssetManager::Get().GetTexture(m_ID);
}

template<>
inline Model* AssetHandle<Model>::Get() const {
    return AssetManager::Get().GetModel(m_ID);
}

template<>
inline Model* AssetHandle<Model>::TryGet() const {
    if (!IsValid()) return nullptr;
    return AssetManager::Get().GetModel(m_ID);
}

} // namespace GLRenderer
