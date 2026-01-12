#pragma once

// 转发到原有的 Mesh 实现
// 在后续重构中，将逐步将代码迁移到这里

#include "../Mesh.h"

// 注意：当前 Mesh 和 Model 类保留在 src/Mesh.h 和 src/Mesh.cpp
// 它们已经实现了基本的 RAII（虽然不完整）
// 后续可以考虑：
// 1. 添加析构函数释放 VAO/VBO/EBO
// 2. 使用 graphics/Buffer.h 中的 VertexArray 类
// 3. 改进纹理缓存机制
