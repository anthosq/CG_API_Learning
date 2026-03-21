#include "Window.h"

namespace GLRenderer {

static bool s_GLFWInitialized = false;

Window::Window(const WindowProps& props) {
    Init(props);
}

Window::~Window() {
    Shutdown();
}

void Window::Init(const WindowProps& props) {
    m_Data.Title = props.Title;
    m_Data.Width = props.Width;
    m_Data.Height = props.Height;
    m_Data.VSync = props.VSync;
    m_Data.Instance = this;

    // 初始化 GLFW（只执行一次）
    if (!s_GLFWInitialized) {
        int success = glfwInit();
        if (!success) {
            throw std::runtime_error("[Window] GLFW 初始化失败");
        }
        s_GLFWInitialized = true;
    }

    // 设置 OpenGL 版本 (4.5 支持 Compute Shader, Image Load/Store 等)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 窗口属性
    glfwWindowHint(GLFW_RESIZABLE, props.Resizable ? GLFW_TRUE : GLFW_FALSE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // 创建窗口
    m_Window = glfwCreateWindow(props.Width, props.Height,
                                props.Title.c_str(), nullptr, nullptr);
    if (!m_Window) {
        throw std::runtime_error("[Window] 窗口创建失败");
    }

    glfwMakeContextCurrent(m_Window);

    // 初始化 GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("[Window] GLAD 初始化失败");
    }

    // 设置用户数据指针
    glfwSetWindowUserPointer(m_Window, &m_Data);

    // 设置回调
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);

    // 设置 VSync
    SetVSync(props.VSync);

    // 打印 OpenGL 信息
    std::cout << "[Window] OpenGL 信息:" << std::endl;
    std::cout << "  供应商: " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "  渲染器: " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "  版本: " << glGetString(GL_VERSION) << std::endl;
}

void Window::Shutdown() {
    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
}

void Window::OnUpdate() {
    glfwPollEvents();
    glfwSwapBuffers(m_Window);
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_Window);
}

void Window::Close() {
    glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
}

float Window::GetAspectRatio() const {
    if (m_Data.Height == 0) return 1.0f;
    return static_cast<float>(m_Data.Width) / static_cast<float>(m_Data.Height);
}

void Window::SetVSync(bool enabled) {
    glfwSwapInterval(enabled ? 1 : 0);
    m_Data.VSync = enabled;
}

void Window::SetTitle(const std::string& title) {
    m_Data.Title = title;
    glfwSetWindowTitle(m_Window, title.c_str());
}

void Window::SetCursorMode(int mode) {
    glfwSetInputMode(m_Window, GLFW_CURSOR, mode);
    m_CursorEnabled = (mode == GLFW_CURSOR_NORMAL);
}

void Window::EnableCursor(bool enable) {
    SetCursorMode(enable ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
}

float Window::GetTime() {
    return static_cast<float>(glfwGetTime());
}

void Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    WindowData* data = static_cast<WindowData*>(glfwGetWindowUserPointer(window));
    if (data) {
        data->Width = width;
        data->Height = height;

        glViewport(0, 0, width, height);

        // 调用用户回调
        if (data->Instance && data->Instance->m_ResizeCallback) {
            data->Instance->m_ResizeCallback(width, height);
        }
    }
}

} // namespace GLRenderer
