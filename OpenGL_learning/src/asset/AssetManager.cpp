#include "AssetManager.h"
#include "Mesh.h"

namespace GLRenderer {

// ============================================================================
// 单例
// ============================================================================

AssetManager& AssetManager::Get() {
    static AssetManager instance;
    return instance;
}

AssetManager::~AssetManager() {
    if (m_Initialized) {
        Shutdown();
    }
}

// ============================================================================
// 生命周期
// ============================================================================

void AssetManager::Initialize(const std::filesystem::path& assetDirectory) {
    if (m_Initialized) {
        std::cout << "[AssetManager] Already initialized" << std::endl;
        return;
    }

    m_AssetDirectory = assetDirectory;
    std::cout << "[AssetManager] Initializing with root: " << assetDirectory << std::endl;

    // 创建默认资产
    CreateDefaultAssets();

    m_Initialized = true;
    std::cout << "[AssetManager] Initialized successfully" << std::endl;
}

void AssetManager::Shutdown() {
    std::cout << "[AssetManager] Shutting down..." << std::endl;

    m_Textures.clear();
    m_Models.clear();
    m_Registry.clear();
    m_PathToID.clear();

    m_DefaultTexture = TextureHandle();
    m_WhiteTexture = TextureHandle();
    m_BlackTexture = TextureHandle();

    m_NextID = 1;
    m_Initialized = false;

    std::cout << "[AssetManager] Shutdown complete" << std::endl;
}

// ============================================================================
// 资产导入
// ============================================================================

TextureHandle AssetManager::ImportTexture(const std::filesystem::path& path) {
    // 规范化路径
    std::string pathStr = path.string();

    // 检查是否已导入
    auto it = m_PathToID.find(pathStr);
    if (it != m_PathToID.end()) {
        return TextureHandle(it->second);
    }

    // 检查文件是否存在
    if (!std::filesystem::exists(path)) {
        std::cerr << "[AssetManager] Texture file not found: " << path << std::endl;
        return GetDefaultTexture();
    }

    // 生成新 ID
    AssetID id = GenerateAssetID();

    // 创建元数据
    AssetMetadata metadata;
    metadata.ID = id;
    metadata.Type = AssetType::Texture;
    metadata.SourcePath = path;
    metadata.Name = path.filename().string();
    metadata.IsLoaded = false;

    // 尝试加载纹理
    try {
        auto texture = std::make_unique<Texture>(path);
        m_Textures[id] = std::move(texture);
        metadata.IsLoaded = true;
        std::cout << "[AssetManager] Imported texture: " << metadata.Name << " (ID: " << id << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AssetManager] Failed to load texture: " << e.what() << std::endl;
        return GetDefaultTexture();
    }

    // 注册
    m_Registry[id] = metadata;
    m_PathToID[pathStr] = id;

    return TextureHandle(id);
}

ModelHandle AssetManager::ImportModel(const std::filesystem::path& path) {
    std::string pathStr = path.string();

    // 检查是否已导入
    auto it = m_PathToID.find(pathStr);
    if (it != m_PathToID.end()) {
        return ModelHandle(it->second);
    }

    // 检查文件是否存在
    if (!std::filesystem::exists(path)) {
        std::cerr << "[AssetManager] Model file not found: " << path << std::endl;
        return ModelHandle();
    }

    // 生成新 ID
    AssetID id = GenerateAssetID();

    // 创建元数据
    AssetMetadata metadata;
    metadata.ID = id;
    metadata.Type = AssetType::Model;
    metadata.SourcePath = path;
    metadata.Name = path.filename().string();
    metadata.IsLoaded = false;

    // 尝试加载模型
    try {
        auto model = std::make_unique<Model>(path);
        m_Models[id] = std::move(model);
        metadata.IsLoaded = true;
        std::cout << "[AssetManager] Imported model: " << metadata.Name << " (ID: " << id << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AssetManager] Failed to load model: " << e.what() << std::endl;
        return ModelHandle();
    }

    // 注册
    m_Registry[id] = metadata;
    m_PathToID[pathStr] = id;

    return ModelHandle(id);
}

// ============================================================================
// 资产访问
// ============================================================================

Texture* AssetManager::GetTexture(AssetID id) {
    auto it = m_Textures.find(id);
    if (it != m_Textures.end()) {
        return it->second.get();
    }
    return nullptr;
}

Model* AssetManager::GetModel(AssetID id) {
    auto it = m_Models.find(id);
    if (it != m_Models.end()) {
        return it->second.get();
    }
    return nullptr;
}

// ============================================================================
// 资产查询
// ============================================================================

const AssetMetadata* AssetManager::GetMetadata(AssetID id) const {
    auto it = m_Registry.find(id);
    if (it != m_Registry.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<AssetID> AssetManager::GetAssetsOfType(AssetType type) const {
    std::vector<AssetID> result;
    for (const auto& [id, metadata] : m_Registry) {
        if (metadata.Type == type) {
            result.push_back(id);
        }
    }
    return result;
}

AssetID AssetManager::FindAssetByPath(const std::filesystem::path& path) const {
    auto it = m_PathToID.find(path.string());
    if (it != m_PathToID.end()) {
        return it->second;
    }
    return NullAssetID;
}

bool AssetManager::IsLoaded(AssetID id) const {
    auto it = m_Registry.find(id);
    if (it != m_Registry.end()) {
        return it->second.IsLoaded;
    }
    return false;
}

// ============================================================================
// 默认资产
// ============================================================================

TextureHandle AssetManager::GetDefaultTexture() {
    return m_DefaultTexture;
}

TextureHandle AssetManager::GetWhiteTexture() {
    return m_WhiteTexture;
}

TextureHandle AssetManager::GetBlackTexture() {
    return m_BlackTexture;
}

void AssetManager::CreateDefaultAssets() {
    // 默认纹理（粉色/黑色棋盘格）
    {
        AssetID id = GenerateAssetID();

        unsigned char data[4 * 4 * 4];
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                int idx = (y * 4 + x) * 4;
                bool pink = ((x + y) % 2 == 0);
                data[idx + 0] = pink ? 255 : 0;   // R
                data[idx + 1] = pink ? 0 : 0;     // G
                data[idx + 2] = pink ? 255 : 0;   // B
                data[idx + 3] = 255;              // A
            }
        }

        auto texture = std::make_unique<Texture>(data, 4, 4, GL_RGBA, GL_RGBA8);
        m_Textures[id] = std::move(texture);

        AssetMetadata metadata;
        metadata.ID = id;
        metadata.Type = AssetType::Texture;
        metadata.Name = "DefaultTexture";
        metadata.IsLoaded = true;
        metadata.IsDefault = true;
        m_Registry[id] = metadata;

        m_DefaultTexture = TextureHandle(id);
    }

    // 白色纹理
    {
        AssetID id = GenerateAssetID();

        unsigned char data[4] = { 255, 255, 255, 255 };
        auto texture = std::make_unique<Texture>(data, 1, 1, GL_RGBA, GL_RGBA8);
        m_Textures[id] = std::move(texture);

        AssetMetadata metadata;
        metadata.ID = id;
        metadata.Type = AssetType::Texture;
        metadata.Name = "WhiteTexture";
        metadata.IsLoaded = true;
        metadata.IsDefault = true;
        m_Registry[id] = metadata;

        m_WhiteTexture = TextureHandle(id);
    }

    // 黑色纹理
    {
        AssetID id = GenerateAssetID();

        unsigned char data[4] = { 0, 0, 0, 255 };
        auto texture = std::make_unique<Texture>(data, 1, 1, GL_RGBA, GL_RGBA8);
        m_Textures[id] = std::move(texture);

        AssetMetadata metadata;
        metadata.ID = id;
        metadata.Type = AssetType::Texture;
        metadata.Name = "BlackTexture";
        metadata.IsLoaded = true;
        metadata.IsDefault = true;
        m_Registry[id] = metadata;

        m_BlackTexture = TextureHandle(id);
    }

    std::cout << "[AssetManager] Created default assets" << std::endl;
}

// ============================================================================
// 目录扫描
// ============================================================================

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

                // 跳过已注册的
                if (m_PathToID.find(pathStr) != m_PathToID.end()) {
                    continue;
                }

                // 仅注册元数据，不加载
                AssetID id = GenerateAssetID();

                AssetMetadata metadata;
                metadata.ID = id;
                metadata.Type = type;
                metadata.SourcePath = entry.path();
                metadata.Name = entry.path().filename().string();
                metadata.IsLoaded = false;

                m_Registry[id] = metadata;
                m_PathToID[pathStr] = id;
                count++;
            }
        }
    }

    std::cout << "[AssetManager] Scanned directory, found " << count << " new assets" << std::endl;
}

// ============================================================================
// 内部方法
// ============================================================================

AssetID AssetManager::GenerateAssetID() {
    return m_NextID++;
}

} // namespace GLRenderer
