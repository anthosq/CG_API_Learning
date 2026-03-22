#pragma once

#include "utils/GLCommon.h"
#include "asset/Asset.h"
#include <string>
#include <filesystem>
#include <array>

namespace GLRenderer {

struct TextureSpec {
    GLenum WrapS = GL_REPEAT;
    GLenum WrapT = GL_REPEAT;
    GLenum MinFilter = GL_LINEAR_MIPMAP_LINEAR;
    GLenum MagFilter = GL_LINEAR;
    bool GenerateMipmaps = true;
    bool FlipVertically = true;
    bool IsHDR = false;  // 自动检测, 通常不需要手动设置
    bool SRGB = false;   // 颜色纹理(albedo)设为 true, 让 GPU 自动线性化采样值
};

struct TextureCubeSpec {
    GLenum WrapS = GL_CLAMP_TO_EDGE;
    GLenum WrapT = GL_CLAMP_TO_EDGE;
    GLenum WrapR = GL_CLAMP_TO_EDGE;
    GLenum MinFilter = GL_LINEAR;
    GLenum MagFilter = GL_LINEAR;
    GLenum InternalFormat = GL_RGB8;  // 可以是 GL_RGB16F 或 GL_RGB32F (HDR)
    bool GenerateMipmaps = true;
    bool FlipVertically = false;
};

class Texture2D : public Asset {
public:
    Texture2D() = default;

    explicit Texture2D(const std::filesystem::path& path,
                       const TextureSpec& spec = TextureSpec());

    Texture2D(const void* data, int width, int height,
              GLenum format = GL_RGBA, GLenum internalFormat = GL_RGBA8,
              const TextureSpec& spec = TextureSpec());

    ~Texture2D();

    void Bind(uint32_t slot = 0) const;
    void Unbind() const;

    GLuint GetID() const { return m_ID; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    int GetChannels() const { return m_Channels; }
    const std::string& GetPath() const { return m_Path; }
    bool IsHDR() const { return m_IsHDR; }

    bool IsValid() const { return m_ID != 0; }
    operator bool() const { return IsValid(); }

    static AssetType GetStaticType() { return AssetType::Texture; }
    AssetType GetAssetType() const override { return GetStaticType(); }

    static Ref<Texture2D> Create(const std::filesystem::path& path,
                                  const TextureSpec& spec = TextureSpec());
    static Ref<Texture2D> Create(const void* data, int width, int height,
                                  GLenum format = GL_RGBA, GLenum internalFormat = GL_RGBA8,
                                  const TextureSpec& spec = TextureSpec());
    // 从浮点数据创建 HDR 纹理
    static Ref<Texture2D> CreateHDR(const float* data, int width, int height,
                                     int channels, const TextureSpec& spec = TextureSpec());
    // 从嵌入的压缩图像数据（PNG/JPG 字节流）创建纹理
    static Ref<Texture2D> CreateFromMemory(const unsigned char* compressedData, int dataSize,
                                            const TextureSpec& spec = TextureSpec());
    static Ref<Texture2D> CreateDefault();

    static bool IsDDSFile(const std::filesystem::path& path);
    static bool IsHDRFile(const std::filesystem::path& path);

private:
    bool LoadFromImage(const std::filesystem::path& path, const TextureSpec& spec);
    bool LoadFromHDR(const std::filesystem::path& path, const TextureSpec& spec);
    bool LoadFromDDS(const std::filesystem::path& path);
    void ApplyParameters(const TextureSpec& spec);

    GLuint m_ID = 0;
    int m_Width = 0;
    int m_Height = 0;
    int m_Channels = 0;
    bool m_IsHDR = false;
    std::string m_Path;
};

using Texture = Texture2D;

class TextureCube : public Asset {
public:
    TextureCube() = default;

    // 从 6 个面的图像文件创建
    explicit TextureCube(std::array<std::filesystem::path, 6>& paths,
                         const TextureCubeSpec& spec = TextureCubeSpec());

    // 从统一数据创建 (每个面相同)
    TextureCube(const void* data, int width, int height,
                GLenum format = GL_RGBA, GLenum internalFormat = GL_RGBA8,
                const TextureCubeSpec& spec = TextureCubeSpec());

    ~TextureCube();

    void Bind(uint32_t slot = 0) const;
    void Unbind() const;

    // 生成 Mipmap
    void GenerateMipmaps() const;

    GLuint GetID() const { return m_ID; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    int GetMipLevels() const { return m_MipLevels; }
    GLenum GetInternalFormat() const { return m_InternalFormat; }
    const std::array<std::filesystem::path, 6>& GetPaths() const { return m_Paths; }

    bool IsValid() const { return m_ID != 0; }
    operator bool() const { return IsValid(); }

    static AssetType GetStaticType() { return AssetType::TextureCube; }
    AssetType GetAssetType() const override { return GetStaticType(); }

    // 从 6 个面的图像创建
    static Ref<TextureCube> Create(std::array<std::filesystem::path, 6>& paths,
                                    const TextureCubeSpec& spec = TextureCubeSpec());

    // 创建空的 CubeMap (用于 Compute Shader 输出)
    // resolution: 每个面的分辨率 (width = height)
    // internalFormat: GL_RGBA16F, GL_RGBA32F 等
    // mipLevels: 0 = 自动计算最大 mip 级别
    static Ref<TextureCube> CreateEmpty(uint32_t resolution, GLenum internalFormat = GL_RGBA16F,
                                         uint32_t mipLevels = 0);

private:
    bool LoadCubeMap(const std::array<std::filesystem::path, 6>& faces);
    void ApplyParameters(const TextureCubeSpec& spec);

    GLuint m_ID = 0;
    int m_Width = 0;
    int m_Height = 0;
    int m_MipLevels = 1;
    GLenum m_InternalFormat = GL_RGBA8;
    std::array<std::filesystem::path, 6> m_Paths;
};

} // namespace GLRenderer

using GLRenderer::TextureSpec;
