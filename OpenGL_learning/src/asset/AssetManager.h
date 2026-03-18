#pragma once

// AssetManager.h - 集中式资产管理器
//
// AssetManager 负责：
// 1. 资产的导入和注册
// 2. 资产生命周期管理
// 3. 资产缓存和去重
// 4. 默认资产提供

#include "AssetTypes.h"
#include "utils/GLCommon.h"
#include "graphics/Texture.h"
#include "Mesh.h"

#include <unordered_map>
#include <memory>
#include <filesystem>
#include <iostream>

namespace GLRenderer {

class AssetManager : public NonCopyable {
public:
    static AssetManager& Get();

    // 生命周期
    void Initialize(const std::filesystem::path& assetDirectory);
    void Shutdown();

    // 资产导入（如果已存在则返回现有句柄）
    AssetHandle ImportTexture(const std::filesystem::path& path);
    AssetHandle ImportModel(const std::filesystem::path& path);

    // 资产访问
    Texture* GetTexture(AssetHandle handle);
    Model* GetModel(AssetHandle handle);

    // 资产查询
    const AssetMetadata* GetMetadata(AssetHandle handle) const;
    std::vector<AssetHandle> GetAssetsOfType(AssetType type) const;
    AssetHandle FindAssetByPath(const std::filesystem::path& path) const;
    bool IsLoaded(AssetHandle handle) const;

    // 默认资产
    AssetHandle GetDefaultTexture();
    AssetHandle GetWhiteTexture();
    AssetHandle GetBlackTexture();

    // 目录操作
    const std::filesystem::path& GetAssetDirectory() const { return m_AssetDirectory; }
    void ScanDirectory(const std::filesystem::path& directory);

    // 统计
    size_t GetTotalAssetCount() const { return m_Registry.size(); }
    size_t GetLoadedTextureCount() const { return m_Textures.size(); }
    size_t GetLoadedModelCount() const { return m_Models.size(); }

private:
    AssetManager() = default;
    ~AssetManager();

    void CreateDefaultAssets();

    // 资产注册表
    std::unordered_map<AssetHandle, AssetMetadata> m_Registry;

    // 已加载的资产
    std::unordered_map<AssetHandle, std::unique_ptr<Texture>> m_Textures;
    std::unordered_map<AssetHandle, std::unique_ptr<Model>> m_Models;

    // 路径到 Handle 的映射（用于去重）
    std::unordered_map<std::string, AssetHandle> m_PathToHandle;

    // 资产根目录
    std::filesystem::path m_AssetDirectory;

    // 默认资产句柄
    AssetHandle m_DefaultTexture;
    AssetHandle m_WhiteTexture;
    AssetHandle m_BlackTexture;

    bool m_Initialized = false;
};

} // namespace GLRenderer
