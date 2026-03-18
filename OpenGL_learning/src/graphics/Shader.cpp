#include "Shader.h"
#include <fstream>
#include <sstream>

namespace GLRenderer {

// 统一格式构造函数
Shader::Shader(const std::filesystem::path& filepath) {
    // 从文件名提取着色器名称
    m_Name = filepath.stem().string();

    // 读取文件
    std::string source = ReadFile(filepath);
    if (source.empty()) {
        std::cerr << "[Shader] 无法读取着色器文件: " << filepath << std::endl;
        return;
    }

    // 解析着色器源码
    auto shaderSources = ParseShaderSource(source);

    if (shaderSources.find(GL_VERTEX_SHADER) == shaderSources.end() ||
        shaderSources.find(GL_FRAGMENT_SHADER) == shaderSources.end()) {
        std::cerr << "[Shader] 缺少顶点或片段着色器: " << filepath << std::endl;
        return;
    }

    // 编译着色器
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, shaderSources[GL_VERTEX_SHADER]);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, shaderSources[GL_FRAGMENT_SHADER]);

    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader) glDeleteShader(vertexShader);
        if (fragmentShader) glDeleteShader(fragmentShader);
        return;
    }

    // 链接程序
    if (!LinkProgram(vertexShader, fragmentShader)) {
        std::cerr << "[Shader] 链接失败: " << filepath << std::endl;
    }

    // 删除已链接的着色器
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GL_CHECK_ERROR();

    if (m_ID != 0) {
        std::cout << "[Shader] 加载成功: " << m_Name << std::endl;
    }
}

// 分离格式构造函数（向后兼容）
Shader::Shader(const std::filesystem::path& vertexPath,
               const std::filesystem::path& fragmentPath) {
    // 从顶点着色器路径提取名称
    std::string filename = vertexPath.stem().string();
    // 移除 _vertex 或 _vert 后缀
    if (filename.size() > 7 && filename.substr(filename.size() - 7) == "_vertex") {
        m_Name = filename.substr(0, filename.size() - 7);
    } else if (filename.size() > 5 && filename.substr(filename.size() - 5) == "_vert") {
        m_Name = filename.substr(0, filename.size() - 5);
    } else {
        m_Name = filename;
    }

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
    LinkProgram(vertexShader, fragmentShader);

    // 删除已链接的着色器
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GL_CHECK_ERROR();
}

Shader::~Shader() {
    if (m_ID != 0) {
        glDeleteProgram(m_ID);
        m_ID = 0;
    }
}

Shader::Shader(Shader&& other) noexcept
    : m_ID(other.m_ID),
      m_Name(std::move(other.m_Name)),
      m_UniformCache(std::move(other.m_UniformCache)),
      m_UniformBlockCache(std::move(other.m_UniformBlockCache)) {
    other.m_ID = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        // 释放当前资源
        if (m_ID != 0) {
            glDeleteProgram(m_ID);
        }
        // 转移所有权
        m_ID = other.m_ID;
        m_Name = std::move(other.m_Name);
        m_UniformCache = std::move(other.m_UniformCache);
        m_UniformBlockCache = std::move(other.m_UniformBlockCache);
        other.m_ID = 0;
    }
    return *this;
}

void Shader::Bind() const {
    glUseProgram(m_ID);
}

void Shader::Unbind() const {
    glUseProgram(0);
}

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
        std::string key = m_Name + "::" + name;
        if (!warned[key]) {
            // std::cerr << "[Shader:" << m_Name << "] uniform '" << name << "' 不存在或未使用" << std::endl;
            warned[key] = true;
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

void Shader::SetIntArray(const std::string& name, const int* values, uint32_t count) const {
    glUniform1iv(GetUniformLocation(name), count, values);
}

void Shader::BindUniformBlock(const std::string& blockName, uint32_t bindingPoint) const {
    // 先查缓存
    auto it = m_UniformBlockCache.find(blockName);
    GLuint blockIndex;

    if (it != m_UniformBlockCache.end()) {
        blockIndex = it->second;
    } else {
        // 查询 uniform block 索引
        blockIndex = glGetUniformBlockIndex(m_ID, blockName.c_str());
        if (blockIndex != GL_INVALID_INDEX) {
            m_UniformBlockCache[blockName] = blockIndex;
        }
    }

    // 绑定到指定绑定点
    if (blockIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(m_ID, blockIndex, bindingPoint);
    }
}

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

std::unordered_map<GLenum, std::string> Shader::ParseShaderSource(const std::string& source) {
    std::unordered_map<GLenum, std::string> shaderSources;

    const char* typeToken = "#type";
    size_t typeTokenLength = strlen(typeToken);
    size_t pos = source.find(typeToken, 0);

    while (pos != std::string::npos) {
        // 找到行尾
        size_t eol = source.find_first_of("\r\n", pos);
        if (eol == std::string::npos) {
            std::cerr << "[Shader] 解析错误：#type 后缺少换行" << std::endl;
            break;
        }

        // 提取着色器类型
        size_t begin = pos + typeTokenLength + 1;
        std::string typeStr = source.substr(begin, eol - begin);

        // 去除前后空格
        size_t start = typeStr.find_first_not_of(" \t");
        size_t end = typeStr.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            typeStr = typeStr.substr(start, end - start + 1);
        }

        GLenum type = ShaderTypeFromString(typeStr);
        if (type == 0) {
            std::cerr << "[Shader] 未知着色器类型: " << typeStr << std::endl;
            pos = source.find(typeToken, eol);
            continue;
        }

        // 找到着色器源码的开始位置
        size_t nextLinePos = source.find_first_not_of("\r\n", eol);
        if (nextLinePos == std::string::npos) {
            std::cerr << "[Shader] 着色器类型 '" << typeStr << "' 后没有源码" << std::endl;
            break;
        }

        // 找到下一个 #type 或文件结尾
        pos = source.find(typeToken, nextLinePos);

        // 提取着色器源码
        if (pos == std::string::npos) {
            shaderSources[type] = source.substr(nextLinePos);
        } else {
            shaderSources[type] = source.substr(nextLinePos, pos - nextLinePos);
        }
    }

    return shaderSources;
}

GLenum Shader::ShaderTypeFromString(const std::string& type) {
    if (type == "vertex" || type == "vert" || type == "vs") {
        return GL_VERTEX_SHADER;
    }
    if (type == "fragment" || type == "frag" || type == "fs" || type == "pixel") {
        return GL_FRAGMENT_SHADER;
    }
    if (type == "geometry" || type == "geom" || type == "gs") {
        return GL_GEOMETRY_SHADER;
    }
    return 0;
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
        char infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        const char* typeName = "未知";
        switch (type) {
            case GL_VERTEX_SHADER: typeName = "顶点"; break;
            case GL_FRAGMENT_SHADER: typeName = "片段"; break;
            case GL_GEOMETRY_SHADER: typeName = "几何"; break;
        }
        std::cerr << "[Shader:" << m_Name << "] " << typeName << "着色器编译失败:\n"
                  << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool Shader::LinkProgram(GLuint vertexShader, GLuint fragmentShader) {
    m_ID = glCreateProgram();
    glAttachShader(m_ID, vertexShader);
    glAttachShader(m_ID, fragmentShader);
    glLinkProgram(m_ID);

    // 检查链接错误
    int success;
    glGetProgramiv(m_ID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(m_ID, 1024, nullptr, infoLog);
        std::cerr << "[Shader:" << m_Name << "] 程序链接失败:\n" << infoLog << std::endl;
        glDeleteProgram(m_ID);
        m_ID = 0;
        return false;
    }

    return true;
}

void ShaderLibrary::Add(const std::string& name, std::shared_ptr<Shader> shader) {
    if (Exists(name)) {
        std::cerr << "[ShaderLibrary] 着色器已存在: " << name << std::endl;
        return;
    }
    m_Shaders[name] = shader;
}

void ShaderLibrary::Add(std::shared_ptr<Shader> shader) {
    if (!shader) {
        std::cerr << "[ShaderLibrary] 无法添加空着色器" << std::endl;
        return;
    }
    Add(shader->GetName(), shader);
}

std::shared_ptr<Shader> ShaderLibrary::Load(const std::filesystem::path& filepath) {
    std::string name = filepath.stem().string();
    return Load(name, filepath);
}

std::shared_ptr<Shader> ShaderLibrary::Load(const std::string& name, const std::filesystem::path& filepath) {
    if (Exists(name)) {
        std::cout << "[ShaderLibrary] 返回已缓存的着色器: " << name << std::endl;
        return m_Shaders[name];
    }

    auto shader = std::make_shared<Shader>(filepath);
    if (shader->IsValid()) {
        m_Shaders[name] = shader;
        return shader;
    }

    std::cerr << "[ShaderLibrary] 加载着色器失败: " << filepath << std::endl;
    return nullptr;
}

std::shared_ptr<Shader> ShaderLibrary::Get(const std::string& name) const {
    auto it = m_Shaders.find(name);
    if (it != m_Shaders.end()) {
        return it->second;
    }
    std::cerr << "[ShaderLibrary] 着色器不存在: " << name << std::endl;
    return nullptr;
}

bool ShaderLibrary::Exists(const std::string& name) const {
    return m_Shaders.find(name) != m_Shaders.end();
}

void ShaderLibrary::Remove(const std::string& name) {
    m_Shaders.erase(name);
}

void ShaderLibrary::Clear() {
    m_Shaders.clear();
}

} // namespace GLRenderer
