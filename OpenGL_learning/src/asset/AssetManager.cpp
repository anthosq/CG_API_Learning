#include "AssetManager.h"
#include "Mesh.h"

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

    CreateDefaultAssets();

    m_Initialized = true;
    std::cout << "[AssetManager] Initialized successfully" << std::endl;
}

void AssetManager::Shutdown() {
    std::cout << "[AssetManager] Shutting down..." << std::endl;

    m_Textures.clear();
    m_Models.clear();
    m_Registry.clear();
    m_PathToHandle.clear();

    m_DefaultTexture = AssetHandle();
    m_WhiteTexture = AssetHandle();
    m_BlackTexture = AssetHandle();

    m_Initialized = false;

    std::cout << "[AssetManager] Shutdown complete" << std::endl;
}

AssetHandle AssetManager::ImportTexture(const std::filesystem::path& path) {
    std::string pathStr = path.string();

    auto it = m_PathToHandle.find(pathStr);
    if (it != m_PathToHandle.end()) {
        return it->second;
    }

    if (!std::filesystem::exists(path)) {
        std::cerr << "[AssetManager] Texture file not found: " << path << std::endl;
        return GetDefaultTexture();
    }

    AssetHandle handle;

    AssetMetadata metadata;
    metadata.Handle = handle;
    metadata.Type = AssetType::Texture;
    metadata.SourcePath = path;
    metadata.Name = path.filename().string();
    metadata.IsLoaded = false;

    try {
        auto texture = std::make_unique<Texture>(path);
        m_Textures[handle] = std::move(texture);
        metadata.IsLoaded = true;
        std::cout << "[AssetManager] Imported texture: " << metadata.Name
                  << " (Handle: " << static_cast<uint64_t>(handle) << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AssetManager] Failed to load texture: " << e.what() << std::endl;
        return GetDefaultTexture();
    }

    m_Registry[handle] = metadata;
    m_PathToHandle[pathStr] = handle;

    return handle;
}

AssetHandle AssetManager::ImportModel(const std::filesystem::path& path) {
    std::string pathStr = path.string();

    auto it = m_PathToHandle.find(pathStr);
    if (it != m_PathToHandle.end()) {
        return it->second;
    }

    if (!std::filesystem::exists(path)) {
        std::cerr << "[AssetManager] Model file not found: " << path << std::endl;
        return AssetHandle(0);
    }

    AssetHandle handle;

    AssetMetadata metadata;
    metadata.Handle = handle;
    metadata.Type = AssetType::Model;
    metadata.SourcePath = path;
    metadata.Name = path.filename().string();
    metadata.IsLoaded = false;

    try {
        auto model = std::make_unique<Model>(path);
        m_Models[handle] = std::move(model);
        metadata.IsLoaded = true;
        std::cout << "[AssetManager] Imported model: " << metadata.Name
                  << " (Handle: " << static_cast<uint64_t>(handle) << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AssetManager] Failed to load model: " << e.what() << std::endl;
        return AssetHandle(0);
    }

    m_Registry[handle] = metadata;
    m_PathToHandle[pathStr] = handle;

    return handle;
}

Texture* AssetManager::GetTexture(AssetHandle handle) {
    auto it = m_Textures.find(handle);
    if (it != m_Textures.end()) {
        return it->second.get();
    }
    return nullptr;
}

Model* AssetManager::GetModel(AssetHandle handle) {
    auto it = m_Models.find(handle);
    if (it != m_Models.end()) {
        return it->second.get();
    }
    return nullptr;
}

const AssetMetadata* AssetManager::GetMetadata(AssetHandle handle) const {
    auto it = m_Registry.find(handle);
    if (it != m_Registry.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<AssetHandle> AssetManager::GetAssetsOfType(AssetType type) const {
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

bool AssetManager::IsLoaded(AssetHandle handle) const {
    auto it = m_Registry.find(handle);
    if (it != m_Registry.end()) {
        return it->second.IsLoaded;
    }
    return false;
}

AssetHandle AssetManager::GetDefaultTexture() {
    return m_DefaultTexture;
}

AssetHandle AssetManager::GetWhiteTexture() {
    return m_WhiteTexture;
}

AssetHandle AssetManager::GetBlackTexture() {
    return m_BlackTexture;
}

void AssetManager::CreateDefaultAssets() {
    // 默认纹理（粉色/黑色棋盘格）
    {
        AssetHandle handle;

        unsigned char data[4 * 4 * 4];
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                int idx = (y * 4 + x) * 4;
                bool pink = ((x + y) % 2 == 0);
                data[idx + 0] = pink ? 255 : 0;
                data[idx + 1] = pink ? 0 : 0;
                data[idx + 2] = pink ? 255 : 0;
                data[idx + 3] = 255;
            }
        }

        auto texture = std::make_unique<Texture>(data, 4, 4, GL_RGBA, GL_RGBA8);
        m_Textures[handle] = std::move(texture);

        AssetMetadata metadata;
        metadata.Handle = handle;
        metadata.Type = AssetType::Texture;
        metadata.Name = "DefaultTexture";
        metadata.IsLoaded = true;
        metadata.IsDefault = true;
        m_Registry[handle] = metadata;

        m_DefaultTexture = handle;
    }

    // 白色纹理
    {
        AssetHandle handle;

        unsigned char data[4] = { 255, 255, 255, 255 };
        auto texture = std::make_unique<Texture>(data, 1, 1, GL_RGBA, GL_RGBA8);
        m_Textures[handle] = std::move(texture);

        AssetMetadata metadata;
        metadata.Handle = handle;
        metadata.Type = AssetType::Texture;
        metadata.Name = "WhiteTexture";
        metadata.IsLoaded = true;
        metadata.IsDefault = true;
        m_Registry[handle] = metadata;

        m_WhiteTexture = handle;
    }

    // 黑色纹理
    {
        AssetHandle handle;

        unsigned char data[4] = { 0, 0, 0, 255 };
        auto texture = std::make_unique<Texture>(data, 1, 1, GL_RGBA, GL_RGBA8);
        m_Textures[handle] = std::move(texture);

        AssetMetadata metadata;
        metadata.Handle = handle;
        metadata.Type = AssetType::Texture;
        metadata.Name = "BlackTexture";
        metadata.IsLoaded = true;
        metadata.IsDefault = true;
        m_Registry[handle] = metadata;

        m_BlackTexture = handle;
    }

    std::cout << "[AssetManager] Created default assets" << std::endl;
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
