#pragma once

// AssetTypes.h - 资产类型定义

#include <string>
#include <filesystem>
#include <cstdint>

#include "core/Ref.h"
#include "core/UUID.h"

namespace GLRenderer {

// AssetHandle - 使用 UUID 作为资产句柄
using AssetHandle = UUID;

// 资产类型枚举
enum class AssetType : uint8_t {
    Unknown = 0,
    Texture,
    Shader,
    Model,
    Material,
    Cubemap
};

// 资产元数据
struct AssetMetadata {
    AssetHandle Handle;
    AssetType Type = AssetType::Unknown;
    std::filesystem::path SourcePath;
    std::string Name;
    bool IsLoaded = false;
    bool IsDefault = false;

    bool IsValid() const { return Handle.IsValid(); }
};

// 工具函数
inline AssetType GetAssetTypeFromExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();

    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".dds" || ext == ".tga" || ext == ".bmp") {
        return AssetType::Texture;
    }

    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" ||
        ext == ".glb" || ext == ".dae" || ext == ".3ds") {
        return AssetType::Model;
    }

    if (ext == ".glsl" || ext == ".vert" || ext == ".frag" ||
        ext == ".vs" || ext == ".fs") {
        return AssetType::Shader;
    }

    return AssetType::Unknown;
}

inline const char* AssetTypeToString(AssetType type) {
    switch (type) {
        case AssetType::Texture:  return "Texture";
        case AssetType::Shader:   return "Shader";
        case AssetType::Model:    return "Model";
        case AssetType::Material: return "Material";
        case AssetType::Cubemap:  return "Cubemap";
        default:                  return "Unknown";
    }
}

} // namespace GLRenderer
