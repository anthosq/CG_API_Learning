#include "ConsolePanel.h"
#include "editor/EditorContext.h"
#include <imgui.h>
#include <chrono>

namespace GLRenderer {

// 静态成员初始化
std::deque<LogEntry> ConsolePanel::s_Entries;
float ConsolePanel::s_StartTime = 0.0f;

void ConsolePanel::Log(const std::string& message, LogEntry::Level level) {
    if (s_StartTime == 0.0f) {
        auto now = std::chrono::steady_clock::now();
        s_StartTime = static_cast<float>(now.time_since_epoch().count()) / 1e9f;
    }

    auto now = std::chrono::steady_clock::now();
    float timestamp = static_cast<float>(now.time_since_epoch().count()) / 1e9f - s_StartTime;

    // 检查是否与上一条消息相同（折叠重复）
    if (!s_Entries.empty()) {
        auto& last = s_Entries.back();
        if (last.Message == message && last.LogLevel == level) {
            last.Count++;
            return;
        }
    }

    // 添加新条目
    LogEntry entry;
    entry.LogLevel = level;
    entry.Message = message;
    entry.Timestamp = timestamp;
    entry.Count = 1;

    s_Entries.push_back(entry);

    // 限制条目数量
    while (s_Entries.size() > MaxEntries) {
        s_Entries.pop_front();
    }
}

void ConsolePanel::LogInfo(const std::string& message) {
    Log(message, LogEntry::Level::Info);
}

void ConsolePanel::LogWarning(const std::string& message) {
    Log(message, LogEntry::Level::Warning);
}

void ConsolePanel::LogError(const std::string& message) {
    Log(message, LogEntry::Level::Error);
}

void ConsolePanel::LogDebug(const std::string& message) {
    Log(message, LogEntry::Level::Debug);
}

void ConsolePanel::Clear() {
    s_Entries.clear();
}

void ConsolePanel::OnDraw(EditorContext& context) {
    DrawToolbar();
    ImGui::Separator();
    DrawLogEntries();
}

void ConsolePanel::DrawToolbar() {
    // 清除按钮
    if (ImGui::Button("Clear")) {
        Clear();
    }

    ImGui::SameLine();

    // 过滤器复选框
    ImGui::Checkbox("Info", &m_ShowInfo);
    ImGui::SameLine();
    ImGui::Checkbox("Warning", &m_ShowWarnings);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &m_ShowErrors);
    ImGui::SameLine();
    ImGui::Checkbox("Debug", &m_ShowDebug);
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_AutoScroll);

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200);

    // 搜索框
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##Filter", "Filter...", m_FilterBuffer, sizeof(m_FilterBuffer));
}

void ConsolePanel::DrawLogEntries() {
    ImGui::BeginChild("LogEntries", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& entry : s_Entries) {
        // 级别过滤
        switch (entry.LogLevel) {
            case LogEntry::Level::Info:
                if (!m_ShowInfo) continue;
                break;
            case LogEntry::Level::Warning:
                if (!m_ShowWarnings) continue;
                break;
            case LogEntry::Level::Error:
                if (!m_ShowErrors) continue;
                break;
            case LogEntry::Level::Debug:
                if (!m_ShowDebug) continue;
                break;
        }

        // 文本过滤
        if (m_FilterBuffer[0] != '\0') {
            if (entry.Message.find(m_FilterBuffer) == std::string::npos) {
                continue;
            }
        }

        // 获取颜色和图标
        ImVec4 color = GetLevelColor(entry.LogLevel);
        const char* icon = GetLevelIcon(entry.LogLevel);

        // 格式化时间
        char timeStr[16];
        snprintf(timeStr, sizeof(timeStr), "[%.2f]", entry.Timestamp);

        // 绘制条目
        ImGui::PushStyleColor(ImGuiCol_Text, color);

        if (entry.Count > 1) {
            ImGui::Text("%s %s %s (%d)", timeStr, icon, entry.Message.c_str(), entry.Count);
        } else {
            ImGui::Text("%s %s %s", timeStr, icon, entry.Message.c_str());
        }

        ImGui::PopStyleColor();

        // 右键菜单
        if (ImGui::BeginPopupContextItem(entry.Message.c_str())) {
            if (ImGui::MenuItem("Copy")) {
                ImGui::SetClipboardText(entry.Message.c_str());
            }
            ImGui::EndPopup();
        }
    }

    // 自动滚动到底部
    if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

ImVec4 ConsolePanel::GetLevelColor(LogEntry::Level level) const {
    switch (level) {
        case LogEntry::Level::Info:    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        case LogEntry::Level::Warning: return ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
        case LogEntry::Level::Error:   return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        case LogEntry::Level::Debug:   return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        default:                       return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}

const char* ConsolePanel::GetLevelIcon(LogEntry::Level level) const {
    switch (level) {
        case LogEntry::Level::Info:    return "[I]";
        case LogEntry::Level::Warning: return "[W]";
        case LogEntry::Level::Error:   return "[E]";
        case LogEntry::Level::Debug:   return "[D]";
        default:                       return "[?]";
    }
}

} // namespace GLRenderer
