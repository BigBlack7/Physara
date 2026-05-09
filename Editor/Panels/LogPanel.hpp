#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Physara::Editor
{
    enum class LogPanelLevel
    {
        Trace,
        Info,
        Warning,
        Error,
        Critical,
        Unknown
    };

    struct LogPanelLine
    {
        std::string text{};
        LogPanelLevel level{LogPanelLevel::Unknown};
    };

    class LogPanel final
    {
    public:
        LogPanel() = default;

        void Draw();

    private:
        void Refresh();
        void DrawToolbar();
        void DrawLogLines();

        bool PassesSearch(const LogPanelLine &line) const;
        static LogPanelLevel DetectLevel(std::string_view line);

    private:
        std::vector<LogPanelLine> m_CachedLogs{};
        std::string m_LastClearedLogLine{};
        char m_SearchBuffer[128]{};
        bool m_AutoScroll{true};
    };
}