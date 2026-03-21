#include "StorageBuffer.h"

namespace GLRenderer {

StorageBuffer::StorageBuffer(size_t size, GLenum usage)
    : m_Size(size) {
    glGenBuffers(1, &m_RendererID);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_RendererID);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, usage);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

StorageBuffer::~StorageBuffer() {
    if (m_RendererID) {
        glDeleteBuffers(1, &m_RendererID);
    }
}

void StorageBuffer::Bind() const {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_RendererID);
}

void StorageBuffer::Unbind() const {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void StorageBuffer::BindBase(uint32_t binding) const {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, m_RendererID);
}

void StorageBuffer::SetData(const void* data, size_t size, size_t offset) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_RendererID);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void StorageBuffer::GetData(void* data, size_t size, size_t offset) const {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_RendererID);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void* StorageBuffer::Map(GLenum access) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_RendererID);
    return glMapBuffer(GL_SHADER_STORAGE_BUFFER, access);
}

void StorageBuffer::Unmap() {
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

Ref<StorageBuffer> StorageBuffer::Create(size_t size, GLenum usage) {
    return Ref<StorageBuffer>(new StorageBuffer(size, usage));
}

} // namespace GLRenderer
