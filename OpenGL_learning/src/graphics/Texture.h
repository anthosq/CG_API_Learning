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
};

struct TextureCubeSpec {
    GLenum WrapS = GL_CLAMP_TO_EDGE;
    GLenum WrapT = GL_CLAMP_TO_EDGE;
    GLenum WrapR = GL_CLAMP_TO_EDGE;
    GLenum MinFilter = GL_LINEAR;
    GLenum MagFilter = GL_LINEAR;
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
    const std::string& GetPath() const { return m_Path; }

    bool IsValid() const { return m_ID != 0; }
    operator bool() const { return IsValid(); }

    static AssetType GetStaticType() { return AssetType::Texture; }
    AssetType GetAssetType() const override { return GetStaticType(); }

    static Ref<Texture2D> Create(const std::filesystem::path& path,
                                  const TextureSpec& spec = TextureSpec());
    static Ref<Texture2D> Create(const void* data, int width, int height,
                                  GLenum format = GL_RGBA, GLenum internalFormat = GL_RGBA8,
                                  const TextureSpec& spec = TextureSpec());
    static Ref<Texture2D> CreateDefault();

    static bool IsDDSFile(const std::filesystem::path& path);

private:
    bool LoadFromImage(const std::filesystem::path& path, const TextureSpec& spec);
    bool LoadFromDDS(const std::filesystem::path& path);
    void ApplyParameters(const TextureSpec& spec);

    GLuint m_ID = 0;
    int m_Width = 0;
    int m_Height = 0;
    std::string m_Path;
};

using Texture = Texture2D;

class TextureCube : public Asset {
public:
    TextureCube() = default;

    explicit TextureCube(std::array<std::filesystem::path, 6>& paths,
                         const TextureCubeSpec& spec = TextureCubeSpec());

    TextureCube(const void* data, int width, int height,
                GLenum format = GL_RGBA, GLenum internalFormat = GL_RGBA8,
                const TextureCubeSpec& spec = TextureCubeSpec());

    ~TextureCube();

    void Bind(uint32_t slot = 0) const;
    void Unbind() const;

    GLuint GetID() const { return m_ID; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    const std::array<std::filesystem::path, 6>& GetPaths() const { return m_Paths; }

    bool IsValid() const { return m_ID != 0; }
    operator bool() const { return IsValid(); }

    static AssetType GetStaticType() { return AssetType::TextureCube; }
    AssetType GetAssetType() const override { return GetStaticType(); }

    static Ref<TextureCube> Create(std::array<std::filesystem::path, 6>& paths,
                                    const TextureCubeSpec& spec = TextureCubeSpec());

private:
    bool LoadCubeMap(const std::array<std::filesystem::path, 6>& faces);
    void ApplyParameters(const TextureCubeSpec& spec);

    GLuint m_ID = 0;
    int m_Width = 0;
    int m_Height = 0;
    std::array<std::filesystem::path, 6> m_Paths;
};

} // namespace GLRenderer

using GLRenderer::TextureSpec;
