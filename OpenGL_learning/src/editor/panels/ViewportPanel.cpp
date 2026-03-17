#include "ViewportPanel.h"
#include "editor/EditorContext.h"
#include "core/Input.h"
#include "asset/AssetManager.h"

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
    spec.HasEntityIDAttachment = true;  // 启用实体 ID 附件用于鼠标拾取
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

    // 接受拖放资产
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const char* path = static_cast<const char*>(payload->Data);
            std::filesystem::path assetPath(path);

            AssetType type = GetAssetTypeFromExtension(assetPath);
            if (type == AssetType::Model) {
                // 导入模型
                ModelHandle handle = AssetManager::Get().ImportModel(assetPath);
                if (handle.IsValid()) {
                    // TODO: 在场景中创建实体并附加模型组件
                    std::cout << "[Viewport] Dropped model: " << assetPath << std::endl;
                }
            } else if (type == AssetType::Texture) {
                // 导入纹理
                TextureHandle handle = AssetManager::Get().ImportTexture(assetPath);
                if (handle.IsValid()) {
                    std::cout << "[Viewport] Dropped texture: " << assetPath << std::endl;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
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

bool ViewportPanel::ProcessMousePicking(int& outEntityID) {
    outEntityID = -1;

    // 仅在视口悬停时处理
    if (!m_IsHovered) {
        m_HoveredEntityID = -1;
        return false;
    }

    // 如果正在进行相机操作，不进行拾取
    if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT) ||
        (Input::IsKeyPressed(GLFW_KEY_LEFT_ALT) &&
         (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) ||
          Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE)))) {
        m_HoveredEntityID = -1;
        return false;
    }

    // 只在左键点击时读取（避免每帧 ReadPixel 导致的 GPU 同步延迟）
    static bool wasLeftPressed = false;
    bool isLeftPressed = Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);

    bool hasNewPick = false;

    // 检测鼠标左键按下的瞬间
    if (isLeftPressed && !wasLeftPressed) {
        // 获取鼠标在视口中的位置
        glm::vec2 mousePos = Input::GetMousePosition();

        // 转换为视口局部坐标
        float mx = mousePos.x - m_ViewportBounds[0].x;
        float my = mousePos.y - m_ViewportBounds[0].y;

        // 翻转 Y 坐标（OpenGL 原点在左下角）
        my = m_ViewportSize.y - my;

        // 检查是否在视口范围内
        if (mx >= 0 && my >= 0 && mx < m_ViewportSize.x && my < m_ViewportSize.y) {
            // 读取实体 ID（仅在点击时读取）
            int pixelData = m_Framebuffer->ReadPixel(static_cast<int>(mx), static_cast<int>(my));
            m_HoveredEntityID = pixelData;
            outEntityID = pixelData;
            hasNewPick = true;

            std::cout << "[Viewport] Clicked at (" << static_cast<int>(mx) << ", "
                      << static_cast<int>(my) << "), Entity ID: " << pixelData << std::endl;
        } else {
            m_HoveredEntityID = -1;
            outEntityID = -1;
            hasNewPick = true;  // 点击了视口外，也算是一次有效的"空选择"
        }
    }

    wasLeftPressed = isLeftPressed;
    return hasNewPick;
}

} // namespace GLRenderer
