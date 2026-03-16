#include "ViewportPanel.h"
#include "editor/EditorContext.h"
#include "core/Input.h"

#include <imgui.h>
#include <GLFW/glfw3.h>

namespace GLRenderer {

ViewportPanel::ViewportPanel()
    : Panel("Viewport", true)
    , m_Camera(glm::vec3(5.0f, 5.0f, 5.0f))
{
    // 初始化相机
    m_Camera.Yaw = -135.0f;
    m_Camera.Pitch = -30.0f;

    // 初始化 Framebuffer
    FramebufferSpec spec;
    spec.Width = static_cast<uint32_t>(m_ViewportSize.x);
    spec.Height = static_cast<uint32_t>(m_ViewportSize.y);
    spec.HasColorAttachment = true;
    spec.HasDepthAttachment = true;
    spec.DepthAsTexture = false;
    spec.Samples = 1;

    m_Framebuffer = std::make_unique<Framebuffer>(spec);
}

void ViewportPanel::OnDraw(EditorContext& context) {
    // 更新焦点和悬停状态
    m_IsFocused = ImGui::IsWindowFocused();
    m_IsHovered = ImGui::IsWindowHovered();

    // 获取可用区域大小
    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

    // 确保大小有效
    if (viewportPanelSize.x <= 0) viewportPanelSize.x = 1;
    if (viewportPanelSize.y <= 0) viewportPanelSize.y = 1;

    m_ViewportSize = {viewportPanelSize.x, viewportPanelSize.y};

    // 检查是否需要调整 FBO 大小
    // if (m_ViewportSize.x != m_Framebuffer->GetWidth() ||
    //     m_ViewportSize.y != m_Framebuffer->GetHeight()) {
    //     ResizeFramebufferIfNeeded(m_ViewportSize.x, m_ViewportSize.y);
    // }

    // 渲染 Framebuffer 颜色附件作为图像
    // 注意：OpenGL 纹理原点在左下角，需要翻转 UV
    GLuint textureID = m_Framebuffer->GetColorAttachment();
    ImGui::Image(
        (void*)(uintptr_t)textureID,
        ImVec2(m_ViewportSize.x, m_ViewportSize.y),
        ImVec2(0, 1),  // UV 最小值 (翻转 Y)
        ImVec2(1, 0)   // UV 最大值 (翻转 Y)
    );

    // 保存视口边界（用于鼠标拾取）
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    m_ViewportBounds[0] = {windowPos.x + contentMin.x, windowPos.y + contentMin.y};
    m_ViewportBounds[1] = {m_ViewportBounds[0].x + m_ViewportSize.x,
                           m_ViewportBounds[0].y + m_ViewportSize.y};

    // 显示视口信息（调试用）
    // ImGui::SetCursorPos(ImVec2(10, 30));
    // ImGui::Text("Size: %.0f x %.0f", m_ViewportSize.x, m_ViewportSize.y);
    // ImGui::Text("Focused: %s, Hovered: %s",
    //             m_IsFocused ? "Yes" : "No",
    //             m_IsHovered ? "Yes" : "No");
}

ImGuiWindowFlags ViewportPanel::GetWindowFlags() const {
    return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
}

float ViewportPanel::GetAspectRatio() const {
    if (m_ViewportSize.y <= 0) return 1.0f;
    return m_ViewportSize.x / m_ViewportSize.y;
}

void ViewportPanel::OnResize(float width, float height) {
    ResizeFramebufferIfNeeded(width, height);
}

void ViewportPanel::ResizeFramebufferIfNeeded(float width, float height) {
    if (width > 0 && height > 0) {
        m_ViewportSize = {width, height};
        m_Framebuffer->Resize(
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        );
    }
}

void ViewportPanel::ProcessCameraInput(float deltaTime) {
    // 仅在视口悬停时处理相机输入
    if (!m_IsHovered) return;

    glm::vec2 mouseDelta = Input::GetMouseDelta();
    float scroll = Input::GetScrollOffset();

    // 右键按下时启用飞行相机模式 (FPS camera)
    if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT) &&
        !Input::IsKeyPressed(GLFW_KEY_LEFT_ALT)) {

        // WASD 移动
        if (Input::IsKeyPressed(GLFW_KEY_W))
            m_Camera.ProcessKeyboard(FORWARD, deltaTime);
        if (Input::IsKeyPressed(GLFW_KEY_S))
            m_Camera.ProcessKeyboard(BACKWARD, deltaTime);
        if (Input::IsKeyPressed(GLFW_KEY_A))
            m_Camera.ProcessKeyboard(LEFT, deltaTime);
        if (Input::IsKeyPressed(GLFW_KEY_D))
            m_Camera.ProcessKeyboard(RIGHT, deltaTime);

        // Q/E 垂直移动
        if (Input::IsKeyPressed(GLFW_KEY_Q))
            m_Camera.ProcessKeyboard(CameraMovement::Down, deltaTime);
        if (Input::IsKeyPressed(GLFW_KEY_E))
            m_Camera.ProcessKeyboard(CameraMovement::Up, deltaTime);

        // 鼠标旋转
        if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
            m_Camera.ProcessMouseMovement(mouseDelta.x, mouseDelta.y);
        }

        // 右键+滚轮调整移动速度
        if (scroll != 0.0f) {
            float currentSpeed = m_Camera.GetSettings().MovementSpeed;
            float newSpeed = currentSpeed + scroll * 0.5f * currentSpeed;
            newSpeed = glm::clamp(newSpeed, 0.1f, 50.0f);
            m_Camera.SetMovementSpeed(newSpeed);
        }
    }
    // Alt + 鼠标: 轨道相机模式 (Arcball camera)
    else if (Input::IsKeyPressed(GLFW_KEY_LEFT_ALT)) {
        // Alt + 左键: 旋转
        if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
            if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
                m_Camera.ProcessMouseMovement(mouseDelta.x, mouseDelta.y);
            }
        }
        // Alt + 中键: 平移
        else if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE)) {
            if (mouseDelta.x != 0.0f || mouseDelta.y != 0.0f) {
                float panSpeed = 0.01f;
                glm::vec3 right = m_Camera.GetRight();
                glm::vec3 up = m_Camera.GetUp();
                glm::vec3 offset = -right * mouseDelta.x * panSpeed + up * mouseDelta.y * panSpeed;
                m_Camera.SetPosition(m_Camera.GetPosition() + offset);
            }
        }
        // Alt + 右键: 缩放 (通过鼠标移动)
        else if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
            float zoomDelta = (mouseDelta.x + mouseDelta.y) * 0.01f;
            m_Camera.ProcessMouseScroll(zoomDelta);
        }
    }

    // 普通滚轮缩放 (无修饰键时)
    if (!Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT) &&
        !Input::IsKeyPressed(GLFW_KEY_LEFT_ALT)) {
        if (scroll != 0.0f) {
            m_Camera.ProcessMouseScroll(scroll);
        }
    }
}

} // namespace GLRenderer
