#pragma once

// OpenGL 函数加载器 (必须在 GLFW 之前包含)
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// 标准库
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <iostream>

// GLM 数学库
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace GLRenderer {

// 检查 OpenGL 错误的辅助宏 (仅在 Debug 模式下启用)
#ifdef _DEBUG
    #define GL_CHECK_ERROR() GLRenderer::CheckOpenGLError(__FILE__, __LINE__)
#else
    #define GL_CHECK_ERROR() ((void)0)
#endif

// 检查 OpenGL 错误
inline void CheckOpenGLError(const char* file, int line) {
    GLenum error = glGetError();
    while (error != GL_NO_ERROR) {
        std::string errorStr;
        switch (error) {
            case GL_INVALID_ENUM:      errorStr = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE:     errorStr = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY:     errorStr = "GL_OUT_OF_MEMORY"; break;
            default:                   errorStr = "Unknown Error"; break;
        }
        std::cerr << "[OpenGL Error] " << errorStr
                  << " at " << file << ":" << line << std::endl;
        error = glGetError();
    }
}

// 禁止拷贝的基类
class NonCopyable {
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;

    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

} // namespace GLRenderer
