#include "PrimitivesPanel.h"
#include "editor/EditorContext.h"
#include "scene/ecs/Components.h"
#include <imgui.h>
#include <sstream>

namespace GLRenderer {

PrimitivesPanel::PrimitivesPanel()
    : Panel("Primitives", true)
{
}

void PrimitivesPanel::OnDraw(EditorContext& context) {
    if (!context.World) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "No World set");
        return;
    }

    ImGui::Text("Create Primitive:");
    ImGui::Separator();

    // 3D 基础几何体
    if (ImGui::CollapsingHeader("3D Primitives", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        // Cube
        if (ImGui::Button("Cube", ImVec2(100, 30))) {
            CreateCube(context);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Create a unit cube (1x1x1)");
        }

        // Sphere
        if (ImGui::Button("Sphere", ImVec2(100, 30))) {
            CreateSphere(context);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Create a UV sphere (radius 0.5)");
        }

        // Plane
        if (ImGui::Button("Plane", ImVec2(100, 30))) {
            CreatePlane(context);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Create a XZ plane (1x1)");
        }

        // Cylinder
        if (ImGui::Button("Cylinder", ImVec2(100, 30))) {
            CreateCylinder(context);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Create a cylinder (radius 0.5, height 1)");
        }

        ImGui::Unindent();
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "Tip: Created entities will be selected");
}

void PrimitivesPanel::CreateCube(EditorContext& context) {
    StaticMesh* mesh = MeshFactory::Get().GetCube();
    if (!mesh) return;

    std::string name = GenerateUniqueName(context, "Cube");
    ECS::Entity entity = context.World->CreateEntity(name);

    // Transform
    entity.AddComponent<ECS::TransformComponent>(glm::vec3(0.0f));

    // Mesh
    entity.AddComponent<ECS::MeshComponent>(mesh->GetVertexArray(), mesh->GetVertexCount());

    // Material (使用默认值)
    entity.AddComponent<ECS::MaterialComponent>();

    // 选中新创建的实体
    context.Select(entity);

    m_CubeCount++;
}

void PrimitivesPanel::CreateSphere(EditorContext& context) {
    StaticMesh* mesh = MeshFactory::Get().GetSphere();
    if (!mesh) return;

    std::string name = GenerateUniqueName(context, "Sphere");
    ECS::Entity entity = context.World->CreateEntity(name);

    // Transform
    entity.AddComponent<ECS::TransformComponent>(glm::vec3(0.0f));

    // Mesh - 球体使用索引绘制
    entity.AddComponent<ECS::MeshComponent>(
        mesh->GetVertexArray(),
        mesh->GetVertexCount(),
        mesh->GetIndexCount()
    );

    // Material
    entity.AddComponent<ECS::MaterialComponent>();

    context.Select(entity);

    m_SphereCount++;
}

void PrimitivesPanel::CreatePlane(EditorContext& context) {
    StaticMesh* mesh = MeshFactory::Get().GetPlane();
    if (!mesh) return;

    std::string name = GenerateUniqueName(context, "Plane");
    ECS::Entity entity = context.World->CreateEntity(name);

    // Transform
    entity.AddComponent<ECS::TransformComponent>(glm::vec3(0.0f));

    // Mesh - 平面使用索引绘制
    entity.AddComponent<ECS::MeshComponent>(
        mesh->GetVertexArray(),
        mesh->GetVertexCount(),
        mesh->GetIndexCount()
    );

    // Material
    entity.AddComponent<ECS::MaterialComponent>();

    context.Select(entity);

    m_PlaneCount++;
}

void PrimitivesPanel::CreateCylinder(EditorContext& context) {
    StaticMesh* mesh = MeshFactory::Get().GetCylinder();
    if (!mesh) return;

    std::string name = GenerateUniqueName(context, "Cylinder");
    ECS::Entity entity = context.World->CreateEntity(name);

    // Transform
    entity.AddComponent<ECS::TransformComponent>(glm::vec3(0.0f));

    // Mesh - 圆柱体使用索引绘制
    entity.AddComponent<ECS::MeshComponent>(
        mesh->GetVertexArray(),
        mesh->GetVertexCount(),
        mesh->GetIndexCount()
    );

    // Material
    entity.AddComponent<ECS::MaterialComponent>();

    context.Select(entity);

    m_CylinderCount++;
}

std::string PrimitivesPanel::GenerateUniqueName(EditorContext& context, const std::string& baseName) {
    int count = 0;

    if (baseName == "Cube") count = m_CubeCount;
    else if (baseName == "Sphere") count = m_SphereCount;
    else if (baseName == "Plane") count = m_PlaneCount;
    else if (baseName == "Cylinder") count = m_CylinderCount;

    std::stringstream ss;
    ss << baseName << "_" << count;
    return ss.str();
}

} // namespace GLRenderer
