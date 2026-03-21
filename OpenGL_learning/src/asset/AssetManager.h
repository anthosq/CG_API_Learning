#pragma once

// AssetManager.h - 集中式资产管理器 (参考 Hazel)
//
// 设计要点：
// 1. 统一存储所有 Asset 派生类 (Ref<Asset>)
// 2. 模板方法 GetAsset<T> / AddMemoryOnlyAsset<T>
// 3. Asset 自带 Handle，注册时检查或生成
//
// TODO: 创建 MeshImporter (使用 Assimp)
//   - 导入 FBX/OBJ/glTF 生成 MeshSource + Submesh
//   - 提取嵌入材质和纹理引用

#include "Asset.h"
#include "AssetTypes.h"
#include "utils/GLCommon.h"
#include "core/Ref.h"

#include <unordered_map>
#include <filesystem>
#include <iostream>

namespace GLRenderer {

// 前向声明
class Texture2D;
class StaticMesh;
class MeshSource;

class AssetManager : public NonCopyable {
public:
    static AssetManager& Get();

    // 生命周期
    void Initialize(const std::filesystem::path& assetDirectory);
    void Shutdown();

    // === Hazel 风格 API ===

    // 获取资产 (模板方法)
    template<typename T>
    Ref<T> GetAsset(AssetHandle handle) {
        if (!handle.IsValid()) return nullptr;
        auto it = m_LoadedAssets.find(handle);
        if (it != m_LoadedAssets.end()) {
            return it->second.As<T>();
        }
        return nullptr;
    }

    // 注册内存资产 (模板方法)
    template<typename T>
    AssetHandle AddMemoryOnlyAsset(Ref<T> asset) {
        static_assert(std::is_base_of_v<Asset, T>, "T must inherit from Asset");
        if (!asset->Handle.IsValid()) {
            asset->Handle = AssetHandle();  // 生成新 Handle
        }

        AssetMetadata metadata;
        metadata.Handle = asset->Handle;
        metadata.Type = T::GetStaticType();
        metadata.IsLoaded = true;
        metadata.IsMemoryOnly = true;

        m_Registry[asset->Handle] = metadata;
        m_LoadedAssets[asset->Handle] = asset;

        return asset->Handle;
    }

    // === 文件导入 API ===
    AssetHandle ImportTexture(const std::filesystem::path& path);
    AssetHandle ImportMeshSource(const std::filesystem::path& path);

    // === 资产查询 ===
    bool IsAssetHandleValid(AssetHandle handle) const;
    bool IsAssetLoaded(AssetHandle handle) const;
    bool IsMemoryAsset(AssetHandle handle) const;
    AssetType GetAssetType(AssetHandle handle) const;
    const AssetMetadata* GetMetadata(AssetHandle handle) const;
    std::vector<AssetHandle> GetAllAssetsWithType(AssetType type) const;
    AssetHandle FindAssetByPath(const std::filesystem::path& path) const;

    // === 目录操作 ===
    const std::filesystem::path& GetAssetDirectory() const { return m_AssetDirectory; }
    void ScanDirectory(const std::filesystem::path& directory);

    // === 统计 ===
    size_t GetTotalAssetCount() const { return m_Registry.size(); }
    size_t GetLoadedAssetCount() const { return m_LoadedAssets.size(); }

private:
    AssetManager() = default;
    ~AssetManager();

    // 资产注册表 (元数据)
    std::unordered_map<AssetHandle, AssetMetadata> m_Registry;

    // 已加载的资产 (统一存储)
    std::unordered_map<AssetHandle, Ref<Asset>> m_LoadedAssets;

    // 路径到 Handle 的映射（用于去重）
    std::unordered_map<std::string, AssetHandle> m_PathToHandle;

    // 资产根目录
    std::filesystem::path m_AssetDirectory;

    bool m_Initialized = false;
};

} // namespace GLRenderer
