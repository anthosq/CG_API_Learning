#pragma once

#include "Window.h"
#include "Input.h"
#include "Timer.h"
#include <memory>

namespace GLRenderer {

// ============================================================================
// Application - 应用程序基类
// ============================================================================
class Application : public NonCopyable {
public:
    Application(const WindowProps& props = WindowProps());
    virtual ~Application();

    // 运行主循环
    void Run();

    // 关闭应用程序
    void Close();

    // 获取窗口
    Window& GetWindow() { return *m_Window; }
    const Window& GetWindow() const { return *m_Window; }

    // 获取时间信息
    float GetDeltaTime() const { return m_Timer.GetDeltaTime(); }
    float GetTime() const { return m_Timer.GetTime(); }
    float GetFPS() const { return m_Timer.GetFPS(); }

    // 获取单例实例
    static Application& Get() { return *s_Instance; }

protected:
    // 子类重写的生命周期方法
    virtual void OnInit() {}
    virtual void OnShutdown() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender() {}
    virtual void OnImGuiRender() {}

    // 帧缓冲大小变化回调（子类可重写）
    virtual void OnWindowResize(int width, int height) {}

private:
    std::unique_ptr<Window> m_Window;
    Timer m_Timer;
    bool m_Running = true;
    bool m_Minimized = false;

    static Application* s_Instance;
};

} // namespace GLRenderer

using GLRenderer::Application;
