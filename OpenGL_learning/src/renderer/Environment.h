#pragma once

#include "asset/Asset.h"
#include "graphics/Texture.h"

namespace GLRenderer {

// Environment Asset - 环境贴图资产
// 包含预计算的 IBL 数据:
// - EnvMap:        原始环境 CubeMap, 1024x1024, 用于天空盒显示 (清晰)
// - RadianceMap:   预过滤环境贴图, 256x256+mips, 用于镜面反射 IBL
// - IrradianceMap: 辐照度贴图, 32x32, 用于漫反射 IBL
class Environment : public Asset {
public:
    Ref<TextureCube> EnvMap;          // 原始环境 (天空盒用)
    Ref<TextureCube> RadianceMap;     // 预过滤镜面反射 IBL
    Ref<TextureCube> IrradianceMap;   // 辐照度漫反射 IBL

    Environment() = default;
    Environment(const Ref<TextureCube>& envMap,
                const Ref<TextureCube>& radianceMap,
                const Ref<TextureCube>& irradianceMap)
        : EnvMap(envMap), RadianceMap(radianceMap), IrradianceMap(irradianceMap) {}

    static AssetType GetStaticType() { return AssetType::Environment; }
    AssetType GetAssetType() const override { return GetStaticType(); }

    bool IsValid() const { return RadianceMap && IrradianceMap; }
};

} // namespace GLRenderer
