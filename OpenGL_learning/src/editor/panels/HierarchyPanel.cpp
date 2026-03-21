#include "HierarchyPanel.h"
#include "editor/EditorContext.h"
#include <imgui.h>

namespace GLRenderer {

void HierarchyPanel::OnDraw(EditorContext& context) {
    if (!context.World) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "No World assigned");
        return;
    }

    // 搜索框
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##Search", "Search entities...", m_SearchBuffer, sizeof(m_SearchBuffer));

    ImGui::Separator();

    // 实体列表
    context.World->Each<ECS::TagComponent>([&](ECS::Entity entity, ECS::TagComponent& tag) {
        // 搜索过滤
        if (m_SearchBuffer[0] != '\0') {
            if (tag.Name.find(m_SearchBuffer) == std::string::npos &&
                tag.Tag.find(m_SearchBuffer) == std::string::npos) {
                return;
            }
        }

        DrawEntityNode(context, entity);
    });

    // 右键菜单（在空白处）
    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
        DrawContextMenu(context);
        ImGui::EndPopup();
    }

    // 点击空白处取消选择
    if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
        context.ClearSelection();
    }
}

void HierarchyPanel::DrawEntityNode(EditorContext& context, ECS::Entity entity) {
    auto& tag = entity.GetComponent<ECS::TagComponent>();

    // 节点标志
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_Leaf;  // 暂不支持层级

    // 如果是选中的实体，高亮显示
    if (context.SelectedEntity.IsValid() &&
        context.SelectedEntity.GetHandle() == entity.GetHandle()) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // 渲染树节点
    bool opened = ImGui::TreeNodeEx(
        reinterpret_cast<void*>(static_cast<uint64_t>(static_cast<uint32_t>(entity.GetHandle()))),
        flags,
        "%s",
        tag.Name.c_str()
    );

    // 点击选择
    if (ImGui::IsItemClicked()) {
        context.Select(entity);
    }

    // 右键菜单
    if (ImGui::BeginPopupContextItem()) {
        context.Select(entity);

        if (ImGui::MenuItem("Delete")) {
            context.World->DestroyEntity(entity);
            context.ClearSelection();
        }

        if (ImGui::MenuItem("Duplicate")) {
            // TODO: 实现实体复制
        }

        ImGui::EndPopup();
    }

    // 显示标签（如果有）
    if (!tag.Tag.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]", tag.Tag.c_str());
    }

    if (opened) {
        // 如果将来支持层级，在这里递归渲染子实体
        ImGui::TreePop();
    }
}

void HierarchyPanel::DrawContextMenu(EditorContext& context) {
    if (ImGui::MenuItem("Create Empty Entity")) {
        ECS::Entity newEntity = context.World->CreateEntity("New Entity");
        newEntity.AddComponent<ECS::TransformComponent>();
        context.Select(newEntity);
    }

    if (ImGui::BeginMenu("Create...")) {
        if (ImGui::MenuItem("Cube")) {
            ECS::Entity entity = context.World->CreateEntity("Cube");
            entity.AddComponent<ECS::TransformComponent>();
            entity.AddComponent<ECS::MeshComponent>();
            context.Select(entity);
        }

        if (ImGui::MenuItem("Point Light")) {
            ECS::Entity entity = context.World->CreateEntity("Point Light");
            entity.AddComponent<ECS::TransformComponent>();
            entity.AddComponent<ECS::PointLightComponent>();
            context.Select(entity);
        }

        if (ImGui::MenuItem("Directional Light")) {
            ECS::Entity entity = context.World->CreateEntity("Directional Light");
            entity.AddComponent<ECS::TransformComponent>();
            entity.AddComponent<ECS::DirectionalLightComponent>();
            context.Select(entity);
        }

        ImGui::EndMenu();
    }
}

} // namespace GLRenderer
