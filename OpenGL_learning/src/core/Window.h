#pragma once

#include "utils/GLCommon.h"
#include <string>
#include <functional>

namespace GLRenderer {

// ============================================================================
// WindowProps - 窗口属性配置
// ============================================================================
class WindowProps {
    public:
        WindowProps(const std::string& title = "GLRenderer",
                    int width = 1280,
                    int height = 720,
                    bool vsync = true,
                    bool resizable = true)
            : Title(title), Width(width), Height(height), VSync(vsync), Resizable(resizable) {}

        std::string Title;
        int Width;
        int Height;
        bool VSync;
        bool Resizable;
};

// ============================================================================
// Window - 窗口管理类
// ============================================================================
class Window : public NonCopyable {
public:
    // 窗口大小变化回调类型
    using ResizeCallback = std::function<void(int width, int height)>;

    Window(const WindowProps& props = WindowProps());
    ~Window();

    // 每帧更新（交换缓冲、处理事件）
    void OnUpdate();

    // 是否应该关闭
    bool ShouldClose() const;

    // 关闭窗口
    void Close();

    // 属性访问
    int GetWidth() const { return m_Data.Width; }
    int GetHeight() const { return m_Data.Height; }
    float GetAspectRatio() const;
    const std::string& GetTitle() const { return m_Data.Title; }

    // 设置属性
    void SetVSync(bool enabled);
    bool IsVSync() const { return m_Data.VSync; }

    void SetTitle(const std::string& title);

    // 光标模式控制
    void SetCursorMode(int mode);  // GLFW_CURSOR_NORMAL, GLFW_CURSOR_DISABLED, etc.
    void EnableCursor(bool enable);
    bool IsCursorEnabled() const { return m_CursorEnabled; }

    // 设置窗口大小变化回调
    void SetResizeCallback(ResizeCallback callback) { m_ResizeCallback = callback; }

    // 获取原生窗口指针
    GLFWwindow* GetNativeWindow() const { return m_Window; }

    // 静态方法：获取当前时间
    static float GetTime();

private:
    void Init(const WindowProps& props);
    void Shutdown();

    // GLFW 回调
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* m_Window = nullptr;
    bool m_CursorEnabled = true;
    ResizeCallback m_ResizeCallback;

    // 窗口数据（用于回调访问）
    struct WindowData {
        std::string Title;
        int Width;
        int Height;
        bool VSync;
        Window* Instance;  // 指向 Window 实例
    } m_Data;
};

} // namespace GLRenderer

using GLRenderer::Window;
using GLRenderer::WindowProps;
