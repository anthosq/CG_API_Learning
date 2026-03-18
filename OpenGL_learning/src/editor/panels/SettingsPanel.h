#pragma once

#include "Panel.h"
#include "renderer/SceneRenderer.h"

namespace GLRenderer {

class SettingsPanel : public Panel {
public:
    SettingsPanel();
    ~SettingsPanel() override = default;

    void OnDraw(EditorContext& context) override;

    // 设置 SceneRenderer 引用
    void SetSceneRenderer(SceneRenderer* renderer) { m_SceneRenderer = renderer; }

private:
    SceneRenderer* m_SceneRenderer = nullptr;
};

} // namespace GLRenderer
