#include "Application.h"

namespace GLRenderer {

Application* Application::s_Instance = nullptr;

Application::Application(const WindowProps& props) {
    if (s_Instance) {
        throw std::runtime_error("[Application] 应用程序已存在，不能创建多个实例");
    }
    s_Instance = this;

    // 创建窗口
    m_Window = std::make_unique<Window>(props);

    // 初始化输入系统
    Input::Init(m_Window->GetNativeWindow());

    // 设置窗口大小变化回调
    m_Window->SetResizeCallback([this](int width, int height) {
        if (width == 0 || height == 0) {
            m_Minimized = true;
            return;
        }
        m_Minimized = false;
        OnWindowResize(width, height);
    });
}

Application::~Application() {
    s_Instance = nullptr;
}

void Application::Run() {
    OnInit();
    m_Timer.Reset();

    while (m_Running && !m_Window->ShouldClose()) {
        m_Timer.Update();
        float deltaTime = m_Timer.GetDeltaTime();

        // 如果窗口最小化，跳过渲染
        if (!m_Minimized) {
            OnUpdate(deltaTime);
            OnRender();
            OnImGuiRender();
        }

        // 重置输入增量（在处理新事件之前）
        Input::OnUpdate();

        // 更新窗口（交换缓冲、处理事件）
        // glfwPollEvents 会触发回调，设置新的输入值
        m_Window->OnUpdate();
    }

    // 清理
    OnShutdown();
}

void Application::Close() {
    m_Running = false;
}

} // namespace GLRenderer
