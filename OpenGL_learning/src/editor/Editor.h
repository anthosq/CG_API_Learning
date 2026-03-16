#pragma once

// Editor.h - 主编辑器类
//
// Editor 管理所有编辑器面板和共享状态。
// 是编辑器系统的入口点。
//

#include "EditorContext.h"
#include "panels/Panel.h"
#include "utils/GLCommon.h"

#include <vector>
#include <memory>

namespace GLRenderer {

class Editor : public NonCopyable {
public:
    Editor();
    ~Editor();

    // 初始化编辑器（传入 ECS World）
    void Initialize(ECS::World* world);

    // 关闭编辑器
    void Shutdown();

    // 每帧更新（可选，用于处理快捷键等）
    void OnUpdate(float deltaTime);

    // 渲染所有面板
    void OnImGuiRender();

    // 获取上下文
    EditorContext& GetContext() { return m_Context; }
    const EditorContext& GetContext() const { return m_Context; }

    // 面板访问
    template<typename T>
    T* GetPanel() {
        for (auto& panel : m_Panels) {
            if (auto* p = dynamic_cast<T*>(panel.get())) {
                return p;
            }
        }
        return nullptr;
    }

    // 设置面板可见性
    void SetPanelOpen(const std::string& name, bool open);

    // 添加自定义面板
    void AddPanel(std::unique_ptr<Panel> panel) {
        m_Panels.push_back(std::move(panel));
    }

    // 获取所有面板
    const std::vector<std::unique_ptr<Panel>>& GetPanels() const { return m_Panels; }

private:
    void DrawMenuBar();
    void DrawToolbar();

    EditorContext m_Context;
    std::vector<std::unique_ptr<Panel>> m_Panels;

    bool m_ShowDemoWindow = false;
};

} // namespace GLRenderer
