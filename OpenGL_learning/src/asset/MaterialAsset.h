#pragma once

#include "Asset.h"
#include "AssetTypes.h"
#include "renderer/Material.h"
#include <map>

namespace GLRenderer {

class MaterialAsset : public Asset {
public:
    MaterialAsset() = default;
    explicit MaterialAsset(bool transparent);
    explicit MaterialAsset(const Ref<Material>& material);
    ~MaterialAsset() = default;

    Ref<Material> GetMaterial() const { return m_Material; }
    void SetMaterial(const Ref<Material>& material) { m_Material = material; }

    glm::vec3 GetAlbedoColor() const;
    void SetAlbedoColor(const glm::vec3& color);

    float GetMetallic() const;
    void SetMetallic(float value);

    float GetRoughness() const;
    void SetRoughness(float value);

    float GetEmission() const;
    void SetEmission(float value);

    AssetHandle GetAlbedoMap() const { return m_AlbedoMap; }
    void SetAlbedoMap(AssetHandle handle);

    AssetHandle GetNormalMap() const { return m_NormalMap; }
    void SetNormalMap(AssetHandle handle);

    AssetHandle GetMetallicMap() const { return m_MetallicMap; }
    void SetMetallicMap(AssetHandle handle);

    AssetHandle GetRoughnessMap() const { return m_RoughnessMap; }
    void SetRoughnessMap(AssetHandle handle);

    AssetHandle GetAOMap() const { return m_AOMap; }
    void SetAOMap(AssetHandle handle);

    AssetHandle GetEmissiveMap() const { return m_EmissiveMap; }
    void SetEmissiveMap(AssetHandle handle);

    bool IsUsingNormalMap() const { return m_UseNormalMap; }
    void SetUseNormalMap(bool value) { m_UseNormalMap = value; }

    bool IsTransparent() const { return m_Transparent; }
    void SetTransparent(bool value) { m_Transparent = value; }

    static AssetType GetStaticType() { return AssetType::Material; }
    AssetType GetAssetType() const override { return GetStaticType(); }

    static Ref<MaterialAsset> Create(bool transparent = false);
    static Ref<MaterialAsset> Create(const Ref<Material>& material);

private:
    Ref<Material> m_Material;
    bool m_Transparent = false;
    bool m_UseNormalMap = false;

    AssetHandle m_AlbedoMap = 0;
    AssetHandle m_NormalMap = 0;
    AssetHandle m_MetallicMap = 0;
    AssetHandle m_RoughnessMap = 0;
    AssetHandle m_AOMap = 0;
    AssetHandle m_EmissiveMap = 0;
};

class MaterialTable : public RefCounter {
public:
    MaterialTable() = default;
    explicit MaterialTable(uint32_t materialCount);
    MaterialTable(const Ref<MaterialTable>& other);
    ~MaterialTable() = default;

    bool HasMaterial(uint32_t index) const;
    AssetHandle GetMaterial(uint32_t index) const;
    void SetMaterial(uint32_t index, AssetHandle material);
    void ClearMaterial(uint32_t index);
    void Clear();

    const std::map<uint32_t, AssetHandle>& GetMaterials() const { return m_Materials; }

    uint32_t GetMaterialCount() const { return m_MaterialCount; }
    void SetMaterialCount(uint32_t count) { m_MaterialCount = count; }

    static Ref<MaterialTable> Create(uint32_t materialCount = 0);
    static Ref<MaterialTable> Copy(const Ref<MaterialTable>& other);

private:
    std::map<uint32_t, AssetHandle> m_Materials;
    uint32_t m_MaterialCount = 0;
};

} // namespace GLRenderer
