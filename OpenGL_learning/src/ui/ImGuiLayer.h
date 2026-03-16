#pragma once

#include "utils/GLCommon.h"
#include <imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

namespace GLRenderer {

// ImGuiLayer - ImGui 生命周期管理
class ImGuiLayer : public NonCopyable {
public:
    ImGuiLayer(GLFWwindow* window);
    ~ImGuiLayer();

    // 每帧调用
    void Begin();
    void End();

    // 设置是否阻止事件传递到应用层
    void BlockEvents(bool block) { m_BlockEvents = block; }
    bool IsBlockingEvents() const { return m_BlockEvents; }

    // 设置主题
    void SetDarkTheme();
    void SetLightTheme();
    void SetClassicTheme();

private:
    bool m_BlockEvents = true;
};

} // namespace GLRenderer

// 兼容旧代码
using ImguiLayer = GLRenderer::ImGuiLayer;
