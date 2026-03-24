#pragma once

#include "utils/GLCommon.h"
#include "core/Ref.h"
#include <string>
#include <filesystem>
#include <unordered_map>

namespace GLRenderer {

class Shader : public RefCounter {
public:
    Shader() = default;

    explicit Shader(const std::filesystem::path& filepath);

    Shader(const std::filesystem::path& vertexPath,
           const std::filesystem::path& fragmentPath);

    ~Shader();

    void Bind() const;
    void Unbind() const;

    bool IsValid() const { return m_ID != 0; }
    operator bool() const { return IsValid(); }

    GLuint GetID() const { return m_ID; }
    const std::string& GetName() const { return m_Name; }

    void SetBool(const std::string& name, bool value) const;
    void SetInt(const std::string& name, int value) const;
    void SetIVec2(const std::string& name, const glm::ivec2& value) const;
    void SetFloat(const std::string& name, float value) const;

    void SetVec2(const std::string& name, const glm::vec2& value) const;
    void SetVec2(const std::string& name, float x, float y) const;

    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetVec3(const std::string& name, float x, float y, float z) const;

    void SetVec4(const std::string& name, const glm::vec4& value) const;
    void SetVec4(const std::string& name, float x, float y, float z, float w) const;

    void SetMat3(const std::string& name, const glm::mat3& mat) const;
    void SetMat4(const std::string& name, const glm::mat4& mat) const;

    void SetIntArray(const std::string& name, const int* values, uint32_t count) const;

    void BindUniformBlock(const std::string& blockName, uint32_t bindingPoint) const;

    static Ref<Shader> Create(const std::filesystem::path& filepath);
    static Ref<Shader> Create(const std::filesystem::path& vertexPath,
                              const std::filesystem::path& fragmentPath);

private:
    GLint GetUniformLocation(const std::string& name) const;
    GLuint CompileShader(GLenum type, const std::string& source);
    bool LinkProgram(GLuint vertexShader, GLuint fragmentShader);
    std::string ReadFile(const std::filesystem::path& path);
    std::unordered_map<GLenum, std::string> ParseShaderSource(const std::string& source);
    static GLenum ShaderTypeFromString(const std::string& type);

    GLuint m_ID = 0;
    std::string m_Name;
    mutable std::unordered_map<std::string, GLint> m_UniformCache;
    mutable std::unordered_map<std::string, GLuint> m_UniformBlockCache;
};

class ShaderLibrary : public RefCounter {
public:
    ShaderLibrary() = default;
    ~ShaderLibrary() = default;

    ShaderLibrary(const ShaderLibrary&) = delete;
    ShaderLibrary& operator=(const ShaderLibrary&) = delete;

    void Add(const std::string& name, Ref<Shader> shader);
    void Add(Ref<Shader> shader);

    Ref<Shader> Load(const std::filesystem::path& filepath);
    Ref<Shader> Load(const std::string& name, const std::filesystem::path& filepath);

    Ref<Shader> Get(const std::string& name) const;

    bool Exists(const std::string& name) const;

    const std::unordered_map<std::string, Ref<Shader>>& GetShaders() const {
        return m_Shaders;
    }

    void Remove(const std::string& name);
    void Clear();

private:
    std::unordered_map<std::string, Ref<Shader>> m_Shaders;
};

} // namespace GLRenderer
