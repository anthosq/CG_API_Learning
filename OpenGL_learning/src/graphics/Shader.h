#pragma once

#include "utils/GLCommon.h"
#include <string>
#include <filesystem>
#include <unordered_map>
#include <memory>

namespace GLRenderer {

// 支持两种加载方式：
// 1. 统一格式：单个文件包含所有着色器阶段，使用 #type 指令分隔
// 2. 分离格式：顶点和片段着色器分别在不同文件中（向后兼容）
//
// 统一格式示例：
//   #type vertex
//   #version 330 core
//   void main() { ... }
//
//   #type fragment
//   #version 330 core
//   void main() { ... }

class Shader : public NonCopyable {
public:
    Shader() = default;

    // 统一格式：从单个文件加载（推荐）
    explicit Shader(const std::filesystem::path& filepath);

    // 分离格式：从两个文件加载（向后兼容）
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

    // 获取着色器名称
    const std::string& GetName() const { return m_Name; }

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

    // 设置 int 数组
    void SetIntArray(const std::string& name, const int* values, uint32_t count) const;

    // UBO 绑定点设置
    // @param blockName GLSL 中 uniform block 的名称
    // @param bindingPoint 绑定点索引 (对应 layout(binding = N))
    void BindUniformBlock(const std::string& blockName, uint32_t bindingPoint) const;

private:
    // 获取 uniform 位置 (带缓存)
    GLint GetUniformLocation(const std::string& name) const;

    // 编译着色器
    GLuint CompileShader(GLenum type, const std::string& source);

    // 链接程序
    bool LinkProgram(GLuint vertexShader, GLuint fragmentShader);

    // 读取着色器文件
    std::string ReadFile(const std::filesystem::path& path);

    // 解析统一格式着色器
    std::unordered_map<GLenum, std::string> ParseShaderSource(const std::string& source);

    // 着色器类型字符串转换
    static GLenum ShaderTypeFromString(const std::string& type);

    GLuint m_ID = 0;
    std::string m_Name;
    mutable std::unordered_map<std::string, GLint> m_UniformCache;
    mutable std::unordered_map<std::string, GLuint> m_UniformBlockCache;
};

// 集中管理所有着色器，避免重复加载，提供便捷访问

class ShaderLibrary {
public:
    ShaderLibrary() = default;
    ~ShaderLibrary() = default;

    // 禁止拷贝
    ShaderLibrary(const ShaderLibrary&) = delete;
    ShaderLibrary& operator=(const ShaderLibrary&) = delete;

    // 添加已有着色器
    void Add(const std::string& name, std::shared_ptr<Shader> shader);
    void Add(std::shared_ptr<Shader> shader);  // 使用着色器自己的名称

    // 从文件加载着色器
    std::shared_ptr<Shader> Load(const std::filesystem::path& filepath);
    std::shared_ptr<Shader> Load(const std::string& name, const std::filesystem::path& filepath);

    // 获取着色器
    std::shared_ptr<Shader> Get(const std::string& name) const;

    // 检查着色器是否存在
    bool Exists(const std::string& name) const;

    // 获取所有着色器
    const std::unordered_map<std::string, std::shared_ptr<Shader>>& GetShaders() const {
        return m_Shaders;
    }

    // 移除着色器
    void Remove(const std::string& name);

    // 清空所有着色器
    void Clear();

private:
    std::unordered_map<std::string, std::shared_ptr<Shader>> m_Shaders;
};

} // namespace GLRenderer
