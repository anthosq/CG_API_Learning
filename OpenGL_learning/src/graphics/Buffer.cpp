#include "Buffer.h"

namespace GLRenderer {

// VertexBuffer 实现
VertexBuffer::VertexBuffer(const void* data, size_t size) {
    glGenBuffers(1, &m_ID);
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    GL_CHECK_ERROR();
}

VertexBuffer::VertexBuffer(const void* data, size_t size, bool dynamic) {
    glGenBuffers(1, &m_ID);
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
    glBufferData(GL_ARRAY_BUFFER, size, data, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    GL_CHECK_ERROR();
}

VertexBuffer::~VertexBuffer() {
    if (m_ID != 0) {
        glDeleteBuffers(1, &m_ID);
        m_ID = 0;
    }
}

VertexBuffer::VertexBuffer(VertexBuffer&& other) noexcept
    : m_ID(other.m_ID) {
    other.m_ID = 0;  // 转移所有权
}

VertexBuffer& VertexBuffer::operator=(VertexBuffer&& other) noexcept {
    if (this != &other) {
        // 释放当前资源
        if (m_ID != 0) {
            glDeleteBuffers(1, &m_ID);
        }
        // 转移所有权
        m_ID = other.m_ID;
        other.m_ID = 0;
    }
    return *this;
}

void VertexBuffer::Bind() const {
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
}

void VertexBuffer::Unbind() const {
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VertexBuffer::SetData(const void* data, size_t size) {
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
}

void VertexBuffer::SetSubData(const void* data, size_t size, size_t offset) {
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
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

IndexBuffer::IndexBuffer(IndexBuffer&& other) noexcept
    : m_ID(other.m_ID), m_Count(other.m_Count) {
    other.m_ID = 0;
    other.m_Count = 0;
}

IndexBuffer& IndexBuffer::operator=(IndexBuffer&& other) noexcept {
    if (this != &other) {
        if (m_ID != 0) {
            glDeleteBuffers(1, &m_ID);
        }
        m_ID = other.m_ID;
        m_Count = other.m_Count;
        other.m_ID = 0;
        other.m_Count = 0;
    }
    return *this;
}

void IndexBuffer::Bind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
}

void IndexBuffer::Unbind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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

VertexArray::VertexArray(VertexArray&& other) noexcept
    : m_ID(other.m_ID),
      m_VertexBuffers(std::move(other.m_VertexBuffers)),
      m_IndexBuffer(std::move(other.m_IndexBuffer)) {
    other.m_ID = 0;
}

VertexArray& VertexArray::operator=(VertexArray&& other) noexcept {
    if (this != &other) {
        if (m_ID != 0) {
            glDeleteVertexArrays(1, &m_ID);
        }
        m_ID = other.m_ID;
        m_VertexBuffers = std::move(other.m_VertexBuffers);
        m_IndexBuffer = std::move(other.m_IndexBuffer);
        other.m_ID = 0;
    }
    return *this;
}

void VertexArray::Bind() const {
    glBindVertexArray(m_ID);
}

void VertexArray::Unbind() const {
    glBindVertexArray(0);
}

void VertexArray::AddVertexBuffer(std::unique_ptr<VertexBuffer> vbo,
                                   const std::vector<VertexAttribute>& layout) {
    glBindVertexArray(m_ID);
    vbo->Bind();

    // 配置顶点属性
    for (const auto& attr : layout) {
        glEnableVertexAttribArray(attr.index);

        if (attr.type == GL_INT || attr.type == GL_UNSIGNED_INT) {
            // 整数属性使用 glVertexAttribIPointer
            glVertexAttribIPointer(
                attr.index,
                attr.size,
                attr.type,
                static_cast<GLsizei>(attr.stride),
                reinterpret_cast<const void*>(attr.offset)
            );
        } else {
            // 浮点属性使用 glVertexAttribPointer
            glVertexAttribPointer(
                attr.index,
                attr.size,
                attr.type,
                attr.normalized ? GL_TRUE : GL_FALSE,
                static_cast<GLsizei>(attr.stride),
                reinterpret_cast<const void*>(attr.offset)
            );
        }
    }

    m_VertexBuffers.push_back(std::move(vbo));
    GL_CHECK_ERROR();
}

void VertexArray::SetIndexBuffer(std::unique_ptr<IndexBuffer> ibo) {
    glBindVertexArray(m_ID);
    ibo->Bind();
    m_IndexBuffer = std::move(ibo);
    GL_CHECK_ERROR();
}

} // namespace GLRenderer
