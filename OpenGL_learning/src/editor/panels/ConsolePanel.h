#pragma once

#include "Panel.h"
#include <vector>
#include <string>
#include <deque>

namespace GLRenderer {

// 日志条目
struct LogEntry {
    enum class Level { Info, Warning, Error, Debug };

    Level LogLevel;
    std::string Message;
    float Timestamp;
    int Count = 1;  // 用于折叠重复消息
};

class ConsolePanel : public Panel {
public:
    ConsolePanel() : Panel("Console") {}

    // 静态日志接口
    static void Log(const std::string& message, LogEntry::Level level = LogEntry::Level::Info);
    static void LogInfo(const std::string& message);
    static void LogWarning(const std::string& message);
    static void LogError(const std::string& message);
    static void LogDebug(const std::string& message);
    static void Clear();

protected:
    void OnDraw(EditorContext& context) override;

private:
    void DrawToolbar();
    void DrawLogEntries();

    ImVec4 GetLevelColor(LogEntry::Level level) const;
    const char* GetLevelIcon(LogEntry::Level level) const;

    // 过滤器
    bool m_ShowInfo = true;
    bool m_ShowWarnings = true;
    bool m_ShowErrors = true;
    bool m_ShowDebug = false;
    bool m_AutoScroll = true;

    char m_FilterBuffer[256] = "";

    // 日志存储
    static constexpr size_t MaxEntries = 500;
    static std::deque<LogEntry> s_Entries;
    static float s_StartTime;
};

} // namespace GLRenderer
