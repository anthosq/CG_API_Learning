#include "Material.h"

namespace GLRenderer {

Material::Material(std::shared_ptr<Shader> shader)
    : m_Shader(std::move(shader)) {
}

void Material::SetShader(std::shared_ptr<Shader> shader) {
    m_Shader = std::move(shader);
}

void Material::SetTexture(uint32_t slot, Texture* texture) {
    m_Textures[slot] = texture;
}

void Material::SetTexture(const std::string& name, uint32_t slot, Texture* texture) {
    m_Textures[slot] = texture;
    m_TextureSlotNames[name] = slot;
}

Texture* Material::GetTexture(uint32_t slot) const {
    auto it = m_Textures.find(slot);
    if (it != m_Textures.end()) {
        return it->second;
    }
    return nullptr;
}

void Material::Bind() const {
    if (!m_Shader || !m_Shader->IsValid()) {
        return;
    }

    m_Shader->Bind();
    UploadUniforms();
    BindTextures();
}

void Material::Unbind() const {
    if (m_Shader) {
        m_Shader->Unbind();
    }
}

void Material::UploadUniforms() const {
    if (!m_Shader || !m_Shader->IsValid()) {
        return;
    }

    for (const auto& [name, value] : m_Uniforms) {
        std::visit([this, &name](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, bool>) {
                m_Shader->SetBool(name, arg);
            }
            else if constexpr (std::is_same_v<T, int>) {
                m_Shader->SetInt(name, arg);
            }
            else if constexpr (std::is_same_v<T, float>) {
                m_Shader->SetFloat(name, arg);
            }
            else if constexpr (std::is_same_v<T, glm::vec2>) {
                m_Shader->SetVec2(name, arg);
            }
            else if constexpr (std::is_same_v<T, glm::vec3>) {
                m_Shader->SetVec3(name, arg);
            }
            else if constexpr (std::is_same_v<T, glm::vec4>) {
                m_Shader->SetVec4(name, arg);
            }
            else if constexpr (std::is_same_v<T, glm::mat3>) {
                m_Shader->SetMat3(name, arg);
            }
            else if constexpr (std::is_same_v<T, glm::mat4>) {
                m_Shader->SetMat4(name, arg);
            }
        }, value);
    }

    // 设置纹理采样器 uniform
    for (const auto& [name, slot] : m_TextureSlotNames) {
        m_Shader->SetInt(name, static_cast<int>(slot));
    }
}

void Material::BindTextures() const {
    for (const auto& [slot, texture] : m_Textures) {
        if (texture && texture->IsValid()) {
            glActiveTexture(GL_TEXTURE0 + slot);
            texture->Bind();
        }
    }

    // 绑定 IBL 纹理 (使用 TextureCube)
    if (m_PBRParams.UseIBL) {
        if (m_IrradianceMap) {
            glActiveTexture(GL_TEXTURE0 + TextureSlot::Irradiance);
            m_IrradianceMap->Bind();
        }
        if (m_PrefilterMap) {
            glActiveTexture(GL_TEXTURE0 + TextureSlot::Prefilter);
            m_PrefilterMap->Bind();
        }
        if (m_BrdfLUT && m_BrdfLUT->IsValid()) {
            glActiveTexture(GL_TEXTURE0 + TextureSlot::BrdfLUT);
            m_BrdfLUT->Bind();
        }
    }

    // 恢复活动纹理单元到 0
    glActiveTexture(GL_TEXTURE0);
}

// ============================================================================
// PBR 方法实现
// ============================================================================

void Material::SetPBRParams(const PBRMaterialParams& params) {
    m_PBRParams = params;

    // 设置对应的 uniform 值
    Set("u_AlbedoColor", params.AlbedoColor);
    Set("u_Metallic", params.Metallic);
    Set("u_Roughness", params.Roughness);
    Set("u_AO", params.AO);
    Set("u_EmissiveColor", params.EmissiveColor);
    Set("u_EmissiveIntensity", params.EmissiveIntensity);

    // 纹理使用标志
    Set("u_UseAlbedoMap", params.UseAlbedoMap);
    Set("u_UseNormalMap", params.UseNormalMap);
    Set("u_UseMetallicMap", params.UseMetallicMap);
    Set("u_UseRoughnessMap", params.UseRoughnessMap);
    Set("u_UseAOMap", params.UseAOMap);
    Set("u_UseEmissiveMap", params.UseEmissiveMap);

    // IBL 设置
    Set("u_UseIBL", params.UseIBL);
    Set("u_EnvironmentIntensity", params.EnvironmentIntensity);

    // 设置纹理采样器 uniform (槽位)
    Set("u_AlbedoMap", static_cast<int>(TextureSlot::Albedo));
    Set("u_NormalMap", static_cast<int>(TextureSlot::Normal));
    Set("u_MetallicMap", static_cast<int>(TextureSlot::Metallic));
    Set("u_RoughnessMap", static_cast<int>(TextureSlot::Roughness));
    Set("u_AOMap", static_cast<int>(TextureSlot::AO));
    Set("u_EmissiveMap", static_cast<int>(TextureSlot::Emissive));
    Set("u_IrradianceMap", static_cast<int>(TextureSlot::Irradiance));
    Set("u_PrefilterMap", static_cast<int>(TextureSlot::Prefilter));
    Set("u_BrdfLUT", static_cast<int>(TextureSlot::BrdfLUT));
}

PBRMaterialParams Material::GetPBRParams() const {
    return m_PBRParams;
}

void Material::SetAlbedo(const glm::vec3& color) {
    m_PBRParams.AlbedoColor = color;
    Set("u_AlbedoColor", color);
}

void Material::SetMetallic(float value) {
    m_PBRParams.Metallic = value;
    Set("u_Metallic", value);
}

void Material::SetRoughness(float value) {
    m_PBRParams.Roughness = value;
    Set("u_Roughness", value);
}

void Material::SetAO(float value) {
    m_PBRParams.AO = value;
    Set("u_AO", value);
}

void Material::SetEmissive(const glm::vec3& color, float intensity) {
    m_PBRParams.EmissiveColor = color;
    m_PBRParams.EmissiveIntensity = intensity;
    Set("u_EmissiveColor", color);
    Set("u_EmissiveIntensity", intensity);
}

void Material::SetAlbedoMap(Texture* texture) {
    m_Textures[TextureSlot::Albedo] = texture;
    m_PBRParams.UseAlbedoMap = (texture != nullptr);
    Set("u_UseAlbedoMap", m_PBRParams.UseAlbedoMap);
    Set("u_AlbedoMap", static_cast<int>(TextureSlot::Albedo));
}

void Material::SetNormalMap(Texture* texture) {
    m_Textures[TextureSlot::Normal] = texture;
    m_PBRParams.UseNormalMap = (texture != nullptr);
    Set("u_UseNormalMap", m_PBRParams.UseNormalMap);
    Set("u_NormalMap", static_cast<int>(TextureSlot::Normal));
}

void Material::SetMetallicMap(Texture* texture) {
    m_Textures[TextureSlot::Metallic] = texture;
    m_PBRParams.UseMetallicMap = (texture != nullptr);
    Set("u_UseMetallicMap", m_PBRParams.UseMetallicMap);
    Set("u_MetallicMap", static_cast<int>(TextureSlot::Metallic));
}

void Material::SetRoughnessMap(Texture* texture) {
    m_Textures[TextureSlot::Roughness] = texture;
    m_PBRParams.UseRoughnessMap = (texture != nullptr);
    Set("u_UseRoughnessMap", m_PBRParams.UseRoughnessMap);
    Set("u_RoughnessMap", static_cast<int>(TextureSlot::Roughness));
}

void Material::SetAOMap(Texture* texture) {
    m_Textures[TextureSlot::AO] = texture;
    m_PBRParams.UseAOMap = (texture != nullptr);
    Set("u_UseAOMap", m_PBRParams.UseAOMap);
    Set("u_AOMap", static_cast<int>(TextureSlot::AO));
}

void Material::SetEmissiveMap(Texture* texture) {
    m_Textures[TextureSlot::Emissive] = texture;
    m_PBRParams.UseEmissiveMap = (texture != nullptr);
    Set("u_UseEmissiveMap", m_PBRParams.UseEmissiveMap);
    Set("u_EmissiveMap", static_cast<int>(TextureSlot::Emissive));
}

void Material::SetIBL(TextureCube* irradianceMap, TextureCube* prefilterMap,
                      Texture* brdfLUT, float intensity) {
    m_IrradianceMap = irradianceMap;
    m_PrefilterMap = prefilterMap;
    m_BrdfLUT = brdfLUT;
    m_PBRParams.UseIBL = (irradianceMap && prefilterMap && brdfLUT);
    m_PBRParams.EnvironmentIntensity = intensity;

    Set("u_UseIBL", m_PBRParams.UseIBL);
    Set("u_EnvironmentIntensity", intensity);
    Set("u_IrradianceMap", static_cast<int>(TextureSlot::Irradiance));
    Set("u_PrefilterMap", static_cast<int>(TextureSlot::Prefilter));
    Set("u_BrdfLUT", static_cast<int>(TextureSlot::BrdfLUT));
}

void Material::DisableIBL() {
    m_PBRParams.UseIBL = false;
    Set("u_UseIBL", false);
}

} // namespace GLRenderer
