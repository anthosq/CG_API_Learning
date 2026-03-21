#pragma once

#include "core/Ref.h"
#include "utils/GLCommon.h"

namespace GLRenderer {

// Shader Storage Buffer Object (SSBO) 封装
// 用于 Compute Shader 数据读写、粒子系统等
class StorageBuffer : public RefCounter {
public:
    StorageBuffer(size_t size, GLenum usage = GL_DYNAMIC_DRAW);
    ~StorageBuffer();

    void Bind() const;
    void Unbind() const;
    void BindBase(uint32_t binding) const;

    void SetData(const void* data, size_t size, size_t offset = 0);
    void GetData(void* data, size_t size, size_t offset = 0) const;

    void* Map(GLenum access);
    void Unmap();

    uint32_t GetRendererID() const { return m_RendererID; }
    size_t GetSize() const { return m_Size; }

    static Ref<StorageBuffer> Create(size_t size, GLenum usage = GL_DYNAMIC_DRAW);

private:
    uint32_t m_RendererID = 0;
    size_t m_Size = 0;
};

} // namespace GLRenderer
