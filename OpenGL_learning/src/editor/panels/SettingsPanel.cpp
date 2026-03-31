#include "SettingsPanel.h"
#include "editor/EditorContext.h"
#include "scene/ecs/Components.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>

namespace GLRenderer {

SettingsPanel::SettingsPanel()
    : Panel("Settings")
{
}

void SettingsPanel::OnDraw(EditorContext& context) {
    if (!m_SceneRenderer) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "No SceneRenderer set");
        return;
    }

    auto& settings = m_SceneRenderer->GetSettings();
    auto& environment = m_SceneRenderer->GetEnvironment();

    // Grid 设置
    if (ImGui::CollapsingHeader("Grid", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show Grid", &settings.ShowGrid);

        if (settings.ShowGrid) {
            ImGui::Indent();
            ImGui::DragFloat("Grid Size", &settings.GridSize, 1.0f, 10.0f, 1000.0f, "%.0f");
            ImGui::DragFloat("Cell Size", &settings.GridCellSize, 0.1f, 0.1f, 10.0f, "%.1f");
            ImGui::Unindent();
        }
    }

    // 环境/IBL 设置
    if (ImGui::CollapsingHeader("Environment (IBL)", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool hasIBL = environment.IBLEnvironment && environment.IBLEnvironment->IsValid();

        // 状态指示
        if (hasIBL) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "IBL: Active");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "IBL: No environment loaded");
        }

        // 拖放区域: 将 .hdr 文件从资产浏览器拖到此处即可加载
        ImGui::PushStyleColor(ImGuiCol_Button,
            hasIBL ? ImVec4(0.15f, 0.35f, 0.15f, 1.0f) : ImVec4(0.25f, 0.2f, 0.1f, 1.0f));
        ImGui::Button("[ Drop .hdr here ]##ibl_drop", ImVec2(-1, 32));
        ImGui::PopStyleColor();
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                const char* path = static_cast<const char*>(payload->Data);
                std::filesystem::path assetPath(path);
                std::string ext = assetPath.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".hdr" || ext == ".exr") {
                    m_SceneRenderer->LoadEnvironment(assetPath);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // 加载 HDR 按钮
        if (ImGui::Button("Load HDR Environment...")) {
            ImGui::OpenPopup("##hdr_path_input");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Default")) {
            const char* defaultHDRs[] = {
                "assets/EnvironmentMaps/puresky_4k.hdr",
                "assets/EnvironmentMaps/meadow_2_2k.hdr",
            };
            bool loaded = false;
            for (const char* p : defaultHDRs) {
                if (std::filesystem::exists(p)) {
                    m_SceneRenderer->LoadEnvironment(p);
                    loaded = true;
                    break;
                }
            }
            if (!loaded) ImGui::OpenPopup("##no_default_hdr");
        }

        // HDR 路径输入弹窗
        if (ImGui::BeginPopup("##hdr_path_input")) {
            static char pathBuf[512] = "assets/EnvironmentMaps/";
            ImGui::Text("HDR Path:");
            ImGui::SetNextItemWidth(400.0f);
            ImGui::InputText("##hdr_path", pathBuf, sizeof(pathBuf));
            if (ImGui::Button("Load") && pathBuf[0] != '\0') {
                m_SceneRenderer->LoadEnvironment(pathBuf);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup("##no_default_hdr")) {
            ImGui::Text("Default HDR not found: assets/EnvironmentMaps/meadow_2_2k.hdr");
            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        // IBL 强度和天空盒设置
        ImGui::Spacing();
        ImGui::SliderFloat("Env Intensity", &environment.EnvironmentIntensity, 0.0f, 5.0f);
        ImGui::Checkbox("Show Skybox", &settings.ShowSkybox);
        if (settings.ShowSkybox) {
            ImGui::Indent();
            ImGui::SliderFloat("Skybox LOD", &settings.SkyboxLOD, 0.0f, 8.0f, "%.1f");
            ImGui::Unindent();
        }
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 统计场景中的光源
        int dirLightCount = 0;
        ECS::Entity firstDirLight;

        if (context.World) {
            context.World->Each<ECS::DirectionalLightComponent>(
                [&](ECS::Entity entity, ECS::DirectionalLightComponent& light) {
                    if (dirLightCount == 0) {
                        firstDirLight = entity;
                    }
                    dirLightCount++;
                }
            );
        }

        ImGui::Text("Directional Lights: %d", dirLightCount);
        ImGui::Text("Point Lights: %d", environment.PointLightCount);

        ImGui::Spacing();

        if (dirLightCount == 0) {
            if (ImGui::Button("Create Directional Light") && context.World) {
                ECS::Entity newLight = context.World->CreateEntity("Directional Light");
                auto& transform = newLight.AddComponent<ECS::TransformComponent>();
                transform.Rotation = glm::radians(glm::vec3(-45.0f, -30.0f, 0.0f));
                auto& light = newLight.AddComponent<ECS::DirectionalLightComponent>();
                light.Color = glm::vec3(1.0f);
                light.Intensity = 1.0f;
                context.Select(newLight);
            }
        } else if (firstDirLight.IsValid()) {
            if (ImGui::Button("Select Directional Light")) {
                context.Select(firstDirLight);
            }
        }

        ImGui::Separator();
        ImGui::ColorEdit3("Ambient Color", &settings.AmbientColor[0]);
    }

    // 渲染设置
    if (ImGui::CollapsingHeader("Rendering")) {
        ImGui::Checkbox("Wireframe",          &settings.Wireframe);
        ImGui::Checkbox("Depth Pre-Pass",     &settings.EnableDepthPrepass);
        ImGui::Checkbox("Deferred Shading",   &settings.UseDeferredShading);
    }

    // 阴影设置
    if (ImGui::CollapsingHeader("Shadows")) {
        const char* resItems[] = { "1024", "2048", "4096" };
        const uint32_t resValues[] = { 1024, 2048, 4096 };
        int resIndex = 1;
        for (int i = 0; i < 3; i++) {
            if (settings.ShadowResolution == resValues[i]) { resIndex = i; break; }
        }
        if (ImGui::Combo("Resolution", &resIndex, resItems, 3))
            settings.ShadowResolution = resValues[resIndex];

        const char* cascadeItems[] = { "2", "4" };
        const uint32_t cascadeValues[] = { 2, 4 };
        int cascadeIndex = 1;
        for (int i = 0; i < 2; i++) {
            if (settings.ShadowCascadeCount == cascadeValues[i]) { cascadeIndex = i; break; }
        }
        if (ImGui::Combo("Cascades", &cascadeIndex, cascadeItems, 2))
            settings.ShadowCascadeCount = cascadeValues[cascadeIndex];

        ImGui::SliderFloat("Distance",     &settings.ShadowDistance,     10.0f, 500.0f, "%.0f");
        ImGui::SliderFloat("Fade",         &settings.ShadowFade,          1.0f,  50.0f, "%.1f");
        ImGui::SliderFloat("Split Lambda", &settings.CascadeSplitLambda,  0.0f,   1.0f, "%.2f");
        ImGui::Checkbox("Soft Shadows",    &settings.SoftShadows);
        if (settings.SoftShadows)
            ImGui::SliderFloat("Light Size",   &settings.ShadowLightSize, 0.0f,   2.0f, "%.2f");
        ImGui::Checkbox("Show Cascades",   &settings.ShowCascades);
    }

    // 点光源阴影设置
    if (ImGui::CollapsingHeader("Point Shadows")) {
        ImGui::Checkbox("Enable##PointShadow", &settings.EnablePointShadows);
        if (settings.EnablePointShadows) {
            const char* resItems[]        = { "256", "512", "1024" };
            const uint32_t resValues[]    = { 256, 512, 1024 };
            int resIdx = 1;
            for (int i = 0; i < 3; i++) {
                if (settings.PointShadowResolution == resValues[i]) { resIdx = i; break; }
            }
            if (ImGui::Combo("Resolution##PSM", &resIdx, resItems, 3))
                settings.PointShadowResolution = resValues[resIdx];
            ImGui::SliderFloat("Far Plane##PSM", &settings.PointShadowFarPlane, 5.0f, 200.0f, "%.0f");
        }
    }

    // Bloom 设置
    if (ImGui::CollapsingHeader("Bloom")) {
        ImGui::Checkbox("Enable",            &settings.EnableBloom);
        if (settings.EnableBloom) {
            ImGui::SliderFloat("Threshold",  &settings.BloomThreshold, 0.1f, 4.0f, "%.2f");
            ImGui::SliderFloat("Knee",       &settings.BloomKnee,      0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Intensity",  &settings.BloomIntensity, 0.0f, 3.0f, "%.2f");
        }
    }

    // SSAO 设置
    if (ImGui::CollapsingHeader("SSAO")) {
        if (settings.EnableGTAO)
            ImGui::TextDisabled("(disabled — GTAO is active)");
        else {
            ImGui::Checkbox("Enable##ssao", &settings.EnableSSAO);
            if (settings.EnableSSAO) {
                ImGui::SliderFloat("Radius",        &settings.SSAORadius,        0.1f,  2.0f,  "%.2f");
                ImGui::SliderFloat("Bias",          &settings.SSAOBias,          0.001f, 0.1f, "%.3f");
                ImGui::SliderInt  ("Kernel Size",   &settings.SSAOKernelSize,    8,     64);
                ImGui::SliderFloat("Blur Sharpness",&settings.SSAOBlurSharpness, 0.1f, 10.0f, "%.1f");
            }
        }
    }

    // GTAO 设置（开启时替代 SSAO）
    if (ImGui::CollapsingHeader("GTAO")) {
        ImGui::Checkbox("Enable##gtao", &settings.EnableGTAO);
        if (settings.EnableGTAO) {
            ImGui::Spacing();
            ImGui::SliderFloat("Radius",            &settings.GTAORadius,           0.1f,  2.0f,  "%.2f");
            ImGui::SliderFloat("Falloff Range",     &settings.GTAOFalloffRange,     0.1f,  1.0f,  "%.3f");
            ImGui::SliderFloat("Radius Multiplier", &settings.GTAORadiusMultiplier, 0.5f,  3.0f,  "%.3f");
            ImGui::SliderFloat("Value Power",       &settings.GTAOFinalValuePower,  0.5f,  5.0f,  "%.2f");
            ImGui::SliderFloat("Sample Dist Power", &settings.GTAOSampleDistPower,  1.0f,  4.0f,  "%.2f");
            ImGui::SliderFloat("Depth MIP Offset",  &settings.GTAODepthMIPOffset,   0.0f,  6.0f,  "%.1f");
            ImGui::SliderFloat("Denoise Beta",      &settings.GTAODenoiseBlurBeta,  0.0f,  5.0f,  "%.2f");

            ImGui::Spacing();
            ImGui::Checkbox("Show AO Buffer", &m_ShowAODebug);
        }
    }

    // AO 调试窗口（独立浮动，避免挤压 Settings 面板布局）
    if (m_ShowAODebug && m_SceneRenderer) {
        ImGui::SetNextWindowSize(ImVec2(400, 430), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("AO Buffer", &m_ShowAODebug)) {
            uint32_t texID = m_SceneRenderer->GetSSAODebugTexture();
            if (texID != 0) {
                ImVec2 avail  = ImGui::GetContentRegionAvail();
                float  aspect = (float)m_SceneRenderer->GetRenderWidth()
                              / (float)m_SceneRenderer->GetRenderHeight();
                float  w = avail.x;
                float  h = w / aspect;
                if (h > avail.y) { h = avail.y; w = h * aspect; }
                ImGui::Image((ImTextureID)(uintptr_t)texID,
                             ImVec2(w, h),
                             ImVec2(0, 1), ImVec2(1, 0));
            } else {
                ImGui::TextDisabled("(not available — enable SSAO or GTAO first)");
            }
        }
        ImGui::End();
    }

    // 描边设置
    if (ImGui::CollapsingHeader("Outline")) {
        ImGui::Checkbox("Enable",         &settings.EnableOutline);
        ImGui::ColorEdit4("Color",        glm::value_ptr(settings.OutlineColor));
        ImGui::SliderInt("Width (px)",    &settings.OutlineWidth, 1, 5);
    }

    // SSR 设置
    if (ImGui::CollapsingHeader("SSR")) {
        ImGui::Checkbox("Enable##ssr",          &settings.EnableSSR);
        if (settings.EnableSSR) {
            ImGui::Checkbox  ("Half Resolution",    &settings.SSRHalfResolution);
            ImGui::SliderFloat("Intensity",         &settings.SSRIntensity,      0.0f, 2.0f,  "%.2f");
            ImGui::SliderInt  ("Max Steps",         &settings.SSRMaxSteps,       16,   128);
            ImGui::SliderFloat("Depth Tol",         &settings.SSRDepthTolerance, 0.01f, 1.0f, "%.3f");
            ImGui::SliderFloat("Fade Start",        &settings.SSRFadeStart,      0.5f, 1.0f,  "%.2f");
            ImGui::SliderFloat("Facing Fade",       &settings.SSRFacingFade,     0.0f, 1.0f,  "%.2f");
        }
    }

    // 流体渲染设置
    if (ImGui::CollapsingHeader("Fluid")) {
        ImGui::Checkbox("Enable SSFR##fluid",        &settings.EnableFluidRendering);
        ImGui::Checkbox("Particle Debug View##fluid", &settings.ShowFluidParticles);
        if (settings.EnableFluidRendering && !settings.ShowFluidParticles) {
            ImGui::Indent();
            if (ImGui::CollapsingHeader("Depth & NRF##fluid")) {
                ImGui::SliderFloat("Sprite Scale##fluid",         &settings.FluidRenderRadiusScale,        0.5f, 5.0f,  "%.2f");
                ImGui::SliderFloat("Emitter Sprite Scale##fluid", &settings.FluidEmitterRenderRadiusScale, 1.0f, 20.0f, "%.1f");
                ImGui::SliderInt  ("NRF Radius##fluid",      &settings.FluidBlurRadius,        1,    40);
                ImGui::SliderFloat("NRF SigmaS##fluid",      &settings.FluidBlurSigmaS,        1.0f, 20.0f, "%.1f");
                ImGui::SliderFloat("NRF Threshold##fluid",   &settings.FluidNRFThreshold,      0.001f, 0.1f, "%.4f");
                ImGui::SliderInt  ("Cleanup Radius##fluid",  &settings.FluidNRFCleanupRadius,  1,    10);
            }
            if (ImGui::CollapsingHeader("Shading##fluid")) {
                ImGui::SliderFloat("Thickness Scale##fluid", &settings.FluidThicknessScale,  0.1f, 10.0f, "%.2f");
                ImGui::SliderFloat("Min Thickness##fluid",   &settings.FluidMinThickness,    0.0f, 0.2f,  "%.4f");
                ImGui::SliderFloat3("Extinction##fluid",     &settings.FluidExtinction.x,    0.0f, 5.0f,  "%.3f");
                ImGui::SliderFloat("Refract##fluid",         &settings.FluidRefractStrength, 0.0f, 0.1f,  "%.3f");
                ImGui::ColorEdit3 ("Water Color##fluid",     &settings.FluidWaterColor.x);
            }
            ImGui::Unindent();
        }
    }

    // 裁剪统计
    if (ImGui::CollapsingHeader("Statistics")) {
        const auto& stats = m_SceneRenderer->GetCullingStats();
        ImGui::Text("Total Objects:   %d", stats.TotalObjects);
        ImGui::Text("Visible:         %d", stats.VisibleObjects);
        ImGui::Text("Culled:          %d", stats.CulledObjects);
        ImGui::Text("BVH Nodes:       %d", stats.BVHNodeCount);
    }
}

} // namespace GLRenderer
