#pragma once

#include "utils/GLCommon.h"
#include <cstdint>

namespace GLRenderer {

// UniformBuffer - UBO (Uniform Buffer Object) 封装
// 用于高效地向着色器传递共享的 uniform 数据
//
// 使用 std140 布局对齐规则:
// - float/int/bool: 4 bytes
// - vec2: 8 bytes
// - vec3/vec4: 16 bytes (vec3 也按 16 bytes 对齐)
// - mat4: 64 bytes (4 x vec4)
// - 数组元素: 每个按 16 bytes 对齐

class UniformBuffer : public NonCopyable {
public:
    // 创建 UBO 并分配内存
    // @param size 缓冲区大小 (字节)
    // @param bindingPoint 绑定点索引 (对应 GLSL 中的 binding = N)
    UniformBuffer(uint32_t size, uint32_t bindingPoint);
    ~UniformBuffer();

    // 移动语义
    UniformBuffer(UniformBuffer&& other) noexcept;
    UniformBuffer& operator=(UniformBuffer&& other) noexcept;

    // 更新缓冲区数据
    // @param data 数据指针
    // @param size 数据大小 (字节)
    // @param offset 偏移量 (默认 0)
    void SetData(const void* data, uint32_t size, uint32_t offset = 0);

    // 绑定/解绑
    void Bind() const;
    void Unbind() const;

    // 获取属性
    GLuint GetID() const { return m_ID; }
    uint32_t GetBindingPoint() const { return m_BindingPoint; }
    uint32_t GetSize() const { return m_Size; }

private:
    GLuint m_ID = 0;
    uint32_t m_BindingPoint = 0;
    uint32_t m_Size = 0;
};

} // namespace GLRenderer
