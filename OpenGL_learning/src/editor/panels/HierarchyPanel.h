#pragma once

#include "Panel.h"
#include "scene/ecs/ECS.h"

namespace GLRenderer {

class HierarchyPanel : public Panel {
public:
    HierarchyPanel() : Panel("Hierarchy") {}

protected:
    void OnDraw(EditorContext& context) override;

private:
    void DrawEntityNode(EditorContext& context, ECS::Entity entity);
    void DrawContextMenu(EditorContext& context);

    char m_SearchBuffer[256] = "";
};

} // namespace GLRenderer
