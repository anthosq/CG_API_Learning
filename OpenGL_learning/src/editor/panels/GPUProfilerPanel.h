#pragma once

#include "Panel.h"
#include "renderer/SceneRenderer.h"
#include <imgui.h>

namespace GLRenderer {

class GPUProfilerPanel : public Panel {
public:
    GPUProfilerPanel() : Panel("GPU Profiler") { SetOpen(false); }
    ~GPUProfilerPanel() override = default;

    void SetSceneRenderer(SceneRenderer* r) { m_SceneRenderer = r; }

    void OnDraw(EditorContext&) override {
        if (!m_SceneRenderer) {
            ImGui::TextDisabled("No SceneRenderer");
            return;
        }

        // Enable queries as soon as the window opens; disable when it closes.
        // OnDraw is only called while the panel is open (Panel::Draw guards it),
        // but we also hook into SetOpen via the panel close button (m_IsOpen is
        // set to false by ImGui::Begin when the user clicks ✕).
        if (!m_SceneRenderer->GetGPUStats().IsEnabled())
            m_SceneRenderer->SetGPUProfilingEnabled(true);

        const auto& gpu = m_SceneRenderer->GetGPUStats();

        constexpr ImGuiTableFlags kFlags =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

        ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_FirstUseEver);

        if (ImGui::BeginTable("GPUTimings", 3, kFlags)) {
            ImGui::TableSetupColumn("Pass",   ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("ms",     ImGuiTableColumnFlags_WidthFixed, 56.0f);
            ImGui::TableSetupColumn("%",      ImGuiTableColumnFlags_WidthFixed, 44.0f);
            ImGui::TableHeadersRow();

            const double totalMs = gpu.TotalFrame.GetMs();

            auto row = [&](const char* label, const GPUTimer& t) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1);
                if (t.IsValid())
                    ImGui::Text("%.3f", t.GetMs());
                else
                    ImGui::TextDisabled("---");
                ImGui::TableSetColumnIndex(2);
                if (t.IsValid() && totalMs > 0.0)
                    ImGui::Text("%.1f%%", t.GetMs() / totalMs * 100.0);
                else
                    ImGui::TextDisabled("---");
            };

            row("Total Frame",        gpu.TotalFrame);
            ImGui::TableNextRow();  // separator row
            row("  Shadow",           gpu.Shadow);
            row("  GBuffer / Geo",    gpu.GBuffer);
            row("  AO",               gpu.AO);
            row("  Lighting",         gpu.Lighting);
            row("  SSR",              gpu.SSR);
            row("  Fluid",            gpu.Fluid);
            row("  Post-process",     gpu.PostProcess);

            ImGui::EndTable();
        }

        ImGui::TextDisabled("Values are 1-frame delayed (double-buffered queries).");

        // ImGui sets m_IsOpen = false on the frame the user clicks the close (x) button.
        // OnDraw is still called once that frame, so we can detect it here and
        // disable queries so there is zero overhead while the window is closed.
        if (!IsOpen())
            m_SceneRenderer->SetGPUProfilingEnabled(false);
    }

private:
    SceneRenderer* m_SceneRenderer = nullptr;
};

} // namespace GLRenderer
