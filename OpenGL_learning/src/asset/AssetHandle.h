#pragma once

// ============================================================================
// AssetHandle.h - 类型安全的资产句柄
// ============================================================================

#include "AssetTypes.h"


namespace GLRenderer {

// 前向声明
class Texture;
class Shader;
class AssetManager;
class Model;

// AssetHandle - 类型安全的资产引用
//// AssetHandle 是对资产的轻量级引用，通过 AssetID 间接访问实际资产。
// 这种设计允许：
// 1. 资产热重载而不破坏引用
// 2. 延迟加载
// 3. 资产去重

template<typename T>
class AssetHandle {
public:
    AssetHandle() : m_ID(NullAssetID) {}
    explicit AssetHandle(AssetID id) : m_ID(id) {}

    bool IsValid() const { return m_ID != NullAssetID; }

    T* Get() const;

    T* TryGet() const;

    AssetID GetID() const { return m_ID; }

    operator bool() const { return IsValid(); }

    bool operator==(const AssetHandle& other) const { return m_ID == other.m_ID; }
    bool operator!=(const AssetHandle& other) const { return m_ID != other.m_ID; }

    // 用于 std::hash
    struct Hash {
        size_t operator()(const AssetHandle& handle) const {
            return std::hash<AssetID>{}(handle.m_ID);
        }
    };

private:
    AssetID m_ID;
};

// 类型别名
using TextureHandle = AssetHandle<Texture>;
using ShaderHandle = AssetHandle<Shader>;
using ModelHandle = AssetHandle<Model>;

} // namespace GLRenderer

// 注意: Get() 和 TryGet() 的实现在 AssetManager.cpp 中，
// 因为它们需要访问 AssetManager
