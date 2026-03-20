#include "Buffer.h"

namespace GLRenderer {

VertexBuffer::VertexBuffer(const void* data, uint32_t size)
    : m_Size(size) {
    glGenBuffers(1, &m_ID);
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    GL_CHECK_ERROR();
}

VertexBuffer::VertexBuffer(const void* data, uint32_t size, bool dynamic)
    : m_Size(size) {
    glGenBuffers(1, &m_ID);
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
    glBufferData(GL_ARRAY_BUFFER, size, data, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    GL_CHECK_ERROR();
}

VertexBuffer::VertexBuffer(uint32_t size)
    : m_Size(size) {
    glGenBuffers(1, &m_ID);
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
    glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    GL_CHECK_ERROR();
}

VertexBuffer::~VertexBuffer() {
    if (m_ID != 0) {
        glDeleteBuffers(1, &m_ID);
        m_ID = 0;
    }
}

void VertexBuffer::Bind() const {
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
}

void VertexBuffer::Unbind() const {
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VertexBuffer::SetData(const void* data, uint32_t size) {
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
}

void VertexBuffer::SetSubData(const void* data, uint32_t size, uint32_t offset) {
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
}

Ref<VertexBuffer> VertexBuffer::Create(const void* data, uint32_t size) {
    return Ref<VertexBuffer>(new VertexBuffer(data, size));
}

Ref<VertexBuffer> VertexBuffer::Create(uint32_t size) {
    return Ref<VertexBuffer>(new VertexBuffer(size));
}

IndexBuffer::IndexBuffer(const uint32_t* data, uint32_t count)
    : m_Count(count) {
    glGenBuffers(1, &m_ID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), data, GL_STATIC_DRAW);
    GL_CHECK_ERROR();
}

IndexBuffer::~IndexBuffer() {
    if (m_ID != 0) {
        glDeleteBuffers(1, &m_ID);
        m_ID = 0;
    }
}

void IndexBuffer::Bind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
}

void IndexBuffer::Unbind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

Ref<IndexBuffer> IndexBuffer::Create(const uint32_t* data, uint32_t count) {
    return Ref<IndexBuffer>(new IndexBuffer(data, count));
}

VertexArray::VertexArray() {
    glGenVertexArrays(1, &m_ID);
    GL_CHECK_ERROR();
}

VertexArray::~VertexArray() {
    if (m_ID != 0) {
        glDeleteVertexArrays(1, &m_ID);
        m_ID = 0;
    }
}

void VertexArray::Bind() const {
    glBindVertexArray(m_ID);
}

void VertexArray::Unbind() const {
    glBindVertexArray(0);
}

void VertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vbo,
                                   const std::vector<VertexAttribute>& layout) {
    glBindVertexArray(m_ID);
    vbo->Bind();

    for (const auto& attr : layout) {
        glEnableVertexAttribArray(attr.index);

        if (attr.type == GL_INT || attr.type == GL_UNSIGNED_INT) {
            glVertexAttribIPointer(
                attr.index,
                attr.size,
                attr.type,
                static_cast<GLsizei>(attr.stride),
                reinterpret_cast<const void*>(static_cast<uintptr_t>(attr.offset))
            );
        } else {
            glVertexAttribPointer(
                attr.index,
                attr.size,
                attr.type,
                attr.normalized ? GL_TRUE : GL_FALSE,
                static_cast<GLsizei>(attr.stride),
                reinterpret_cast<const void*>(static_cast<uintptr_t>(attr.offset))
            );
        }
    }

    m_VertexBuffers.push_back(vbo);
    GL_CHECK_ERROR();
}

void VertexArray::SetIndexBuffer(const Ref<IndexBuffer>& ibo) {
    glBindVertexArray(m_ID);
    ibo->Bind();
    m_IndexBuffer = ibo;
    GL_CHECK_ERROR();
}

Ref<VertexArray> VertexArray::Create() {
    return Ref<VertexArray>(new VertexArray());
}

} // namespace GLRenderer
