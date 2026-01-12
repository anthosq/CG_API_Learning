#include "Shader.h"
#include <fstream>
#include <sstream>

namespace GLRenderer {

Shader::Shader(const std::filesystem::path& vertexPath,
               const std::filesystem::path& fragmentPath) {
    // 读取着色器源码
    std::string vertexSource = ReadFile(vertexPath);
    std::string fragmentSource = ReadFile(fragmentPath);

    if (vertexSource.empty() || fragmentSource.empty()) {
        std::cerr << "[Shader] 无法读取着色器文件" << std::endl;
        return;
    }

    // 编译着色器
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);

    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader) glDeleteShader(vertexShader);
        if (fragmentShader) glDeleteShader(fragmentShader);
        return;
    }

    // 链接程序
    m_ID = glCreateProgram();
    glAttachShader(m_ID, vertexShader);
    glAttachShader(m_ID, fragmentShader);
    glLinkProgram(m_ID);

    // 检查链接错误
    int success;
    glGetProgramiv(m_ID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_ID, 512, nullptr, infoLog);
        std::cerr << "[Shader] 程序链接失败:\n" << infoLog << std::endl;
        glDeleteProgram(m_ID);
        m_ID = 0;
    }

    // 删除已链接的着色器
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 兼容旧代码
    shader_id = m_ID;

    GL_CHECK_ERROR();
}

Shader::~Shader() {
    if (m_ID != 0) {
        glDeleteProgram(m_ID);
        m_ID = 0;
        shader_id = 0;
    }
}

Shader::Shader(Shader&& other) noexcept
    : m_ID(other.m_ID),
      m_UniformCache(std::move(other.m_UniformCache)) {
    shader_id = m_ID;
    other.m_ID = 0;
    other.shader_id = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        // 释放当前资源
        if (m_ID != 0) {
            glDeleteProgram(m_ID);
        }
        // 转移所有权
        m_ID = other.m_ID;
        shader_id = m_ID;
        m_UniformCache = std::move(other.m_UniformCache);
        other.m_ID = 0;
        other.shader_id = 0;
    }
    return *this;
}

void Shader::Bind() const {
    glUseProgram(m_ID);
}

void Shader::Unbind() const {
    glUseProgram(0);
}

// ============================================================================
// Uniform 设置方法
// ============================================================================

GLint Shader::GetUniformLocation(const std::string& name) const {
    // 先查缓存
    auto it = m_UniformCache.find(name);
    if (it != m_UniformCache.end()) {
        return it->second;
    }

    // 查询并缓存
    GLint location = glGetUniformLocation(m_ID, name.c_str());
    if (location == -1) {
        // 仅在首次警告，避免刷屏
        static std::unordered_map<std::string, bool> warned;
        if (!warned[name]) {
            std::cerr << "[Shader] uniform '" << name << "' 不存在或未使用" << std::endl;
            warned[name] = true;
        }
    }
    m_UniformCache[name] = location;
    return location;
}

void Shader::SetBool(const std::string& name, bool value) const {
    glUniform1i(GetUniformLocation(name), static_cast<int>(value));
}

void Shader::SetInt(const std::string& name, int value) const {
    glUniform1i(GetUniformLocation(name), value);
}

void Shader::SetFloat(const std::string& name, float value) const {
    glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetVec2(const std::string& name, const glm::vec2& value) const {
    glUniform2fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetVec2(const std::string& name, float x, float y) const {
    glUniform2f(GetUniformLocation(name), x, y);
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) const {
    glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetVec3(const std::string& name, float x, float y, float z) const {
    glUniform3f(GetUniformLocation(name), x, y, z);
}

void Shader::SetVec4(const std::string& name, const glm::vec4& value) const {
    glUniform4fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::SetVec4(const std::string& name, float x, float y, float z, float w) const {
    glUniform4f(GetUniformLocation(name), x, y, z, w);
}

void Shader::SetMat3(const std::string& name, const glm::mat3& mat) const {
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::SetMat4(const std::string& name, const glm::mat4& mat) const {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
}

// ============================================================================
// 私有辅助方法
// ============================================================================

std::string Shader::ReadFile(const std::filesystem::path& path) {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        file.open(path);
        std::stringstream stream;
        stream << file.rdbuf();
        file.close();
        return stream.str();
    } catch (const std::ifstream::failure& e) {
        std::cerr << "[Shader] 无法读取文件: " << path << "\n"
                  << "  错误: " << e.what() << std::endl;
        return "";
    }
}

GLuint Shader::CompileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // 检查编译错误
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        const char* typeName = (type == GL_VERTEX_SHADER) ? "顶点" : "片段";
        std::cerr << "[Shader] " << typeName << "着色器编译失败:\n"
                  << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

} // namespace GLRenderer
