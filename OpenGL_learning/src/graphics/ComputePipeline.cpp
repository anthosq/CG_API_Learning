#include "ComputePipeline.h"
#include <fstream>
#include <sstream>

namespace GLRenderer {

ComputePipeline::ComputePipeline(const std::filesystem::path& filepath) {
    m_Name = filepath.stem().string();

    std::string source = ReadFile(filepath);
    if (source.empty()) {
        std::cerr << "[ComputePipeline] 无法读取文件: " << filepath << std::endl;
        return;
    }

    if (!CompileAndLink(source)) {
        std::cerr << "[ComputePipeline] 编译失败: " << filepath << std::endl;
        return;
    }

    std::cout << "[ComputePipeline] 加载成功: " << m_Name << std::endl;
}

ComputePipeline::~ComputePipeline() {
    if (m_ProgramID != 0) {
        glDeleteProgram(m_ProgramID);
        m_ProgramID = 0;
    }
}

void ComputePipeline::Bind() const {
    glUseProgram(m_ProgramID);
}

void ComputePipeline::Unbind() const {
    glUseProgram(0);
}

void ComputePipeline::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) const {
    glDispatchCompute(groupsX, groupsY, groupsZ);
}

void ComputePipeline::DispatchAndWait(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) const {
    glDispatchCompute(groupsX, groupsY, groupsZ);
    Wait();
}

void ComputePipeline::Wait(GLbitfield barriers) {
    glMemoryBarrier(barriers);
}

void ComputePipeline::BindImageTexture(uint32_t unit, GLuint texture, int level,
                                        GLenum access, GLenum format) const {
    glBindImageTexture(unit, texture, level, GL_FALSE, 0, access, format);
}

void ComputePipeline::BindImageTextureCube(uint32_t unit, GLuint texture, int level,
                                            GLenum access, GLenum format) const {
    // layered = GL_TRUE 表示绑定所有层 (CubeMap 的 6 个面)
    glBindImageTexture(unit, texture, level, GL_TRUE, 0, access, format);
}

GLint ComputePipeline::GetUniformLocation(const std::string& name) const {
    auto it = m_UniformCache.find(name);
    if (it != m_UniformCache.end()) {
        return it->second;
    }

    GLint location = glGetUniformLocation(m_ProgramID, name.c_str());
    if (location == -1) {
        // 静态警告，避免重复输出
        static std::unordered_map<std::string, bool> warned;
        std::string key = m_Name + "::" + name;
        if (!warned[key]) {
            std::cerr << "[ComputePipeline:" << m_Name << "] Uniform 未找到: " << name << std::endl;
            warned[key] = true;
        }
    }
    m_UniformCache[name] = location;
    return location;
}

void ComputePipeline::SetInt(const std::string& name, int value) const {
    glUniform1i(GetUniformLocation(name), value);
}

void ComputePipeline::SetFloat(const std::string& name, float value) const {
    glUniform1f(GetUniformLocation(name), value);
}

void ComputePipeline::SetVec2(const std::string& name, const glm::vec2& value) const {
    glUniform2fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void ComputePipeline::SetVec3(const std::string& name, const glm::vec3& value) const {
    glUniform3fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void ComputePipeline::SetVec4(const std::string& name, const glm::vec4& value) const {
    glUniform4fv(GetUniformLocation(name), 1, glm::value_ptr(value));
}

void ComputePipeline::SetMat3(const std::string& name, const glm::mat3& value) const {
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

void ComputePipeline::SetMat4(const std::string& name, const glm::mat4& value) const {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

glm::ivec3 ComputePipeline::GetWorkGroupSize() const {
    glm::ivec3 size(0);
    glGetProgramiv(m_ProgramID, GL_COMPUTE_WORK_GROUP_SIZE, glm::value_ptr(size));
    return size;
}

Ref<ComputePipeline> ComputePipeline::Create(const std::filesystem::path& filepath) {
    return Ref<ComputePipeline>(new ComputePipeline(filepath));
}

std::string ComputePipeline::ReadFile(const std::filesystem::path& path) const {
    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try {
        file.open(path);
        std::stringstream stream;
        stream << file.rdbuf();
        file.close();
        return stream.str();
    } catch (const std::ifstream::failure& e) {
        std::cerr << "[ComputePipeline] 文件读取错误: " << path << "\n"
                  << "  " << e.what() << std::endl;
        return "";
    }
}

bool ComputePipeline::CompileAndLink(const std::string& source) {
    // 预处理: 移除 #type compute 标记 (如果存在)
    std::string processedSource = source;
    const std::string typeMarker = "#type";
    size_t pos = processedSource.find(typeMarker);
    if (pos != std::string::npos) {
        size_t eol = processedSource.find_first_of("\r\n", pos);
        if (eol != std::string::npos) {
            // 检查是否是 compute 类型
            std::string typeStr = processedSource.substr(pos + typeMarker.length(), eol - pos - typeMarker.length());
            // 移除 #type 行
            size_t nextLine = processedSource.find_first_not_of("\r\n", eol);
            if (nextLine != std::string::npos) {
                processedSource = processedSource.substr(nextLine);
            }
        }
    }

    // 创建 Compute Shader
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    const char* src = processedSource.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // 检查编译错误
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cerr << "[ComputePipeline:" << m_Name << "] 着色器编译失败:\n"
                  << infoLog << std::endl;
        glDeleteShader(shader);
        return false;
    }

    // 创建程序并链接
    m_ProgramID = glCreateProgram();
    glAttachShader(m_ProgramID, shader);
    glLinkProgram(m_ProgramID);

    // 检查链接错误
    glGetProgramiv(m_ProgramID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetProgramInfoLog(m_ProgramID, 1024, nullptr, infoLog);
        std::cerr << "[ComputePipeline:" << m_Name << "] 程序链接失败:\n"
                  << infoLog << std::endl;
        glDeleteProgram(m_ProgramID);
        m_ProgramID = 0;
        glDeleteShader(shader);
        return false;
    }

    // 清理 shader (已链接到程序)
    glDeleteShader(shader);
    // 编译和链接成功状态已通过 glGetShaderiv/glGetProgramiv 显式检查, 无需 GL_CHECK_ERROR
    return true;
}

} // namespace GLRenderer
