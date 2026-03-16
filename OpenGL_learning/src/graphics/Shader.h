#pragma once

#include "utils/GLCommon.h"
#include <string>
#include <filesystem>
#include <unordered_map>

namespace GLRenderer {

// Shader - 着色器程序封装
class Shader : public NonCopyable {
public:
    Shader() = default;

    // 从文件加载着色器
    Shader(const std::filesystem::path& vertexPath,
           const std::filesystem::path& fragmentPath);

    ~Shader();

    // 移动语义
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    // 激活着色器程序
    void Bind() const;
    void Unbind() const;

    // 检查着色器是否有效
    bool IsValid() const { return m_ID != 0; }
    operator bool() const { return IsValid(); }

    // 获取程序 ID
    GLuint GetID() const { return m_ID; }

    // Uniform 设置方法 (带缓存，const 因为只修改 GPU 状态)
    void SetBool(const std::string& name, bool value) const;
    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;

    void SetVec2(const std::string& name, const glm::vec2& value) const;
    void SetVec2(const std::string& name, float x, float y) const;

    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetVec3(const std::string& name, float x, float y, float z) const;

    void SetVec4(const std::string& name, const glm::vec4& value) const;
    void SetVec4(const std::string& name, float x, float y, float z, float w) const;

    void SetMat3(const std::string& name, const glm::mat3& mat) const;
    void SetMat4(const std::string& name, const glm::mat4& mat) const;


private:
    // 获取 uniform 位置 (带缓存)
    GLint GetUniformLocation(const std::string& name) const;

    // 编译着色器
    GLuint CompileShader(GLenum type, const std::string& source);

    // 读取着色器文件
    std::string ReadFile(const std::filesystem::path& path);

    GLuint m_ID = 0;
    mutable std::unordered_map<std::string, GLint> m_UniformCache;
};
} 
