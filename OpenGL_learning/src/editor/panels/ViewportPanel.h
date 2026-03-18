#pragma once

//
// ViewportPanel 负责：
// 1. 持有独立的 Framebuffer 用于场景渲染
// 2. 将渲染结果作为 ImGui::Image 显示
// 3. 处理视口相机控制
// 4. 管理视口大小和 FBO 调整
// 5. ImGuizmo 变换操控
//

#include "Panel.h"
#include "graphics/Framebuffer.h"
#include "graphics/Camera.h"

#include <memory>
#include <glm/glm.hpp>
#include <ImGuizmo.h>

namespace GLRenderer {

class ViewportPanel : public Panel {
public:
    ViewportPanel();
    ~ViewportPanel() override = default;

    // 获取视口 Framebuffer（供外部渲染使用）
    Framebuffer* GetFramebuffer() { return m_Framebuffer.get(); }
    const Framebuffer* GetFramebuffer() const { return m_Framebuffer.get(); }

    // 获取视口相机
    Camera& GetCamera() { return m_Camera; }
    const Camera& GetCamera() const { return m_Camera; }

    // 视口状态
    bool IsFocused() const { return m_IsFocused; }
    bool IsHovered() const { return m_IsHovered; }

    // 视口大小
    glm::vec2 GetSize() const { return m_ViewportSize; }
    float GetAspectRatio() const;

    // 处理相机输入（在 Update 中调用）
    void ProcessCameraInput(float deltaTime);
    void OnResize(float width, float height);

    // 鼠标拾取 - 返回是否有新的拾取结果
    int GetHoveredEntityID() const { return m_HoveredEntityID; }
    bool ProcessMousePicking(int& outEntityID);

    // Gizmo 操作模式
    ImGuizmo::OPERATION GetGizmoOperation() const { return m_GizmoOperation; }
    void SetGizmoOperation(ImGuizmo::OPERATION op) { m_GizmoOperation = op; }

protected:
    void OnDraw(EditorContext& context) override;
    ImGuiWindowFlags GetWindowFlags() const override;

private:
    void ResizeFramebufferIfNeeded(float width, float height);
    void DrawGizmo(EditorContext& context);

    std::unique_ptr<Framebuffer> m_Framebuffer;
    Camera m_Camera;

    glm::vec2 m_ViewportSize = {1280.0f, 720.0f};
    glm::vec2 m_ViewportBounds[2];  // 用于鼠标拾取

    bool m_IsFocused = false;
    bool m_IsHovered = false;
    bool m_CameraControlEnabled = false;

    int m_HoveredEntityID = -1;

    // Gizmo 设置
    ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE m_GizmoMode = ImGuizmo::LOCAL;
};

} // namespace GLRenderer
