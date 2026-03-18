#include "UniformBuffer.h"
#include <utility>

namespace GLRenderer {

UniformBuffer::UniformBuffer(uint32_t size, uint32_t bindingPoint)
    : m_BindingPoint(bindingPoint), m_Size(size) {
    glGenBuffers(1, &m_ID);
    glBindBuffer(GL_UNIFORM_BUFFER, m_ID);
    glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, m_ID);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

UniformBuffer::~UniformBuffer() {
    if (m_ID != 0) {
        glDeleteBuffers(1, &m_ID);
    }
}

UniformBuffer::UniformBuffer(UniformBuffer&& other) noexcept
    : m_ID(other.m_ID)
    , m_BindingPoint(other.m_BindingPoint)
    , m_Size(other.m_Size) {
    other.m_ID = 0;
    other.m_BindingPoint = 0;
    other.m_Size = 0;
}

UniformBuffer& UniformBuffer::operator=(UniformBuffer&& other) noexcept {
    if (this != &other) {
        // 删除当前资源
        if (m_ID != 0) {
            glDeleteBuffers(1, &m_ID);
        }

        // 移动资源
        m_ID = other.m_ID;
        m_BindingPoint = other.m_BindingPoint;
        m_Size = other.m_Size;

        // 清空源对象
        other.m_ID = 0;
        other.m_BindingPoint = 0;
        other.m_Size = 0;
    }
    return *this;
}

void UniformBuffer::SetData(const void* data, uint32_t size, uint32_t offset) {
    glBindBuffer(GL_UNIFORM_BUFFER, m_ID);
    glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void UniformBuffer::Bind() const {
    glBindBufferBase(GL_UNIFORM_BUFFER, m_BindingPoint, m_ID);
}

void UniformBuffer::Unbind() const {
    glBindBufferBase(GL_UNIFORM_BUFFER, m_BindingPoint, 0);
}

} // namespace GLRenderer
