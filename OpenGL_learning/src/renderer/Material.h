#pragma once

// Material - 材质类
//
// 封装着色器、uniform 值和纹理绑定。
// 参考 Hazel Engine 设计，简化为 OpenGL 单线程模型。
//
// 职责:
// - 持有 Shader 引用
// - 存储 uniform 值 (CPU 端)
// - 管理纹理槽位绑定
// - 支持通过 AssetHandle 序列化
//
// PBR 支持:
// - Metallic-Roughness 工作流
// - 支持 Albedo, Normal, Metallic, Roughness, AO, Emissive 贴图
// - 支持 IBL (Image-Based Lighting)

#include "graphics/Shader.h"
#include "graphics/Texture.h"
#include "asset/AssetTypes.h"

#include <memory>
#include <unordered_map>
#include <string>
#include <variant>

namespace GLRenderer {

// ============================================================================
// PBR 纹理槽位常量
// ============================================================================
namespace TextureSlot {
    constexpr uint32_t Albedo      = 0;
    constexpr uint32_t Normal      = 1;
    constexpr uint32_t Metallic    = 2;
    constexpr uint32_t Roughness   = 3;
    constexpr uint32_t AO          = 4;
    constexpr uint32_t Emissive    = 5;
    // IBL 纹理
    constexpr uint32_t Irradiance  = 6;
    constexpr uint32_t Prefilter   = 7;
    constexpr uint32_t BrdfLUT     = 8;
    // 旧版兼容
    constexpr uint32_t Diffuse     = 0;  // 别名
    constexpr uint32_t Specular    = 2;  // 别名
}

// ============================================================================
// PBR 材质参数
// ============================================================================
struct PBRMaterialParams {
    // 基础颜色
    glm::vec3 AlbedoColor = glm::vec3(1.0f);

    // PBR 参数
    float Metallic = 0.0f;    // 0 = 非金属 (电介质), 1 = 金属
    float Roughness = 0.5f;   // 0 = 光滑, 1 = 粗糙
    float AO = 1.0f;          // 环境遮蔽

    // 自发光
    glm::vec3 EmissiveColor = glm::vec3(0.0f);
    float EmissiveIntensity = 0.0f;

    // 纹理使用标志
    bool UseAlbedoMap = false;
    bool UseNormalMap = false;
    bool UseMetallicMap = false;
    bool UseRoughnessMap = false;
    bool UseAOMap = false;
    bool UseEmissiveMap = false;

    // IBL 设置
    bool UseIBL = false;
    float EnvironmentIntensity = 1.0f;
};

// Uniform 值类型 (使用 std::variant 实现类型安全存储)
using UniformValue = std::variant<
    bool, int, float,
    glm::vec2, glm::vec3, glm::vec4,
    glm::mat3, glm::mat4
>;

class Material {
public:
    Material() = default;
    explicit Material(std::shared_ptr<Shader> shader);
    ~Material() = default;

    // 支持拷贝 (材质实例化)
    Material(const Material&) = default;
    Material& operator=(const Material&) = default;

    // 支持移动
    Material(Material&&) = default;
    Material& operator=(Material&&) = default;

    // === 着色器 ===

    void SetShader(std::shared_ptr<Shader> shader);
    std::shared_ptr<Shader> GetShader() const { return m_Shader; }
    bool HasShader() const { return m_Shader != nullptr && m_Shader->IsValid(); }

    // === Uniform 值设置/获取 ===

    // 设置 uniform 值 (存储到 CPU 缓冲)
    template<typename T>
    void Set(const std::string& name, const T& value) {
        m_Uniforms[name] = value;
    }

    // 获取 uniform 值
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

    // 检查 uniform 是否存在
    bool Has(const std::string& name) const {
        return m_Uniforms.find(name) != m_Uniforms.end();
    }

    // 移除 uniform
    void Remove(const std::string& name) {
        m_Uniforms.erase(name);
    }

    // 获取所有 uniform
    const std::unordered_map<std::string, UniformValue>& GetUniforms() const {
        return m_Uniforms;
    }

    // === 纹理绑定 ===

    // 按槽位设置纹理
    void SetTexture(uint32_t slot, Texture* texture);

    // 按名称和槽位设置纹理 (名称用于 shader uniform)
    void SetTexture(const std::string& name, uint32_t slot, Texture* texture);

    // 获取纹理
    Texture* GetTexture(uint32_t slot) const;

    // 获取所有纹理绑定
    const std::unordered_map<uint32_t, Texture*>& GetTextures() const {
        return m_Textures;
    }

    // === 应用材质 ===

    // 绑定着色器、上传 uniform、绑定纹理
    void Bind() const;
    void Unbind() const;

    // 仅上传 uniform 值 (假设着色器已绑定)
    void UploadUniforms() const;

    // 仅绑定纹理
    void BindTextures() const;

    // === PBR 材质设置 ===

    // 设置 PBR 参数 (会自动设置对应的 uniform)
    void SetPBRParams(const PBRMaterialParams& params);

    // 获取当前 PBR 参数
    PBRMaterialParams GetPBRParams() const;

    // 快捷设置方法
    void SetAlbedo(const glm::vec3& color);
    void SetMetallic(float value);
    void SetRoughness(float value);
    void SetAO(float value);
    void SetEmissive(const glm::vec3& color, float intensity = 1.0f);

    // 设置 PBR 纹理 (自动设置对应的 use flag)
    void SetAlbedoMap(Texture* texture);
    void SetNormalMap(Texture* texture);
    void SetMetallicMap(Texture* texture);
    void SetRoughnessMap(Texture* texture);
    void SetAOMap(Texture* texture);
    void SetEmissiveMap(Texture* texture);

    // IBL 设置
    void SetIBL(TextureCube* irradianceMap, TextureCube* prefilterMap,
                Texture* brdfLUT, float intensity = 1.0f);
    void DisableIBL();

    // === 资产句柄 (用于序列化) ===

    AssetHandle ShaderHandle;
    // PBR 纹理句柄
    AssetHandle AlbedoMapHandle;
    AssetHandle NormalMapHandle;
    AssetHandle MetallicMapHandle;
    AssetHandle RoughnessMapHandle;
    AssetHandle AOMapHandle;
    AssetHandle EmissiveMapHandle;
    // 旧版兼容
    AssetHandle DiffuseMapHandle;   // 别名 -> AlbedoMapHandle
    AssetHandle SpecularMapHandle;  // 废弃

private:
    std::shared_ptr<Shader> m_Shader;
    std::unordered_map<std::string, UniformValue> m_Uniforms;
    std::unordered_map<uint32_t, Texture*> m_Textures;  // slot -> texture
    std::unordered_map<std::string, uint32_t> m_TextureSlotNames;  // name -> slot

    // PBR 参数缓存
    PBRMaterialParams m_PBRParams;

    // IBL 纹理 (使用 TextureCube)
    TextureCube* m_IrradianceMap = nullptr;
    TextureCube* m_PrefilterMap = nullptr;
    Texture* m_BrdfLUT = nullptr;
};

// ============================================================================
// 便捷工厂方法
// ============================================================================

// 创建默认 PBR 材质
inline Material CreateDefaultPBRMaterial(std::shared_ptr<Shader> pbrShader) {
    Material mat(pbrShader);
    mat.SetPBRParams(PBRMaterialParams{});
    return mat;
}

// 创建简单颜色 PBR 材质
inline Material CreatePBRMaterial(std::shared_ptr<Shader> pbrShader,
                                   const glm::vec3& albedo,
                                   float metallic = 0.0f,
                                   float roughness = 0.5f) {
    Material mat(pbrShader);
    PBRMaterialParams params;
    params.AlbedoColor = albedo;
    params.Metallic = metallic;
    params.Roughness = roughness;
    mat.SetPBRParams(params);
    return mat;
}

} // namespace GLRenderer
