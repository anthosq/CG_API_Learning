#include "AssetManager.h"
#include "graphics/Texture.h"
#include "graphics/MeshSource.h"
#include "AssimpMeshImporter.h"

namespace GLRenderer {

AssetManager& AssetManager::Get() {
    static AssetManager instance;
    return instance;
}

AssetManager::~AssetManager() {
    if (m_Initialized) {
        Shutdown();
    }
}

void AssetManager::Initialize(const std::filesystem::path& assetDirectory) {
    if (m_Initialized) {
        std::cout << "[AssetManager] Already initialized" << std::endl;
        return;
    }

    m_AssetDirectory = assetDirectory;
    std::cout << "[AssetManager] Initializing with root: " << assetDirectory << std::endl;

    m_Initialized = true;
    std::cout << "[AssetManager] Initialized successfully" << std::endl;
}

void AssetManager::Shutdown() {
    std::cout << "[AssetManager] Shutting down..." << std::endl;

    m_LoadedAssets.clear();
    m_Registry.clear();
    m_PathToHandle.clear();

    m_Initialized = false;

    std::cout << "[AssetManager] Shutdown complete" << std::endl;
}

AssetHandle AssetManager::ImportTexture(const std::filesystem::path& path,
                                         const TextureSpec& spec) {
    // 缓存键：路径 + sRGB 标志，同一文件作为颜色贴图和数据贴图时分开缓存
    std::string pathStr = path.string() + (spec.SRGB ? ":srgb" : "");

    // 检查是否已导入
    auto it = m_PathToHandle.find(pathStr);
    if (it != m_PathToHandle.end()) {
        return it->second;
    }

    if (!std::filesystem::exists(path)) {
        std::cerr << "[AssetManager] Texture file not found: " << path << std::endl;
        return AssetHandle(0);
    }

    try {
        auto texture = Texture2D::Create(path, spec);
        texture->Handle = AssetHandle();  // 生成新 Handle

        AssetMetadata metadata;
        metadata.Handle = texture->Handle;
        metadata.Type = AssetType::Texture;
        metadata.SourcePath = path;
        metadata.Name = path.filename().string();
        metadata.IsLoaded = true;

        m_Registry[texture->Handle] = metadata;
        m_LoadedAssets[texture->Handle] = texture;
        m_PathToHandle[pathStr] = texture->Handle;

        std::cout << "[AssetManager] Imported texture: " << metadata.Name
                  << " (Handle: " << static_cast<uint64_t>(texture->Handle) << ")" << std::endl;

        return texture->Handle;
    } catch (const std::exception& e) {
        std::cerr << "[AssetManager] Failed to load texture: " << e.what() << std::endl;
        return AssetHandle(0);
    }
}

AssetHandle AssetManager::ImportMeshSource(const std::filesystem::path& path) {
    std::string pathStr = path.string();

    // 检查是否已导入
    auto it = m_PathToHandle.find(pathStr);
    if (it != m_PathToHandle.end()) {
        return it->second;
    }

    if (!std::filesystem::exists(path)) {
        std::cerr << "[AssetManager] Mesh file not found: " << path << std::endl;
        return AssetHandle(0);
    }

    try {
        AssimpMeshImporter importer(path);
        auto meshSource = importer.ImportToMeshSource();

        if (!meshSource) {
            std::cerr << "[AssetManager] Failed to import mesh: " << path << std::endl;
            return AssetHandle(0);
        }

        // TODO: Handle 应在 Asset 构造时生成，这里是临时实现
        meshSource->Handle = AssetHandle();

        AssetMetadata metadata;
        metadata.Handle = meshSource->Handle;
        metadata.Type = AssetType::MeshSource;
        metadata.SourcePath = path;
        metadata.Name = path.filename().string();
        metadata.IsLoaded = true;

        m_Registry[meshSource->Handle] = metadata;
        m_LoadedAssets[meshSource->Handle] = meshSource;
        m_PathToHandle[pathStr] = meshSource->Handle;

        std::cout << "[AssetManager] Imported mesh: " << metadata.Name
                  << " (Handle: " << static_cast<uint64_t>(meshSource->Handle) << ")" << std::endl;

        return meshSource->Handle;
    } catch (const std::exception& e) {
        std::cerr << "[AssetManager] Failed to load mesh: " << e.what() << std::endl;
        return AssetHandle(0);
    }
}

bool AssetManager::IsAssetHandleValid(AssetHandle handle) const {
    return handle.IsValid() && m_Registry.find(handle) != m_Registry.end();
}

bool AssetManager::IsAssetLoaded(AssetHandle handle) const {
    return m_LoadedAssets.find(handle) != m_LoadedAssets.end();
}

bool AssetManager::IsMemoryAsset(AssetHandle handle) const {
    auto it = m_Registry.find(handle);
    if (it != m_Registry.end()) {
        return it->second.IsMemoryOnly;
    }
    return false;
}

AssetType AssetManager::GetAssetType(AssetHandle handle) const {
    auto it = m_Registry.find(handle);
    if (it != m_Registry.end()) {
        return it->second.Type;
    }
    return AssetType::Unknown;
}

const AssetMetadata* AssetManager::GetMetadata(AssetHandle handle) const {
    auto it = m_Registry.find(handle);
    if (it != m_Registry.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<AssetHandle> AssetManager::GetAllAssetsWithType(AssetType type) const {
    std::vector<AssetHandle> result;
    for (const auto& [handle, metadata] : m_Registry) {
        if (metadata.Type == type) {
            result.push_back(handle);
        }
    }
    return result;
}

AssetHandle AssetManager::FindAssetByPath(const std::filesystem::path& path) const {
    auto it = m_PathToHandle.find(path.string());
    if (it != m_PathToHandle.end()) {
        return it->second;
    }
    return AssetHandle(0);
}

void AssetManager::ScanDirectory(const std::filesystem::path& directory) {
    if (!std::filesystem::exists(directory)) {
        std::cerr << "[AssetManager] Directory not found: " << directory << std::endl;
        return;
    }

    int count = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file()) {
            AssetType type = GetAssetTypeFromExtension(entry.path());
            if (type != AssetType::Unknown) {
                std::string pathStr = entry.path().string();

                if (m_PathToHandle.find(pathStr) != m_PathToHandle.end()) {
                    continue;
                }

                AssetHandle handle;

                AssetMetadata metadata;
                metadata.Handle = handle;
                metadata.Type = type;
                metadata.SourcePath = entry.path();
                metadata.Name = entry.path().filename().string();
                metadata.IsLoaded = false;

                m_Registry[handle] = metadata;
                m_PathToHandle[pathStr] = handle;
                count++;
            }
        }
    }

    std::cout << "[AssetManager] Scanned directory, found " << count << " new assets" << std::endl;
}

} // namespace GLRenderer
