#include "Texture.h"
#include "utils/DDSLoader.h"
#include <stb_image/stb_image.h>
#include <algorithm>

namespace GLRenderer {

Texture2D::Texture2D(const std::filesystem::path& path, const TextureSpec& spec) {
    m_Path = path.string();

    bool success = false;

    if (IsDDSFile(path)) {
        success = LoadFromDDS(path);
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

bool Texture2D::LoadFromImage(const std::filesystem::path& path, const TextureSpec& spec) {
    stbi_set_flip_vertically_on_load(spec.FlipVertically);

    int channels;
    unsigned char* data = stbi_load(path.string().c_str(),
                                     &m_Width, &m_Height, &channels, 0);

    if (!data) {
        std::cerr << "[Texture2D] stbi_load 失败: " << path << std::endl;
        return false;
    }

    GLenum format = GL_RGBA;
    GLenum internalFormat = GL_RGBA8;

    switch (channels) {
        case 1:
            format = GL_RED;
            internalFormat = GL_R8;
            break;
        case 2:
            format = GL_RG;
            internalFormat = GL_RG8;
            break;
        case 3:
            format = GL_RGB;
            internalFormat = GL_RGB8;
            break;
        case 4:
            format = GL_RGBA;
            internalFormat = GL_RGBA8;
            break;
    }

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
        ApplyParameters(spec);
    }
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

bool TextureCube::LoadCubeMap(const std::array<std::filesystem::path, 6>& faces) {
    glGenTextures(1, &m_ID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_ID);

    int nrChannels;
    GLenum format;

    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].string().c_str(),
                                         &m_Width, &m_Height, &nrChannels, 0);
        if (data) {
            switch (nrChannels) {
                case 1: format = GL_RED; break;
                case 3: format = GL_RGB; break;
                case 4: format = GL_RGBA; break;
                default: format = GL_RGB;
            }
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format,
                         m_Width, m_Height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
            std::cerr << "[TextureCube] Failed to load texture: " << faces[i] << std::endl;
            return false;
        }
        m_Paths[i] = faces[i];
    }

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
