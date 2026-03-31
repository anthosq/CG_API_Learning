#include "Material.h"

namespace GLRenderer {

Material::Material(const std::string& name)
    : m_Name(name) {
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

void Material::Apply(Shader& shader) const {
    // 上传 uniform 值
    for (const auto& [name, value] : m_Uniforms) {
        std::visit([&shader, &name](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, bool>) {
                shader.SetBool(name, arg);
            }
            else if constexpr (std::is_same_v<T, int>) {
                shader.SetInt(name, arg);
            }
            else if constexpr (std::is_same_v<T, float>) {
                shader.SetFloat(name, arg);
            }
            else if constexpr (std::is_same_v<T, glm::vec2>) {
                shader.SetVec2(name, arg);
            }
            else if constexpr (std::is_same_v<T, glm::vec3>) {
                shader.SetVec3(name, arg);
            }
            else if constexpr (std::is_same_v<T, glm::vec4>) {
                shader.SetVec4(name, arg);
            }
            else if constexpr (std::is_same_v<T, glm::mat3>) {
                shader.SetMat3(name, arg);
            }
            else if constexpr (std::is_same_v<T, glm::mat4>) {
                shader.SetMat4(name, arg);
            }
        }, value);
    }

    // 设置纹理采样器 uniform
    for (const auto& [name, slot] : m_TextureSlotNames) {
        shader.SetInt(name, static_cast<int>(slot));
    }

    BindTextures();
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

Ref<Material> Material::Create(const std::string& name) {
    return Ref<Material>(new Material(name));
}

Ref<Material> Material::Copy(const Ref<Material>& other) {
    if (!other) return nullptr;

    auto material = Ref<Material>(new Material(other->m_Name + "_Copy"));
    material->m_Uniforms = other->m_Uniforms;
    material->m_Textures = other->m_Textures;
    material->m_TexturesCube = other->m_TexturesCube;
    material->m_TextureSlotNames = other->m_TextureSlotNames;
    material->m_Flags = other->m_Flags;
    return material;
}

} // namespace GLRenderer
