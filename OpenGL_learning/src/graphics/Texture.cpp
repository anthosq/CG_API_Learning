#include "Texture.h"
#include "utils/DDSLoader.h"
#include <stb_image/stb_image.h>
#include <algorithm>

namespace GLRenderer {

namespace Utils {

// 根据通道数获取 LDR 纹理格式
inline void GetLDRTextureFormat(int channels, GLenum& outFormat, GLenum& outInternalFormat) {
    switch (channels) {
        case 1:
            outFormat = GL_RED;
            outInternalFormat = GL_R8;
            break;
        case 2:
            outFormat = GL_RG;
            outInternalFormat = GL_RG8;
            break;
        case 3:
            outFormat = GL_RGB;
            outInternalFormat = GL_RGB8;
            break;
        case 4:
        default:
            outFormat = GL_RGBA;
            outInternalFormat = GL_RGBA8;
            break;
    }
}

// 根据通道数获取 HDR 纹理格式 (16F)
inline void GetHDRTextureFormat(int channels, GLenum& outFormat, GLenum& outInternalFormat) {
    switch (channels) {
        case 1:
            outFormat = GL_RED;
            outInternalFormat = GL_R16F;
            break;
        case 2:
            outFormat = GL_RG;
            outInternalFormat = GL_RG16F;
            break;
        case 3:
            outFormat = GL_RGB;
            outInternalFormat = GL_RGB16F;
            break;
        case 4:
        default:
            outFormat = GL_RGBA;
            outInternalFormat = GL_RGBA16F;
            break;
    }
}

} // namespace Utils

Texture2D::Texture2D(const std::filesystem::path& path, const TextureSpec& spec) {
    m_Path = path.string();

    bool success = false;

    if (IsDDSFile(path)) {
        success = LoadFromDDS(path);
    } else if (IsHDRFile(path)) {
        success = LoadFromHDR(path, spec);
    } else {
        success = LoadFromImage(path, spec);
    }

    if (!success) {
        std::cerr << "[Texture2D] 加载失败: " << path << std::endl;
    }
}

Texture2D::Texture2D(const void* data, int width, int height,
                     GLenum format, GLenum internalFormat,
                     const TextureSpec& spec)
    : m_Width(width), m_Height(height) {
    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_2D, m_ID);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                 format, GL_UNSIGNED_BYTE, data);

    ApplyParameters(spec);

    if (spec.GenerateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    GL_CHECK_ERROR();
}

Texture2D::~Texture2D() {
    if (m_ID != 0) {
        glDeleteTextures(1, &m_ID);
        m_ID = 0;
    }
}

void Texture2D::Bind(uint32_t slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_ID);
}

void Texture2D::Unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

Ref<Texture2D> Texture2D::Create(const std::filesystem::path& path, const TextureSpec& spec) {
    return Ref<Texture2D>(new Texture2D(path, spec));
}

Ref<Texture2D> Texture2D::Create(const void* data, int width, int height,
                                  GLenum format, GLenum internalFormat,
                                  const TextureSpec& spec) {
    return Ref<Texture2D>(new Texture2D(data, width, height, format, internalFormat, spec));
}

Ref<Texture2D> Texture2D::CreateDefault() {
    const int size = 8;
    unsigned char data[size * size * 3];

    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int index = (i * size + j) * 3;
            bool isPink = ((i / 2) + (j / 2)) % 2 == 0;
            data[index + 0] = isPink ? 255 : 0;
            data[index + 1] = isPink ? 0 : 0;
            data[index + 2] = isPink ? 255 : 0;
        }
    }

    TextureSpec spec;
    spec.MinFilter = GL_NEAREST;
    spec.MagFilter = GL_NEAREST;
    spec.GenerateMipmaps = false;

    return Ref<Texture2D>(new Texture2D(data, size, size, GL_RGB, GL_RGB8, spec));
}

bool Texture2D::IsDDSFile(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".dds";
}

bool Texture2D::IsHDRFile(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".hdr" || ext == ".exr";
}

bool Texture2D::LoadFromImage(const std::filesystem::path& path, const TextureSpec& spec) {
    stbi_set_flip_vertically_on_load(spec.FlipVertically);

    unsigned char* data = stbi_load(path.string().c_str(),
                                     &m_Width, &m_Height, &m_Channels, 0);

    if (!data) {
        std::cerr << "[Texture2D] stbi_load 失败: " << path << std::endl;
        return false;
    }

    GLenum format, internalFormat;
    Utils::GetLDRTextureFormat(m_Channels, format, internalFormat);

    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_2D, m_ID);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                 m_Width, m_Height, 0,
                 format, GL_UNSIGNED_BYTE, data);

    ApplyParameters(spec);

    if (spec.GenerateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    stbi_image_free(data);

    GL_CHECK_ERROR();
    return true;
}

bool Texture2D::LoadFromHDR(const std::filesystem::path& path, const TextureSpec& spec) {
    stbi_set_flip_vertically_on_load(spec.FlipVertically);

    // 加载 HDR 图像 (浮点数据)
    float* data = stbi_loadf(path.string().c_str(), &m_Width, &m_Height, &m_Channels, 0);

    if (!data) {
        std::cerr << "[Texture2D] stbi_loadf HDR 失败: " << path << std::endl;
        return false;
    }

    m_IsHDR = true;

    GLenum format, internalFormat;
    Utils::GetHDRTextureFormat(m_Channels, format, internalFormat);

    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_2D, m_ID);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                 m_Width, m_Height, 0,
                 format, GL_FLOAT, data);

    ApplyParameters(spec);

    if (spec.GenerateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    stbi_image_free(data);

    GL_CHECK_ERROR();
    std::cout << "[Texture2D] HDR 加载成功: " << path.filename().string()
              << " (" << m_Width << "x" << m_Height << ", " << m_Channels << " channels)" << std::endl;
    return true;
}

bool Texture2D::LoadFromDDS(const std::filesystem::path& path) {
    m_ID = DDSLoader::LoadDDS(path.string());

    if (m_ID == 0) {
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, m_ID);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &m_Width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &m_Height);

    return true;
}

Ref<Texture2D> Texture2D::CreateHDR(const float* data, int width, int height,
                                     int channels, const TextureSpec& spec) {
    auto texture = Ref<Texture2D>(new Texture2D());
    texture->m_Width = width;
    texture->m_Height = height;
    texture->m_Channels = channels;
    texture->m_IsHDR = true;

    GLenum format, internalFormat;
    Utils::GetHDRTextureFormat(channels, format, internalFormat);

    glGenTextures(1, &texture->m_ID);
    glBindTexture(GL_TEXTURE_2D, texture->m_ID);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
                 width, height, 0,
                 format, GL_FLOAT, data);

    texture->ApplyParameters(spec);

    if (spec.GenerateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    GL_CHECK_ERROR();
    return texture;
}

void Texture2D::ApplyParameters(const TextureSpec& spec) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, spec.WrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, spec.WrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, spec.MinFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, spec.MagFilter);
}

TextureCube::TextureCube(std::array<std::filesystem::path, 6>& paths,
                         const TextureCubeSpec& spec) {
    stbi_set_flip_vertically_on_load(spec.FlipVertically);
    bool success = LoadCubeMap(paths);
    if (!success) {
        std::cerr << "[TextureCube] 加载立方体纹理失败" << std::endl;
    } else {
        if (spec.GenerateMipmaps) {
            GenerateMipmaps();
        }
        ApplyParameters(spec);
    }
}

TextureCube::TextureCube(const void* data, int width, int height,
                         GLenum format, GLenum internalFormat,
                         const TextureCubeSpec& spec)
    : m_Width(width), m_Height(height), m_InternalFormat(internalFormat) {
    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_ID);

    for (int i = 0; i < 6; i++) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat,
                     width, height, 0, format, GL_UNSIGNED_BYTE, data);
    }

    if (spec.GenerateMipmaps) {
        GenerateMipmaps();
    }
    ApplyParameters(spec);
    GL_CHECK_ERROR();
}

TextureCube::~TextureCube() {
    if (m_ID != 0) {
        glDeleteTextures(1, &m_ID);
        m_ID = 0;
    }
}

void TextureCube::Bind(uint32_t slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_ID);
}

void TextureCube::Unbind() const {
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

Ref<TextureCube> TextureCube::Create(std::array<std::filesystem::path, 6>& paths,
                                      const TextureCubeSpec& spec) {
    return Ref<TextureCube>(new TextureCube(paths, spec));
}

Ref<TextureCube> TextureCube::CreateEmpty(uint32_t resolution, GLenum internalFormat, uint32_t mipLevels) {
    auto cube = Ref<TextureCube>(new TextureCube());
    cube->m_Width = resolution;
    cube->m_Height = resolution;
    cube->m_InternalFormat = internalFormat;

    // 确定格式 (对应 internalFormat 的基础格式)
    GLenum format = GL_RGBA;
    if (internalFormat == GL_RGB16F || internalFormat == GL_RGB32F || internalFormat == GL_RGB8) {
        format = GL_RGB;
    }

    // 计算 mip 级别数
    if (mipLevels == 0) {
        // 最大 mip 级别: log2(resolution) + 1
        uint32_t r = resolution;
        mipLevels = 1;
        while (r > 1) { r >>= 1; mipLevels++; }
    }
    cube->m_MipLevels = static_cast<int>(mipLevels);

    glGenTextures(1, &cube->m_ID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube->m_ID);

    // 分配存储空间 (glTexStorage2D 分配不可变存储, 效率更高)
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, mipLevels, internalFormat, resolution, resolution);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                    mipLevels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GL_CHECK_ERROR();
    return cube;
}

void TextureCube::GenerateMipmaps() const {
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_ID);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    // 更新 mip 级别数
    const_cast<TextureCube*>(this)->m_MipLevels = 1;
    int r = m_Width;
    while (r > 1) { r >>= 1; const_cast<TextureCube*>(this)->m_MipLevels++; }
}

bool TextureCube::LoadCubeMap(const std::array<std::filesystem::path, 6>& faces) {
    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_ID);

    int nrChannels;

    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].string().c_str(),
                                         &m_Width, &m_Height, &nrChannels, 0);
        if (data) {
            GLenum format, internalFormat;
            Utils::GetLDRTextureFormat(nrChannels, format, internalFormat);

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat,
                         m_Width, m_Height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
            std::cerr << "[TextureCube] Failed to load texture: " << faces[i] << std::endl;
            return false;
        }
        m_Paths[i] = faces[i];
    }

    m_InternalFormat = GL_RGB8;  // 默认 LDR 格式
    return true;
}

void TextureCube::ApplyParameters(const TextureCubeSpec& spec) {
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, spec.WrapS);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, spec.WrapT);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, spec.WrapR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, spec.MinFilter);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, spec.MagFilter);
}

} // namespace GLRenderer
