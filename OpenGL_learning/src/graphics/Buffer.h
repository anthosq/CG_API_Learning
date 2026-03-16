#pragma once

#include "utils/GLCommon.h"
#include <vector>
#include <memory>

namespace GLRenderer {

// VertexBuffer - VBO 封装
class VertexBuffer : public NonCopyable {
public:
    // 创建顶点缓冲并上传数据
    VertexBuffer(const void* data, size_t size);
    ~VertexBuffer();

    // 移动语义
    VertexBuffer(VertexBuffer&& other) noexcept;
    VertexBuffer& operator=(VertexBuffer&& other) noexcept;

    void Bind() const;
    void Unbind() const;

    // 更新缓冲区数据（用于动态缓冲）
    void SetData(const void* data, size_t size);

    GLuint GetID() const { return m_ID; }

private:
    GLuint m_ID = 0;
};

// ============================================================================
// IndexBuffer - EBO 封装
// ============================================================================
class IndexBuffer : public NonCopyable {
public:
    // 创建索引缓冲并上传数据
    IndexBuffer(const uint32_t* data, uint32_t count);
    ~IndexBuffer();

    // 移动语义
    IndexBuffer(IndexBuffer&& other) noexcept;
    IndexBuffer& operator=(IndexBuffer&& other) noexcept;

    void Bind() const;
    void Unbind() const;

    uint32_t GetCount() const { return m_Count; }
    GLuint GetID() const { return m_ID; }

private:
    GLuint m_ID = 0;
    uint32_t m_Count = 0;
};

// ============================================================================
// VertexAttribute - 顶点属性描述
// ============================================================================
struct VertexAttribute {
    uint32_t index;       // 属性索引 (location)
    int32_t size;         // 分量数量 (1, 2, 3, 4)
    GLenum type;          // 数据类型 (GL_FLOAT, GL_INT, etc.)
    bool normalized;      // 是否归一化
    size_t stride;        // 步长 (通常为 sizeof(Vertex))
    size_t offset;        // 在顶点结构中的偏移

    // 便捷构造函数
    static VertexAttribute Float(uint32_t index, int32_t size, size_t stride, size_t offset) {
        return {index, size, GL_FLOAT, false, stride, offset};
    }

    static VertexAttribute Int(uint32_t index, int32_t size, size_t stride, size_t offset) {
        return {index, size, GL_INT, false, stride, offset};
    }
};

// ============================================================================
// VertexArray - VAO 封装
// ============================================================================
class VertexArray : public NonCopyable {
public:
    VertexArray();
    ~VertexArray();

    // 移动语义
    VertexArray(VertexArray&& other) noexcept;
    VertexArray& operator=(VertexArray&& other) noexcept;

    void Bind() const;
    void Unbind() const;

    // 添加顶点缓冲并配置属性布局
    void AddVertexBuffer(std::unique_ptr<VertexBuffer> vbo,
                         const std::vector<VertexAttribute>& layout);

    // 设置索引缓冲
    void SetIndexBuffer(std::unique_ptr<IndexBuffer> ibo);

    // 获取索引缓冲
    const IndexBuffer* GetIndexBuffer() const {
        return m_IndexBuffer.get();
    }

    // 获取顶点缓冲数量
    size_t GetVertexBufferCount() const {
        return m_VertexBuffers.size();
    }

    GLuint GetID() const { return m_ID; }

private:
    GLuint m_ID = 0;
    std::vector<std::unique_ptr<VertexBuffer>> m_VertexBuffers;
    std::unique_ptr<IndexBuffer> m_IndexBuffer;
};

} // namespace GLRenderer
