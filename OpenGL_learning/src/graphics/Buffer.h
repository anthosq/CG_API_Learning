#pragma once

#include "utils/GLCommon.h"
#include "core/Ref.h"
#include <vector>

namespace GLRenderer {

class VertexBuffer : public RefCounter {
public:
    VertexBuffer(const void* data, uint32_t size);
    VertexBuffer(const void* data, uint32_t size, bool dynamic);
    VertexBuffer(uint32_t size);
    ~VertexBuffer();

    void Bind() const;
    void Unbind() const;

    void SetData(const void* data, uint32_t size);
    void SetSubData(const void* data, uint32_t size, uint32_t offset);

    GLuint GetID() const { return m_ID; }
    uint32_t GetSize() const { return m_Size; }

    static Ref<VertexBuffer> Create(const void* data, uint32_t size);
    static Ref<VertexBuffer> Create(uint32_t size);

private:
    GLuint m_ID = 0;
    uint32_t m_Size = 0;
};

class IndexBuffer : public RefCounter {
public:
    IndexBuffer(const uint32_t* data, uint32_t count);
    ~IndexBuffer();

    void Bind() const;
    void Unbind() const;

    uint32_t GetCount() const { return m_Count; }
    GLuint GetID() const { return m_ID; }

    static Ref<IndexBuffer> Create(const uint32_t* data, uint32_t count);

private:
    GLuint m_ID = 0;
    uint32_t m_Count = 0;
};

struct VertexAttribute {
    uint32_t index;
    int32_t size;
    GLenum type;
    bool normalized;
    uint32_t stride;
    uint32_t offset;

    static VertexAttribute Float(uint32_t index, int32_t size, uint32_t stride, uint32_t offset) {
        return {index, size, GL_FLOAT, false, stride, offset};
    }

    static VertexAttribute Int(uint32_t index, int32_t size, uint32_t stride, uint32_t offset) {
        return {index, size, GL_INT, false, stride, offset};
    }
};

class VertexArray : public RefCounter {
public:
    VertexArray();
    ~VertexArray();

    void Bind() const;
    void Unbind() const;

    void AddVertexBuffer(const Ref<VertexBuffer>& vbo, const std::vector<VertexAttribute>& layout);
    void SetIndexBuffer(const Ref<IndexBuffer>& ibo);

    Ref<IndexBuffer> GetIndexBuffer() const { return m_IndexBuffer; }
    const std::vector<Ref<VertexBuffer>>& GetVertexBuffers() const { return m_VertexBuffers; }
    uint32_t GetVertexBufferCount() const { return static_cast<uint32_t>(m_VertexBuffers.size()); }

    GLuint GetID() const { return m_ID; }

    static Ref<VertexArray> Create();

private:
    GLuint m_ID = 0;
    std::vector<Ref<VertexBuffer>> m_VertexBuffers;
    Ref<IndexBuffer> m_IndexBuffer;
};

} // namespace GLRenderer
