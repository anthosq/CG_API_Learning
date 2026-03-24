#pragma once

#include "utils/GLCommon.h"
#include "core/Ref.h"
#include "Texture.h"
#include <string>
#include <filesystem>
#include <unordered_map>

namespace GLRenderer {

// ComputePipeline - 计算管线
// 用于管理 Compute Shader 的加载、绑定和调度
//
// 使用场景:
// - IBL 环境贴图预处理
// - Forward+ 光源剔除
// - 粒子模拟
// - Photon Mapping
//
// 示例:
//   auto pipeline = ComputePipeline::Create("assets/shaders/compute/irradiance.glsl");
//   pipeline->Bind();
//   pipeline->SetInt("u_Samples", 64);
//   pipeline->BindImageTexture(0, outputCubemap, 0, GL_WRITE_ONLY, GL_RGBA16F);
//   pipeline->Dispatch(32, 32, 6);  // 处理 CubeMap 的 6 个面
//   pipeline->Wait();  // 等待计算完成

class ComputePipeline : public RefCounter {
public:
    ComputePipeline() = default;
    explicit ComputePipeline(const std::filesystem::path& filepath);
    ~ComputePipeline();

    // 禁止拷贝
    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;

    // 绑定/解绑计算管线
    void Bind() const;
    void Unbind() const;

    // 调度计算着色器
    // groupsX/Y/Z: 工作组数量 (总线程数 = groups * local_size)
    void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) const;

    // 调度并等待完成 (插入内存屏障)
    void DispatchAndWait(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) const;

    // 等待所有计算完成 (glMemoryBarrier)
    static void Wait(GLbitfield barriers = GL_ALL_BARRIER_BITS);

    // 绑定图像纹理 (用于 image load/store)
    // unit: 绑定点 (对应 layout(binding = unit) 在 shader 中)
    // texture: 纹理 ID
    // level: mipmap 级别
    // access: GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE
    // format: 内部格式 (如 GL_RGBA16F, GL_RGBA32F)
    void BindImageTexture(uint32_t unit, GLuint texture, int level,
                          GLenum access, GLenum format) const;

    // 绑定 CubeMap 图像纹理 (绑定所有 6 个面)
    void BindImageTextureCube(uint32_t unit, GLuint texture, int level,
                              GLenum access, GLenum format) const;

    // Uniform 设置方法
    void SetInt(const std::string& name, int value) const;
    void SetIVec2(const std::string& name, const glm::ivec2& value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec2(const std::string& name, const glm::vec2& value) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetVec4(const std::string& name, const glm::vec4& value) const;
    void SetMat3(const std::string& name, const glm::mat3& value) const;
    void SetMat4(const std::string& name, const glm::mat4& value) const;

    // 状态查询
    bool IsValid() const { return m_ProgramID != 0; }
    operator bool() const { return IsValid(); }
    GLuint GetID() const { return m_ProgramID; }
    const std::string& GetName() const { return m_Name; }

    // 获取工作组大小 (从 shader 中查询)
    glm::ivec3 GetWorkGroupSize() const;

    // 静态创建方法
    static Ref<ComputePipeline> Create(const std::filesystem::path& filepath);

private:
    GLint GetUniformLocation(const std::string& name) const;
    std::string ReadFile(const std::filesystem::path& path) const;
    bool CompileAndLink(const std::string& source);

    GLuint m_ProgramID = 0;
    std::string m_Name;
    mutable std::unordered_map<std::string, GLint> m_UniformCache;
};

} // namespace GLRenderer
