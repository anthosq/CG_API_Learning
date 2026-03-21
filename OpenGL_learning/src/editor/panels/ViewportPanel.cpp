#include "ViewportPanel.h"
#include "editor/EditorContext.h"
#include "core/Input.h"
#include "asset/AssetManager.h"
#include "asset/MaterialAsset.h"
#include "graphics/MeshSource.h"
#include "scene/ecs/Components.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/common.hpp>
#include <algorithm>

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

    // 保存视口边界（用于鼠标拾取和 Gizmo）
    // 使用 GetItemRectMin/Max 获取 Image 的精确位置
    ImVec2 imageMin = ImGui::GetItemRectMin();
    ImVec2 imageMax = ImGui::GetItemRectMax();
    m_ViewportBounds[0] = {imageMin.x, imageMin.y};
    m_ViewportBounds[1] = {imageMax.x, imageMax.y};

    // 绘制 Gizmo
    DrawGizmo(context);

    // 接受拖放资产
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            const char* path = static_cast<const char*>(payload->Data);
            std::filesystem::path assetPath(path);

            AssetType type = GetAssetTypeFromExtension(assetPath);
            if (type == AssetType::MeshSource) {
                AssetHandle meshHandle = AssetManager::Get().ImportMeshSource(assetPath);
                if (meshHandle.IsValid() && context.World) {
                    std::string entityName = assetPath.stem().string();
                    ECS::Entity entity = context.World->CreateEntity(entityName);

                    glm::vec3 spawnPosition = m_Camera.GetPosition() + m_Camera.GetFront() * 5.0f;
                    entity.AddComponent<ECS::TransformComponent>(spawnPosition);
                    entity.AddComponent<ECS::MeshComponent>(meshHandle);

                    // 添加 MaterialComponent 并设置从模型加载的材质
                    auto& matComp = entity.AddComponent<ECS::MaterialComponent>();
                    auto meshSource = AssetManager::Get().GetAsset<MeshSource>(meshHandle);
                    if (meshSource && !meshSource->GetMaterials().empty()) {
                        AssetHandle matHandle = meshSource->GetMaterials()[0];
                        auto matAsset = AssetManager::Get().GetAsset<MaterialAsset>(matHandle);
                        if (matAsset) {
                            matComp.Albedo = matAsset->GetAlbedoColor();
                            matComp.Metallic = matAsset->GetMetallic();
                            matComp.Roughness = matAsset->GetRoughness();
                            matComp.AlbedoMap = matAsset->GetAlbedoMap();
                            matComp.NormalMap = matAsset->GetNormalMap();
                        }
                    }

                    context.Select(entity);

                    std::cout << "[Viewport] Created entity '" << entityName
                              << "' with mesh: " << assetPath << std::endl;
                }
            } else if (type == AssetType::Texture) {
                AssetHandle handle = AssetManager::Get().ImportTexture(assetPath);
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

    // 如果正在进行相机操作或 Gizmo 操作，不进行拾取
    if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT) ||
        (Input::IsKeyPressed(GLFW_KEY_LEFT_ALT) &&
         (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) ||
          Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE))) ||
        ImGuizmo::IsOver() || ImGuizmo::IsUsing()) {
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

        // 转换为视口局部坐标（相对于 Image 的位置）
        float mx = mousePos.x - m_ViewportBounds[0].x;
        float my = mousePos.y - m_ViewportBounds[0].y;

        // 检查是否在视口范围内
        if (mx >= 0 && my >= 0 && mx < m_ViewportSize.x && my < m_ViewportSize.y) {
            // 将视口坐标映射到 Framebuffer 坐标（可能大小不同）
            float scaleX = static_cast<float>(m_Framebuffer->GetWidth()) / m_ViewportSize.x;
            float scaleY = static_cast<float>(m_Framebuffer->GetHeight()) / m_ViewportSize.y;

            int fbX = static_cast<int>(mx * scaleX);
            int fbY = static_cast<int>(my * scaleY);

            // 翻转 Y 坐标（OpenGL 原点在左下角）
            fbY = m_Framebuffer->GetHeight() - fbY - 1;

            // 确保坐标在有效范围内
            fbX = glm::clamp(fbX, 0, static_cast<int>(m_Framebuffer->GetWidth()) - 1);
            fbY = glm::clamp(fbY, 0, static_cast<int>(m_Framebuffer->GetHeight()) - 1);

            // 读取实体 ID（仅在点击时读取）
            int pixelData = m_Framebuffer->ReadPixel(fbX, fbY);
            m_HoveredEntityID = pixelData;
            outEntityID = pixelData;
            hasNewPick = true;

            std::cout << "[Viewport] Clicked at viewport(" << static_cast<int>(mx) << ", "
                      << static_cast<int>(my) << ") -> FBO(" << fbX << ", " << fbY
                      << "), Entity ID: " << pixelData << std::endl;
        } else {
            m_HoveredEntityID = -1;
            outEntityID = -1;
            hasNewPick = true;  // 点击了视口外，也算是一次有效的"空选择"
        }
    }

    wasLeftPressed = isLeftPressed;
    return hasNewPick;
}

void ViewportPanel::DrawGizmo(EditorContext& context) {
    // 检查是否有选中的实体
    if (!context.HasSelection()) {
        return;
    }

    ECS::Entity selectedEntity = context.SelectedEntity;
    if (!selectedEntity.IsValid() || !selectedEntity.HasComponent<ECS::TransformComponent>()) {
        return;
    }

    // 获取变换组件
    auto& transform = selectedEntity.GetComponent<ECS::TransformComponent>();

    // 设置 ImGuizmo
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();

    // 设置 Gizmo 绘制区域
    ImGuizmo::SetRect(
        m_ViewportBounds[0].x,
        m_ViewportBounds[0].y,
        m_ViewportSize.x,
        m_ViewportSize.y
    );

    // 获取相机矩阵
    float aspectRatio = GetAspectRatio();
    glm::mat4 viewMatrix = m_Camera.GetViewMatrix();
    glm::mat4 projectionMatrix = m_Camera.GetProjectionMatrix(aspectRatio);

    // 获取实体的世界变换矩阵
    glm::mat4 entityMatrix = transform.WorldMatrix;

    // 同步 EditorContext 的 Gizmo 模式到 ImGuizmo
    switch (context.CurrentGizmoMode) {
        case EditorContext::GizmoMode::Translate:
            m_GizmoOperation = ImGuizmo::TRANSLATE;
            break;
        case EditorContext::GizmoMode::Rotate:
            m_GizmoOperation = ImGuizmo::ROTATE;
            break;
        case EditorContext::GizmoMode::Scale:
            m_GizmoOperation = ImGuizmo::SCALE;
            break;
    }

    // 同步空间模式
    m_GizmoMode = (context.CurrentGizmoSpace == EditorContext::GizmoSpace::Local)
                  ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    // 处理快捷键切换 Gizmo 模式（同时更新 context）
    if (m_IsFocused || m_IsHovered) {
        if (Input::IsKeyPressed(GLFW_KEY_W) && !Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
            context.CurrentGizmoMode = EditorContext::GizmoMode::Translate;
            m_GizmoOperation = ImGuizmo::TRANSLATE;
        }
        if (Input::IsKeyPressed(GLFW_KEY_E) && !Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
            context.CurrentGizmoMode = EditorContext::GizmoMode::Rotate;
            m_GizmoOperation = ImGuizmo::ROTATE;
        }
        if (Input::IsKeyPressed(GLFW_KEY_R) && !Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
            context.CurrentGizmoMode = EditorContext::GizmoMode::Scale;
            m_GizmoOperation = ImGuizmo::SCALE;
        }
    }

    // 绘制并操控 Gizmo
    float snapValue = 0.5f;  // 对齐值
    if (m_GizmoOperation == ImGuizmo::ROTATE) {
        snapValue = 45.0f;  // 旋转对齐角度
    }

    // 按住 Ctrl 启用对齐
    bool snap = Input::IsKeyPressed(GLFW_KEY_LEFT_CONTROL) || Input::IsKeyPressed(GLFW_KEY_RIGHT_CONTROL);
    float snapValues[3] = {snapValue, snapValue, snapValue};

    ImGuizmo::Manipulate(
        glm::value_ptr(viewMatrix),
        glm::value_ptr(projectionMatrix),
        m_GizmoOperation,
        m_GizmoMode,
        glm::value_ptr(entityMatrix),
        nullptr,
        snap ? snapValues : nullptr
    );

    // 如果 Gizmo 被使用，更新变换
    if (ImGuizmo::IsUsing()) {
        // 分解变换矩阵
        glm::vec3 translation, rotation, scale;
        ImGuizmo::DecomposeMatrixToComponents(
            glm::value_ptr(entityMatrix),
            glm::value_ptr(translation),
            glm::value_ptr(rotation),
            glm::value_ptr(scale)
        );

        // 更新变换组件
        transform.Position = translation;
        transform.SetEulerAngles(rotation);
        transform.Scale = scale;
        transform.Dirty = true;
    }
}

} // namespace GLRenderer
