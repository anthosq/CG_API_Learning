#include "MaterialAsset.h"

namespace GLRenderer {

MaterialAsset::MaterialAsset(bool transparent)
    : m_Transparent(transparent) {
    m_Material = Material::Create(nullptr, "DefaultMaterial");

    m_Material->Set<glm::vec3>("u_AlbedoColor", glm::vec3(1.0f));
    m_Material->Set<float>("u_Metallic", 0.0f);
    m_Material->Set<float>("u_Roughness", 0.5f);
    m_Material->Set<float>("u_Emission", 0.0f);
}

MaterialAsset::MaterialAsset(const Ref<Material>& material)
    : m_Material(material) {
}

glm::vec3 MaterialAsset::GetAlbedoColor() const {
    if (m_Material) {
        return m_Material->Get<glm::vec3>("u_AlbedoColor", glm::vec3(1.0f));
    }
    return glm::vec3(1.0f);
}

void MaterialAsset::SetAlbedoColor(const glm::vec3& color) {
    if (m_Material) {
        m_Material->Set<glm::vec3>("u_AlbedoColor", color);
    }
}

float MaterialAsset::GetMetallic() const {
    if (m_Material) {
        return m_Material->Get<float>("u_Metallic", 0.0f);
    }
    return 0.0f;
}

void MaterialAsset::SetMetallic(float value) {
    if (m_Material) {
        m_Material->Set<float>("u_Metallic", value);
    }
}

float MaterialAsset::GetRoughness() const {
    if (m_Material) {
        return m_Material->Get<float>("u_Roughness", 0.5f);
    }
    return 0.5f;
}

void MaterialAsset::SetRoughness(float value) {
    if (m_Material) {
        m_Material->Set<float>("u_Roughness", value);
    }
}

float MaterialAsset::GetEmission() const {
    if (m_Material) {
        return m_Material->Get<float>("u_Emission", 0.0f);
    }
    return 0.0f;
}

void MaterialAsset::SetEmission(float value) {
    if (m_Material) {
        m_Material->Set<float>("u_Emission", value);
    }
}

void MaterialAsset::SetAlbedoMap(AssetHandle handle) {
    m_AlbedoMap = handle;
}

void MaterialAsset::SetNormalMap(AssetHandle handle) {
    m_NormalMap = handle;
    m_UseNormalMap = handle.IsValid();
}

void MaterialAsset::SetMetallicMap(AssetHandle handle) {
    m_MetallicMap = handle;
}

void MaterialAsset::SetRoughnessMap(AssetHandle handle) {
    m_RoughnessMap = handle;
}

void MaterialAsset::SetAOMap(AssetHandle handle) {
    m_AOMap = handle;
}

void MaterialAsset::SetEmissiveMap(AssetHandle handle) {
    m_EmissiveMap = handle;
}

Ref<MaterialAsset> MaterialAsset::Create(bool transparent) {
    return Ref<MaterialAsset>(new MaterialAsset(transparent));
}

Ref<MaterialAsset> MaterialAsset::Create(const Ref<Material>& material) {
    return Ref<MaterialAsset>(new MaterialAsset(material));
}

MaterialTable::MaterialTable(uint32_t materialCount)
    : m_MaterialCount(materialCount) {
}

MaterialTable::MaterialTable(const Ref<MaterialTable>& other) {
    if (other) {
        m_Materials = other->m_Materials;
        m_MaterialCount = other->m_MaterialCount;
    }
}

bool MaterialTable::HasMaterial(uint32_t index) const {
    return m_Materials.find(index) != m_Materials.end();
}

AssetHandle MaterialTable::GetMaterial(uint32_t index) const {
    auto it = m_Materials.find(index);
    if (it != m_Materials.end()) {
        return it->second;
    }
    return 0;
}

void MaterialTable::SetMaterial(uint32_t index, AssetHandle material) {
    m_Materials[index] = material;
    if (index >= m_MaterialCount) {
        m_MaterialCount = index + 1;
    }
}

void MaterialTable::ClearMaterial(uint32_t index) {
    m_Materials.erase(index);
}

void MaterialTable::Clear() {
    m_Materials.clear();
}

Ref<MaterialTable> MaterialTable::Create(uint32_t materialCount) {
    return Ref<MaterialTable>(new MaterialTable(materialCount));
}

Ref<MaterialTable> MaterialTable::Copy(const Ref<MaterialTable>& other) {
    return Ref<MaterialTable>(new MaterialTable(other));
}

} // namespace GLRenderer
