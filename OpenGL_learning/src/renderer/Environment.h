#pragma once

#include "asset/Asset.h"
#include "graphics/Texture.h"

namespace GLRenderer {

// Environment Asset - 环境贴图资产
// 包含预计算的 IBL 数据:
// - RadianceMap: 预过滤环境贴图 (镜面反射)
// - IrradianceMap: 辐照度贴图 (漫反射)
class Environment : public Asset {
public:
    Ref<TextureCube> RadianceMap;
    Ref<TextureCube> IrradianceMap;

    Environment() = default;
    Environment(const Ref<TextureCube>& radianceMap, const Ref<TextureCube>& irradianceMap)
        : RadianceMap(radianceMap), IrradianceMap(irradianceMap) {}

    static AssetType GetStaticType() { return AssetType::EnvMap; }
    AssetType GetAssetType() const override { return GetStaticType(); }

    bool IsValid() const { return RadianceMap && IrradianceMap; }
};

} // namespace GLRenderer
