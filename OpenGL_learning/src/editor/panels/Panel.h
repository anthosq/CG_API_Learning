#pragma once

//
// 所有编辑器面板继承此基类，提供统一的接口：
// - Draw(): 渲染面板
// - IsOpen()/SetOpen(): 控制面板可见性
//

#include <string>
#include <imgui.h>

namespace GLRenderer {

// 前向声明
struct EditorContext;

class Panel {
public:
    Panel(const std::string& name, bool defaultOpen = true)
        : m_Name(name), m_IsOpen(defaultOpen) {}

    virtual ~Panel() = default;

    // 渲染面板（如果打开）
    void Draw(EditorContext& context) {
        if (!m_IsOpen) return;

        ImGuiWindowFlags flags = GetWindowFlags();

        // 设置窗口大小约束（可选）
        ImVec2 minSize = GetMinSize();
        ImVec2 maxSize = GetMaxSize();
        if (minSize.x > 0 || minSize.y > 0 || maxSize.x > 0 || maxSize.y > 0) {
            ImGui::SetNextWindowSizeConstraints(minSize, maxSize);
        }

        if (ImGui::Begin(m_Name.c_str(), &m_IsOpen, flags)) {
            OnDraw(context);
        }
        ImGui::End();
    }

    // 面板状态
    bool IsOpen() const { return m_IsOpen; }
    void SetOpen(bool open) { m_IsOpen = open; }
    void Toggle() { m_IsOpen = !m_IsOpen; }

    // 获取面板名称
    const std::string& GetName() const { return m_Name; }

protected:
    // 子类实现此方法来渲染面板内容
    virtual void OnDraw(EditorContext& context) = 0;

    // 子类可重写以自定义窗口标志
    virtual ImGuiWindowFlags GetWindowFlags() const {
        return ImGuiWindowFlags_None;
    }

    // 子类可重写以设置窗口大小约束
    virtual ImVec2 GetMinSize() const { return ImVec2(0, 0); }
    virtual ImVec2 GetMaxSize() const { return ImVec2(FLT_MAX, FLT_MAX); }

    std::string m_Name;
    bool m_IsOpen;
};

} // namespace GLRenderer
