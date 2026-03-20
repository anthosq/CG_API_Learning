#include "Material.h"

namespace GLRenderer {

Material::Material(const Ref<Shader>& shader, const std::string& name)
    : m_Shader(shader), m_Name(name) {
}

void Material::SetTexture(uint32_t slot, const Ref<Texture2D>& texture) {
    m_Textures[slot] = texture;
}

void Material::SetTexture(const std::string& name, uint32_t slot, const Ref<Texture2D>& texture) {
    m_Textures[slot] = texture;
    m_TextureSlotNames[name] = slot;
}

void Material::SetTextureCube(uint32_t slot, const Ref<TextureCube>& texture) {
    m_TexturesCube[slot] = texture;
}

Ref<Texture2D> Material::GetTexture(uint32_t slot) const {
    auto it = m_Textures.find(slot);
    return (it != m_Textures.end()) ? it->second : nullptr;
}

Ref<TextureCube> Material::GetTextureCube(uint32_t slot) const {
    auto it = m_TexturesCube.find(slot);
    return (it != m_TexturesCube.end()) ? it->second : nullptr;
}

void Material::SetFlag(Flag flag, bool value) {
    if (value) {
        m_Flags |= static_cast<uint32_t>(flag);
    } else {
        m_Flags &= ~static_cast<uint32_t>(flag);
    }
}

bool Material::GetFlag(Flag flag) const {
    return (m_Flags & static_cast<uint32_t>(flag)) != 0;
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

    for (const auto& [name, slot] : m_TextureSlotNames) {
        m_Shader->SetInt(name, static_cast<int>(slot));
    }
}

void Material::BindTextures() const {
    for (const auto& [slot, texture] : m_Textures) {
        if (texture && texture->IsValid()) {
            texture->Bind(slot);
        }
    }

    for (const auto& [slot, texture] : m_TexturesCube) {
        if (texture && texture->IsValid()) {
            texture->Bind(slot);
        }
    }
}

Ref<Material> Material::Create(const Ref<Shader>& shader, const std::string& name) {
    return Ref<Material>(new Material(shader, name));
}

Ref<Material> Material::Copy(const Ref<Material>& other) {
    if (!other) return nullptr;

    auto material = Ref<Material>(new Material(other->m_Shader, other->m_Name + "_Copy"));
    material->m_Uniforms = other->m_Uniforms;
    material->m_Textures = other->m_Textures;
    material->m_TexturesCube = other->m_TexturesCube;
    material->m_TextureSlotNames = other->m_TextureSlotNames;
    material->m_Flags = other->m_Flags;
    return material;
}

} // namespace GLRenderer
