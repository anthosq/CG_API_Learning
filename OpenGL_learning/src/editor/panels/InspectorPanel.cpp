#include "InspectorPanel.h"
#include "editor/EditorContext.h"
#include "asset/AssetManager.h"
#include "asset/MaterialAsset.h"
#include "graphics/Texture.h"
#include "graphics/MeshSource.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <functional>

namespace GLRenderer {

static bool DrawVec3Control(const char* label, glm::vec3& value, float resetValue = 0.0f, float speed = 0.1f) {
    bool changed = false;

    ImGui::PushID(label);

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 100.0f);
    ImGui::Text("%s", label);
    ImGui::NextColumn();

    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    float lineHeight = ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 2.0f;
    ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

    // X
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
    if (ImGui::Button("X", buttonSize)) {
        value.x = resetValue;
        changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (ImGui::DragFloat("##X", &value.x, speed)) changed = true;
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Y
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
    if (ImGui::Button("Y", buttonSize)) {
        value.y = resetValue;
        changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (ImGui::DragFloat("##Y", &value.y, speed)) changed = true;
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Z
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.35f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
    if (ImGui::Button("Z", buttonSize)) {
        value.z = resetValue;
        changed = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (ImGui::DragFloat("##Z", &value.z, speed)) changed = true;
    ImGui::PopItemWidth();

    ImGui::PopStyleVar();
    ImGui::Columns(1);
    ImGui::PopID();

    return changed;
}

static bool DrawColorControl(const char* label, glm::vec3& color) {
    ImGui::PushID(label);

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 100.0f);
    ImGui::Text("%s", label);
    ImGui::NextColumn();

    bool changed = ImGui::ColorEdit3("##Color", glm::value_ptr(color));

    ImGui::Columns(1);
    ImGui::PopID();

    return changed;
}

void InspectorPanel::OnDraw(EditorContext& context) {
    if (!context.HasSelection()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No entity selected");
        return;
    }

    ECS::Entity entity = context.SelectedEntity;

    // 确保实体仍然有效
    if (!entity.IsValid()) {
        context.ClearSelection();
        return;
    }

    // TagComponent（总是存在）
    if (entity.HasComponent<ECS::TagComponent>()) {
        DrawTagComponent(entity.GetComponent<ECS::TagComponent>());
    }

    ImGui::Separator();

    // TransformComponent
    if (entity.HasComponent<ECS::TransformComponent>()) {
        if (DrawComponentHeader<ECS::TransformComponent>(entity, "Transform")) {
            DrawTransformComponent(entity.GetComponent<ECS::TransformComponent>());
        }
    }

    // MeshComponent (包含材质编辑)
    if (entity.HasComponent<ECS::MeshComponent>()) {
        if (DrawComponentHeader<ECS::MeshComponent>(entity, "Mesh")) {
            DrawMeshComponent(entity.GetComponent<ECS::MeshComponent>(), entity);
        }
    }

    // PointLightComponent
    if (entity.HasComponent<ECS::PointLightComponent>()) {
        if (DrawComponentHeader<ECS::PointLightComponent>(entity, "Point Light")) {
            DrawPointLightComponent(entity.GetComponent<ECS::PointLightComponent>());
        }
    }

    // DirectionalLightComponent
    if (entity.HasComponent<ECS::DirectionalLightComponent>()) {
        if (DrawComponentHeader<ECS::DirectionalLightComponent>(entity, "Directional Light")) {
            DrawDirectionalLightComponent(entity.GetComponent<ECS::DirectionalLightComponent>());
        }
    }

    // RotatorComponent
    if (entity.HasComponent<ECS::RotatorComponent>()) {
        if (DrawComponentHeader<ECS::RotatorComponent>(entity, "Rotator")) {
            DrawRotatorComponent(entity.GetComponent<ECS::RotatorComponent>());
        }
    }

    // FloatingComponent
    if (entity.HasComponent<ECS::FloatingComponent>()) {
        if (DrawComponentHeader<ECS::FloatingComponent>(entity, "Floating")) {
            DrawFloatingComponent(entity.GetComponent<ECS::FloatingComponent>());
        }
    }

    ImGui::Separator();

    // 添加组件按钮
    DrawAddComponentButton(entity);
}

void InspectorPanel::DrawTagComponent(ECS::TagComponent& tag) {
    char buffer[256];
    memset(buffer, 0, sizeof(buffer));
    strncpy_s(buffer, tag.Name.c_str(), sizeof(buffer) - 1);

    ImGui::Text("Name");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##Name", buffer, sizeof(buffer))) {
        tag.Name = buffer;
    }

    // 显示标签
    if (!tag.Tag.empty()) {
        ImGui::Text("Tag: %s", tag.Tag.c_str());
    }
}

void InspectorPanel::DrawTransformComponent(ECS::TransformComponent& transform) {
    bool changed = false;

    // Position
    if (DrawVec3Control("Position", transform.Position)) {
        transform.Dirty = true;
        changed = true;
    }

    // Rotation (欧拉角)
    glm::vec3 euler = transform.GetEulerAngles();
    if (DrawVec3Control("Rotation", euler, 0.0f, 1.0f)) {
        transform.SetEulerAngles(euler);
        changed = true;
    }

    // Scale
    if (DrawVec3Control("Scale", transform.Scale, 1.0f)) {
        transform.Dirty = true;
        changed = true;
    }
}

void InspectorPanel::DrawMeshComponent(ECS::MeshComponent& mesh, ECS::Entity entity) {
    ImGui::Text("Mesh Handle: %s", mesh.MeshHandle.IsValid() ? "Valid" : "None");
    ImGui::Checkbox("Visible", &mesh.Visible);

    if (!mesh.MeshHandle.IsValid()) return;

    // 获取 MeshSource 以了解 submesh 数量
    Ref<MeshSource> meshSource;
    auto staticMesh = AssetManager::Get().GetAsset<StaticMesh>(mesh.MeshHandle);
    if (staticMesh) {
        meshSource = AssetManager::Get().GetAsset<MeshSource>(staticMesh->GetMeshSource());
    } else {
        meshSource = AssetManager::Get().GetAsset<MeshSource>(mesh.MeshHandle);
    }

    if (!meshSource) return;

    uint32_t submeshCount = meshSource->GetSubmeshCount();
    ImGui::Text("Submeshes: %u", submeshCount);

    ImGui::Separator();
    ImGui::Text("Materials");
    ImGui::Spacing();

    // 确保 MaterialTable 存在
    if (!mesh.Materials) {
        if (ImGui::Button("Create Material Overrides")) {
            mesh.Materials = MaterialTable::Create(submeshCount);
            // 从 MeshSource 复制已有材质，没有的创建默认材质
            const auto& defaultMaterials = meshSource->GetMaterials();
            for (uint32_t i = 0; i < submeshCount; ++i) {
                if (i < defaultMaterials.size() && defaultMaterials[i].IsValid()) {
                    mesh.Materials->SetMaterial(i, defaultMaterials[i]);
                } else {
                    // 创建默认 PBR 材质
                    auto defaultMat = MaterialAsset::Create();
                    defaultMat->SetAlbedoColor(glm::vec3(0.8f));
                    defaultMat->SetMetallic(0.0f);
                    defaultMat->SetRoughness(0.5f);
                    AssetHandle matHandle = AssetManager::Get().AddMemoryOnlyAsset(defaultMat);
                    mesh.Materials->SetMaterial(i, matHandle);
                }
            }
        }
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Using default materials");
        return;
    }

    // 绘制每个材质槽
    for (uint32_t i = 0; i < submeshCount; ++i) {
        ImGui::PushID(static_cast<int>(i));

        const auto& submeshes = meshSource->GetSubmeshes();
        const char* submeshName = i < submeshes.size() ? submeshes[i].MeshName.c_str() : "Unknown";

        if (ImGui::TreeNode("Material", "Slot %u: %s", i, submeshName)) {
            AssetHandle matHandle = mesh.Materials->GetMaterial(i);
            auto matAsset = AssetManager::Get().GetAsset<MaterialAsset>(matHandle);

            if (matAsset) {
                DrawMaterialEditor(matAsset);
            } else {
                ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "No material assigned");
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    // 清除覆盖按钮
    if (ImGui::Button("Clear Material Overrides")) {
        mesh.Materials = nullptr;
    }
}

void InspectorPanel::DrawMaterialEditor(Ref<MaterialAsset> matAsset) {
    if (!matAsset) return;

    // 纹理槽绘制辅助 lambda
    auto DrawTextureSlot = [](const char* label, AssetHandle handle,
                              std::function<void(AssetHandle)> onChanged,
                              std::function<void()> onClear) {
        ImGui::PushID(label);

        float textureSize = 64.0f;
        auto texture = AssetManager::Get().GetAsset<Texture2D>(handle);
        bool hasTexture = texture && texture->IsValid();

        // 先绘制标签
        ImGui::Text("%s", label);

        // 保存纹理预览的位置（用于放置 X 按钮）
        ImVec2 textureCursorPos = ImGui::GetCursorPos();

        // 纹理预览
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        if (hasTexture) {
            ImGui::Image((ImTextureID)(intptr_t)texture->GetID(),
                        ImVec2(textureSize, textureSize), ImVec2(0, 1), ImVec2(1, 0));
        } else {
            // 棋盘格占位符（用按钮模拟）
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::Button("Drop", ImVec2(textureSize, textureSize));
            ImGui::PopStyleColor(2);
        }
        ImGui::PopStyleVar();

        // 拖放接收
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                const char* path = static_cast<const char*>(payload->Data);
                std::filesystem::path assetPath(path);
                std::string ext = assetPath.extension().string();
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".dds") {
                    AssetHandle newHandle = AssetManager::Get().ImportTexture(assetPath);
                    if (newHandle.IsValid() && onChanged) {
                        onChanged(newHandle);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // 悬停时显示大图 tooltip
        if (ImGui::IsItemHovered() && hasTexture) {
            ImGui::BeginTooltip();
            ImGui::Image((ImTextureID)(intptr_t)texture->GetID(),
                        ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::Text("%dx%d", texture->GetWidth(), texture->GetHeight());
            ImGui::EndTooltip();
        }

        // X 清除按钮（左上角）
        ImVec2 nextRowPos = ImGui::GetCursorPos();
        if (hasTexture) {
            ImGui::SetCursorPos(textureCursorPos);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 0.8f));
            if (ImGui::Button("X", ImVec2(18, 18))) {
                if (onClear) onClear();
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            ImGui::SetCursorPos(nextRowPos);
        }

        ImGui::PopID();
    };

    // === Albedo ===
    if (ImGui::CollapsingHeader("Albedo", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawTextureSlot("Albedo Map", matAsset->GetAlbedoMap(),
            [&](AssetHandle h) { matAsset->SetAlbedoMap(h); },
            [&]() { matAsset->SetAlbedoMap(AssetHandle(0)); });

        glm::vec3 albedo = matAsset->GetAlbedoColor();
        if (ImGui::ColorEdit3("Color", &albedo[0])) {
            matAsset->SetAlbedoColor(albedo);
        }
    }

    // === Normal ===
    if (ImGui::CollapsingHeader("Normal", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawTextureSlot("Normal Map", matAsset->GetNormalMap(),
            [&](AssetHandle h) {
                matAsset->SetNormalMap(h);
                matAsset->SetUseNormalMap(true);
            },
            [&]() {
                matAsset->SetNormalMap(AssetHandle(0));
                matAsset->SetUseNormalMap(false);
            });

        bool useNormalMap = matAsset->IsUsingNormalMap();
        if (ImGui::Checkbox("Use Normal Map", &useNormalMap)) {
            matAsset->SetUseNormalMap(useNormalMap);
        }
    }

    // === Metallic ===
    if (ImGui::CollapsingHeader("Metallic")) {
        DrawTextureSlot("Metallic Map", matAsset->GetMetallicMap(),
            [&](AssetHandle h) { matAsset->SetMetallicMap(h); },
            [&]() { matAsset->SetMetallicMap(AssetHandle(0)); });

        float metallic = matAsset->GetMetallic();
        if (ImGui::SliderFloat("##MetallicValue", &metallic, 0.0f, 1.0f)) {
            matAsset->SetMetallic(metallic);
        }
    }

    // === Roughness ===
    if (ImGui::CollapsingHeader("Roughness")) {
        DrawTextureSlot("Roughness Map", matAsset->GetRoughnessMap(),
            [&](AssetHandle h) { matAsset->SetRoughnessMap(h); },
            [&]() { matAsset->SetRoughnessMap(AssetHandle(0)); });

        float roughness = matAsset->GetRoughness();
        if (ImGui::SliderFloat("##RoughnessValue", &roughness, 0.0f, 1.0f)) {
            matAsset->SetRoughness(roughness);
        }
    }

    // === AO ===
    if (ImGui::CollapsingHeader("Ambient Occlusion")) {
        DrawTextureSlot("AO Map", matAsset->GetAOMap(),
            [&](AssetHandle h) { matAsset->SetAOMap(h); },
            [&]() { matAsset->SetAOMap(AssetHandle(0)); });
    }
}

void InspectorPanel::DrawPointLightComponent(ECS::PointLightComponent& light) {
    DrawColorControl("Color", light.Color);

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 100.0f);

    ImGui::Text("Intensity");
    ImGui::NextColumn();
    ImGui::DragFloat("##Intensity", &light.Intensity, 0.1f, 0.0f, 100.0f);
    ImGui::NextColumn();

    ImGui::Text("Constant");
    ImGui::NextColumn();
    ImGui::DragFloat("##Constant", &light.Constant, 0.01f, 0.0f, 10.0f);
    ImGui::NextColumn();

    ImGui::Text("Linear");
    ImGui::NextColumn();
    ImGui::DragFloat("##Linear", &light.Linear, 0.001f, 0.0f, 1.0f);
    ImGui::NextColumn();

    ImGui::Text("Quadratic");
    ImGui::NextColumn();
    ImGui::DragFloat("##Quadratic", &light.Quadratic, 0.001f, 0.0f, 1.0f);
    ImGui::NextColumn();

    // 显示计算出的范围（只读）
    float range = light.GetRange();
    ImGui::Text("Range");
    ImGui::NextColumn();
    ImGui::Text("%.2f", range);

    ImGui::Columns(1);
}

void InspectorPanel::DrawDirectionalLightComponent(ECS::DirectionalLightComponent& light) {
    // 提示：方向由 Transform 旋转控制
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Direction: Controlled by Transform rotation");
    ImGui::Spacing();

    DrawColorControl("Color", light.Color);

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 100.0f);

    ImGui::Text("Intensity");
    ImGui::NextColumn();
    ImGui::DragFloat("##Intensity", &light.Intensity, 0.1f, 0.0f, 10.0f);

    ImGui::Columns(1);
}

void InspectorPanel::DrawRotatorComponent(ECS::RotatorComponent& rotator) {
    DrawVec3Control("Axis", rotator.Axis);

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 100.0f);

    ImGui::Text("Speed");
    ImGui::NextColumn();
    ImGui::DragFloat("##Speed", &rotator.Speed, 1.0f, -360.0f, 360.0f);

    ImGui::Columns(1);
}

void InspectorPanel::DrawFloatingComponent(ECS::FloatingComponent& floating) {
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 100.0f);

    ImGui::Text("Amplitude");
    ImGui::NextColumn();
    ImGui::DragFloat("##Amplitude", &floating.Amplitude, 0.1f, 0.0f, 10.0f);
    ImGui::NextColumn();

    ImGui::Text("Frequency");
    ImGui::NextColumn();
    ImGui::DragFloat("##Frequency", &floating.Frequency, 0.1f, 0.0f, 10.0f);
    ImGui::NextColumn();

    ImGui::Text("Phase");
    ImGui::NextColumn();
    ImGui::DragFloat("##Phase", &floating.Phase, 0.1f, 0.0f, 6.28f);

    ImGui::Columns(1);
}

template<typename T>
bool InspectorPanel::DrawComponentHeader(ECS::Entity entity, const char* name) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen |
                                ImGuiTreeNodeFlags_Framed |
                                ImGuiTreeNodeFlags_SpanAvailWidth |
                                ImGuiTreeNodeFlags_AllowItemOverlap |
                                ImGuiTreeNodeFlags_FramePadding;

    ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    float lineHeight = ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 2.0f;
    ImGui::Separator();

    bool open = ImGui::TreeNodeEx(name, flags);
    ImGui::PopStyleVar();

    // 删除按钮（右对齐）
    ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
    if (ImGui::Button("X", ImVec2(lineHeight, lineHeight))) {
        // 不能删除 TagComponent
        if constexpr (!std::is_same_v<T, ECS::TagComponent>) {
            entity.RemoveComponent<T>();
        }
    }

    if (open) {
        ImGui::TreePop();
    }

    return open && entity.HasComponent<T>();
}

void InspectorPanel::DrawAddComponentButton(ECS::Entity entity) {
    float width = ImGui::GetContentRegionAvail().x;

    ImGui::SetCursorPosX((width - 200.0f) * 0.5f);
    if (ImGui::Button("Add Component", ImVec2(200.0f, 0.0f))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (!entity.HasComponent<ECS::TransformComponent>()) {
            if (ImGui::MenuItem("Transform")) {
                entity.AddComponent<ECS::TransformComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        if (!entity.HasComponent<ECS::MeshComponent>()) {
            if (ImGui::MenuItem("Mesh")) {
                entity.AddComponent<ECS::MeshComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        // 注: 光源组件通过 PrimitivesPanel 或专用面板创建，不在此处添加

        if (!entity.HasComponent<ECS::RotatorComponent>()) {
            if (ImGui::MenuItem("Rotator")) {
                entity.AddComponent<ECS::RotatorComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        if (!entity.HasComponent<ECS::FloatingComponent>()) {
            if (ImGui::MenuItem("Floating")) {
                entity.AddComponent<ECS::FloatingComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }
}

// 显式实例化模板
template bool InspectorPanel::DrawComponentHeader<ECS::TagComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::TransformComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::MeshComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::PointLightComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::DirectionalLightComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::RotatorComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::FloatingComponent>(ECS::Entity, const char*);

} // namespace GLRenderer
