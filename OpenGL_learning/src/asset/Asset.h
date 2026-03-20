#pragma once

#include "core/Ref.h"
#include "asset/AssetTypes.h"

namespace GLRenderer {

enum class AssetFlag : uint16_t {
    None = 0,
    Missing = 1 << 0,
    Invalid = 1 << 1
};

class Asset : public RefCounter {
public:
    AssetHandle Handle;
    uint16_t Flags = 0;

    virtual ~Asset() = default;

    static AssetType GetStaticType() { return AssetType::Unknown; }
    virtual AssetType GetAssetType() const { return AssetType::Unknown; }

    bool IsValid() const { return !IsFlagSet(AssetFlag::Invalid) && !IsFlagSet(AssetFlag::Missing); }

    bool IsFlagSet(AssetFlag flag) const {
        return (Flags & static_cast<uint16_t>(flag)) != 0;
    }

    void SetFlag(AssetFlag flag, bool value = true) {
        if (value) {
            Flags |= static_cast<uint16_t>(flag);
        } else {
            Flags &= ~static_cast<uint16_t>(flag);
        }
    }
};

} // namespace GLRenderer
