#pragma once

#include "utils/GLCommon.h"
#include <string>
#include <filesystem>

namespace GLRenderer {

// ============================================================================
// TextureSpec - 纹理配置
// ============================================================================
struct TextureSpec {
    GLenum WrapS = GL_REPEAT;
    GLenum WrapT = GL_REPEAT;
    GLenum MinFilter = GL_LINEAR_MIPMAP_LINEAR;
    GLenum MagFilter = GL_LINEAR;
    bool GenerateMipmaps = true;
    bool FlipVertically = true;  // stb_image 默认需要翻转
};

// ============================================================================
// Texture - 2D 纹理封装
// ============================================================================
class Texture : public NonCopyable {
public:
    Texture() = default;

    // 从文件加载纹理（自动检测格式）
    explicit Texture(const std::filesystem::path& path,
                     const TextureSpec& spec = TextureSpec());

    // 从内存数据创建纹理
    Texture(const void* data, int width, int height,
            GLenum format = GL_RGBA, GLenum internalFormat = GL_RGBA8,
            const TextureSpec& spec = TextureSpec());

    ~Texture();

    // 移动语义
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // 绑定到指定纹理单元
    void Bind(uint32_t slot = 0) const;
    void Unbind() const;

    // 属性访问
    GLuint GetID() const { return m_ID; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    const std::string& GetPath() const { return m_Path; }

    bool IsValid() const { return m_ID != 0; }
    operator bool() const { return IsValid(); }

    // 静态方法：创建默认纹理（棋盘格）
    static Texture CreateDefault();

    // 静态方法：检查文件是否为 DDS 格式
    static bool IsDDSFile(const std::filesystem::path& path);

private:
    // 从普通图片文件加载（PNG, JPG 等）
    bool LoadFromImage(const std::filesystem::path& path, const TextureSpec& spec);

    // 从 DDS 文件加载
    bool LoadFromDDS(const std::filesystem::path& path);

    // 应用纹理参数
    void ApplyParameters(const TextureSpec& spec);

    GLuint m_ID = 0;
    int m_Width = 0;
    int m_Height = 0;
    std::string m_Path;
};

} // namespace GLRenderer

// 全局命名空间别名
using GLRenderer::Texture;
using GLRenderer::TextureSpec;
