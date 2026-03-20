#pragma once

#include "core/Ref.h"
#include "graphics/Shader.h"
#include "graphics/Texture.h"
#include "asset/AssetTypes.h"
#include <unordered_map>
#include <string>
#include <variant>

namespace GLRenderer {

namespace TextureSlot {
    constexpr uint32_t Albedo      = 0;
    constexpr uint32_t Normal      = 1;
    constexpr uint32_t Metallic    = 2;
    constexpr uint32_t Roughness   = 3;
    constexpr uint32_t AO          = 4;
    constexpr uint32_t Emissive    = 5;
    constexpr uint32_t Irradiance  = 6;
    constexpr uint32_t Prefilter   = 7;
    constexpr uint32_t BrdfLUT     = 8;
    constexpr uint32_t Diffuse     = 0;
    constexpr uint32_t Specular    = 2;
}

using UniformValue = std::variant<
    bool, int, float,
    glm::vec2, glm::vec3, glm::vec4,
    glm::mat3, glm::mat4
>;

class Material : public RefCounter {
public:
    enum class Flag : uint32_t {
        None = 0,
        DepthTest = 1 << 0,
        Blend = 1 << 1,
        TwoSided = 1 << 2,
        DisableShadowCasting = 1 << 3
    };

    Material() = default;
    explicit Material(const Ref<Shader>& shader, const std::string& name = "");
    ~Material() = default;

    Ref<Shader> GetShader() const { return m_Shader; }
    void SetShader(const Ref<Shader>& shader) { m_Shader = shader; }
    bool HasShader() const { return m_Shader && m_Shader->IsValid(); }

    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    template<typename T>
    void Set(const std::string& name, const T& value) {
        m_Uniforms[name] = value;
    }

    template<typename T>
    T Get(const std::string& name, const T& defaultValue = T{}) const {
        auto it = m_Uniforms.find(name);
        if (it != m_Uniforms.end()) {
            if (auto* ptr = std::get_if<T>(&it->second)) {
                return *ptr;
            }
        }
        return defaultValue;
    }

    bool Has(const std::string& name) const {
        return m_Uniforms.find(name) != m_Uniforms.end();
    }

    void Remove(const std::string& name) {
        m_Uniforms.erase(name);
    }

    const std::unordered_map<std::string, UniformValue>& GetUniforms() const {
        return m_Uniforms;
    }

    void SetTexture(uint32_t slot, const Ref<Texture2D>& texture);
    void SetTexture(const std::string& name, uint32_t slot, const Ref<Texture2D>& texture);
    void SetTextureCube(uint32_t slot, const Ref<TextureCube>& texture);

    Ref<Texture2D> GetTexture(uint32_t slot) const;
    Ref<TextureCube> GetTextureCube(uint32_t slot) const;

    const std::unordered_map<uint32_t, Ref<Texture2D>>& GetTextures() const {
        return m_Textures;
    }

    void SetFlag(Flag flag, bool value = true);
    bool GetFlag(Flag flag) const;
    uint32_t GetFlags() const { return m_Flags; }

    void Bind() const;
    void Unbind() const;
    void UploadUniforms() const;
    void BindTextures() const;

    static Ref<Material> Create(const Ref<Shader>& shader, const std::string& name = "");
    static Ref<Material> Copy(const Ref<Material>& other);

private:
    Ref<Shader> m_Shader;
    std::string m_Name;
    std::unordered_map<std::string, UniformValue> m_Uniforms;
    std::unordered_map<uint32_t, Ref<Texture2D>> m_Textures;
    std::unordered_map<uint32_t, Ref<TextureCube>> m_TexturesCube;
    std::unordered_map<std::string, uint32_t> m_TextureSlotNames;
    uint32_t m_Flags = static_cast<uint32_t>(Flag::DepthTest);
};

inline Material::Flag operator|(Material::Flag a, Material::Flag b) {
    return static_cast<Material::Flag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline Material::Flag operator&(Material::Flag a, Material::Flag b) {
    return static_cast<Material::Flag>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

} // namespace GLRenderer
