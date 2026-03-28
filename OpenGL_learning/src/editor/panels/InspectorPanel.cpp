#include "InspectorPanel.h"
#include "editor/EditorContext.h"
#include "asset/AssetManager.h"
#include "asset/MaterialAsset.h"
#include "graphics/Texture.h"
#include "graphics/MeshSource.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "renderer/ParticleSystem.h"
#include "physics/FluidSimulation.h"
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

    // ParticleComponent
    if (entity.HasComponent<ECS::ParticleComponent>()) {
        if (DrawComponentHeader<ECS::ParticleComponent>(entity, "Particle System")) {
            DrawParticleComponent(entity.GetComponent<ECS::ParticleComponent>());
        }
    }

    // FluidComponent
    if (entity.HasComponent<ECS::FluidComponent>()) {
        if (DrawComponentHeader<ECS::FluidComponent>(entity, "Fluid Simulation")) {
            DrawFluidComponent(entity.GetComponent<ECS::FluidComponent>());
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
    // srgb=true 用于颜色贴图(albedo)，加载时使用 GL_SRGB8 让 GPU 自动线性化采样值
    auto DrawTextureSlot = [](const char* label, AssetHandle handle,
                              std::function<void(AssetHandle)> onChanged,
                              std::function<void()> onClear,
                              bool srgb = false) {
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
                    TextureSpec spec;
                    spec.SRGB = srgb;
                    AssetHandle newHandle = AssetManager::Get().ImportTexture(assetPath, spec);
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
            [&]() { matAsset->SetAlbedoMap(AssetHandle(0)); },
            true);  // sRGB: 颜色贴图需要 GPU 自动线性化

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
    ImGui::NextColumn();

    ImGui::Separator();
    ImGui::Text("Cast Shadows");
    ImGui::NextColumn();
    ImGui::Checkbox("##CastShadows", &light.CastShadows);
    ImGui::NextColumn();

    if (light.CastShadows) {
        ImGui::Text("Shadow Far");
        ImGui::NextColumn();
        ImGui::DragFloat("##ShadowFar", &light.ShadowFarPlane, 0.5f, 1.0f, 200.0f, "%.1f");
        ImGui::NextColumn();
    }

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

        if (!entity.HasComponent<ECS::ParticleComponent>()) {
            if (ImGui::MenuItem("Particle System")) {
                entity.AddComponent<ECS::ParticleComponent>();
                ImGui::CloseCurrentPopup();
            }
        }

        if (!entity.HasComponent<ECS::FluidComponent>()) {
            if (ImGui::MenuItem("Fluid Simulation")) {
                auto& f = entity.AddComponent<ECS::FluidComponent>();
                // 用 Transform 初始化仿真域（若无 Transform 则保留默认值）
                if (entity.HasComponent<ECS::TransformComponent>()) {
                    const auto& tr = entity.GetComponent<ECS::TransformComponent>();
                    const float hw = tr.Scale.x * 0.5f;
                    const float hd = tr.Scale.z * 0.5f;
                    f.BoundaryMin = { tr.Position.x - hw, tr.Position.y,              tr.Position.z - hd };
                    f.BoundaryMax = { tr.Position.x + hw, tr.Position.y + tr.Scale.y, tr.Position.z + hd };
                }
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }
}

void InspectorPanel::DrawParticleComponent(ECS::ParticleComponent& p) {
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 120.0f);

    // --- Emitter ---
    ImGui::Text("Emit Rate");    ImGui::NextColumn();
    ImGui::DragFloat("##EmitRate", &p.EmitRate, 1.0f, 0.0f, 10000.0f, "%.0f/s");
    ImGui::NextColumn();

    ImGui::Text("Direction");   ImGui::NextColumn();
    ImGui::DragFloat3("##EmitDir", &p.EmitDirection.x, 0.01f, -1.0f, 1.0f);
    ImGui::NextColumn();

    float spreadDeg = glm::degrees(p.EmitSpread);
    ImGui::Text("Spread");      ImGui::NextColumn();
    if (ImGui::DragFloat("##Spread", &spreadDeg, 0.5f, 0.0f, 90.0f, "%.1f deg"))
        p.EmitSpread = glm::radians(spreadDeg);
    ImGui::NextColumn();

    // --- Lifetime ---
    ImGui::Text("Lifetime Min"); ImGui::NextColumn();
    ImGui::DragFloat("##LifeMin", &p.LifetimeMin, 0.05f, 0.01f, p.LifetimeMax);
    ImGui::NextColumn();

    ImGui::Text("Lifetime Max"); ImGui::NextColumn();
    ImGui::DragFloat("##LifeMax", &p.LifetimeMax, 0.05f, p.LifetimeMin, 30.0f);
    ImGui::NextColumn();

    // --- Velocity ---
    ImGui::Text("Speed Min");   ImGui::NextColumn();
    ImGui::DragFloat("##SpeedMin", &p.SpeedMin, 0.05f, 0.0f, p.SpeedMax);
    ImGui::NextColumn();

    ImGui::Text("Speed Max");   ImGui::NextColumn();
    ImGui::DragFloat("##SpeedMax", &p.SpeedMax, 0.05f, p.SpeedMin, 100.0f);
    ImGui::NextColumn();

    ImGui::Text("Gravity");     ImGui::NextColumn();
    ImGui::DragFloat3("##Gravity", &p.Gravity.x, 0.1f);
    ImGui::NextColumn();

    // --- Appearance ---
    ImGui::Text("Color Begin"); ImGui::NextColumn();
    ImGui::ColorEdit4("##ColorBegin", &p.ColorBegin.x);
    ImGui::NextColumn();

    ImGui::Text("Color End");   ImGui::NextColumn();
    ImGui::ColorEdit4("##ColorEnd", &p.ColorEnd.x);
    ImGui::NextColumn();

    ImGui::Text("Size Begin");  ImGui::NextColumn();
    ImGui::DragFloat("##SizeBegin", &p.SizeBegin, 0.01f, 0.0f, 10.0f);
    ImGui::NextColumn();

    ImGui::Text("Size End");    ImGui::NextColumn();
    ImGui::DragFloat("##SizeEnd", &p.SizeEnd, 0.01f, 0.0f, 10.0f);
    ImGui::NextColumn();

    // --- Runtime ---
    ImGui::Text("Max Particles"); ImGui::NextColumn();
    int maxP = p.MaxParticles;
    if (ImGui::DragInt("##MaxParticles", &maxP, 64, 64, 1048576)) {
        p.MaxParticles = maxP;
        p.RuntimeSystem = nullptr;  // 参数变化时重建 GPU buffer
    }
    ImGui::NextColumn();

    ImGui::Columns(1);

    ImGui::Checkbox("Looping", &p.Looping);
    ImGui::SameLine();
    ImGui::Checkbox("Playing", &p.Playing);

    // 运行时信息
    if (p.RuntimeSystem) {
        ImGui::TextDisabled("Alive: %d / %d",
            p.RuntimeSystem->GetAliveCount(), p.MaxParticles);
    } else {
        ImGui::TextDisabled("(not initialized)");
    }
}

void InspectorPanel::DrawFluidComponent(ECS::FluidComponent& f) {
    const float colW = 130.0f;
    bool reset = false;   // 参数变化时重建模拟

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, colW);

    // 粒子参数
    ImGui::SeparatorText("Particle");

    ImGui::Text("Radius");        ImGui::NextColumn();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Particle radius in meters (ref: 0.01m).\nKernel radius = 4x particle radius.");
    if (ImGui::DragFloat("##Radius", &f.ParticleRadius, 0.001f, 0.005f, 0.1f, "%.3f m"))
        reset = true;
    ImGui::NextColumn();

    ImGui::Text("Rest Density");  ImGui::NextColumn();
    if (ImGui::DragFloat("##RestDensity", &f.RestDensity, 1.0f, 100.0f, 5000.0f, "%.0f kg/m³"))
        reset = true;
    ImGui::NextColumn();

    ImGui::Text("Max Particles"); ImGui::NextColumn();
    if (ImGui::DragInt("##MaxParticles", &f.MaxParticles, 512, 512, 524288, "%d"))
        reset = true;
    ImGui::NextColumn();

    // 求解器参数
    ImGui::SeparatorText("Solver");

    ImGui::Text("Iterations");    ImGui::NextColumn();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("PBF constraint solver iterations per substep (4 is typical)");
    ImGui::DragInt("##SolverIters", &f.SolverIters, 1, 1, 16);
    ImGui::NextColumn();

    ImGui::Text("Substeps (min)"); ImGui::NextColumn();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Minimum substeps per frame.\nSystem auto-increases to keep sub-dt <= 2ms (~ref 1.6ms).\nAt 60fps: auto ~9 substeps.");
    ImGui::DragInt("##Substeps", &f.Substeps, 1, 1, 32);
    ImGui::NextColumn();

    // 效果参数（运行时可调，不需要重建）
    ImGui::SeparatorText("Effects");

    ImGui::Text("Viscosity");     ImGui::NextColumn();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("XSPH viscosity coefficient c (ref default: 0.01).\nFormula: dv = c * (1/rho0) * sum W");
    ImGui::DragFloat("##Viscosity", &f.Viscosity, 0.001f, 0.0f, 0.5f, "%.3f");
    ImGui::NextColumn();

    ImGui::Text("Vorticity");     ImGui::NextColumn();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Vorticity confinement strength (0 = disabled).\nCompensates numerical damping.");
    ImGui::DragFloat("##VorticityEps", &f.VorticityEps, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::NextColumn();

    ImGui::Columns(1);

    // 仿真域（由 TransformComponent 驱动，这里只读显示）
    ImGui::SeparatorText("Domain (from Transform)");
    ImGui::TextDisabled("Position = domain floor center");
    ImGui::TextDisabled("Scale    = domain width x height x depth");
    glm::vec3 size = f.BoundaryMax - f.BoundaryMin;
    ImGui::TextDisabled("Current:  %.2f x %.2f x %.2f m", size.x, size.y, size.z);
    ImGui::TextDisabled("Min: (%.2f, %.2f, %.2f)", f.BoundaryMin.x, f.BoundaryMin.y, f.BoundaryMin.z);
    ImGui::TextDisabled("Max: (%.2f, %.2f, %.2f)", f.BoundaryMax.x, f.BoundaryMax.y, f.BoundaryMax.z);

    // 运行时信息
    ImGui::SeparatorText("Runtime");
    if (f.Runtime && f.Runtime->IsReady()) {
        int N = f.Runtime->GetParticleCount();
        ImGui::TextDisabled("Particles: %d / %d", N, f.MaxParticles);
        ImGui::TextDisabled("Cells:     %d", f.Runtime->GetCellCount());
        auto g = f.Runtime->GetGridDims();
        ImGui::TextDisabled("Grid:      %dx%dx%d", g.x, g.y, g.z);
        ImGui::TextDisabled("Mass:      %.6f kg  (6.4 r^3 rho0)", 6.4f * f.ParticleRadius * f.ParticleRadius * f.ParticleRadius * f.RestDensity);
        ImGui::TextDisabled("Kernel r:  %.3f m  (4x particle r)", f.ParticleRadius * 4.0f);
        // neighborIdx SSBO 内存 = N × MAX_NEIGHBORS × 4B
        float neighborMB = N * 64 * 4 / (1024.0f * 1024.0f);
        ImGui::TextDisabled("NeighborBuf: %.1f MB  (N x 64 x 4)", neighborMB);
    } else {
        ImGui::TextDisabled("(not initialized — starts on Play)");
    }

    // 重置按钮
    ImGui::Spacing();
    if (ImGui::Button("Reset Simulation") || reset)
        f.Runtime = nullptr;
}

// 显式实例化模板
template bool InspectorPanel::DrawComponentHeader<ECS::TagComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::TransformComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::MeshComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::PointLightComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::DirectionalLightComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::RotatorComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::FloatingComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::ParticleComponent>(ECS::Entity, const char*);
template bool InspectorPanel::DrawComponentHeader<ECS::FluidComponent>(ECS::Entity, const char*);

} // namespace GLRenderer
