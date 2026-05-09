#include "LogPanel.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

#include <Engine/Core/Log.hpp>
#include <imgui/imgui.h>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr const char *PanelName = "Log";
        constexpr const char *ScrollRegionName = "LogScrollRegion";

        bool ContainsIgnoreCase(std::string_view text, std::string_view pattern)
        {
            if (pattern.empty())
            {
                return true;
            }

            auto lower = [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            };

            return std::search(
                       text.begin(),
                       text.end(),
                       pattern.begin(),
                       pattern.end(),
                       [lower](char lhs, char rhs)
                       {
                           return lower(static_cast<unsigned char>(lhs)) == lower(static_cast<unsigned char>(rhs));
                       }) != text.end();
        }

        ImVec4 LevelColor(LogPanelLevel level)
        {
            switch (level)
            {
            case LogPanelLevel::Trace:
                return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
            case LogPanelLevel::Info:
                return ImVec4(0.68f, 0.92f, 0.72f, 1.0f);
            case LogPanelLevel::Warning:
                return ImVec4(1.0f, 0.78f, 0.28f, 1.0f);
            case LogPanelLevel::Error:
                return ImVec4(1.0f, 0.34f, 0.32f, 1.0f);
            case LogPanelLevel::Critical:
                return ImVec4(1.0f, 0.18f, 0.18f, 1.0f);
            case LogPanelLevel::Unknown:
                return ImGui::GetStyleColorVec4(ImGuiCol_Text);
            }

            return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        }
    }

    void LogPanel::Draw()
    {
        ImGui::Begin(Internal::PanelName);

        DrawToolbar();
        ImGui::Separator();
        DrawLogLines();

        ImGui::End();
    }

    void LogPanel::Refresh()
    {
        const std::vector<std::string> recentLogs = Engine::Log::GetRecentLogs();
        std::size_t firstVisibleLog = 0;
        if (!m_LastClearedLogLine.empty())
        {
            const auto lastClearedIt = std::find(recentLogs.rbegin(), recentLogs.rend(), m_LastClearedLogLine);
            if (lastClearedIt != recentLogs.rend())
            {
                firstVisibleLog = static_cast<std::size_t>(std::distance(lastClearedIt, recentLogs.rend()));
            }
            else
            {
                m_LastClearedLogLine.clear();
            }
        }

        m_CachedLogs.clear();
        m_CachedLogs.reserve(recentLogs.size() - firstVisibleLog);

        for (std::size_t index = firstVisibleLog; index < recentLogs.size(); ++index)
        {
            const std::string &line = recentLogs[index];
            m_CachedLogs.push_back(LogPanelLine{
                line,
                DetectLevel(line)});
        }
    }

    void LogPanel::DrawToolbar()
    {
        if (ImGui::Button("Refresh"))
        {
            Refresh();
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear View"))
        {
            const std::vector<std::string> recentLogs = Engine::Log::GetRecentLogs();
            m_LastClearedLogLine = recentLogs.empty() ? std::string{} : recentLogs.back();
            m_CachedLogs.clear();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto Scroll", &m_AutoScroll);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(220.0f);
        ImGui::InputText("Search", m_SearchBuffer, sizeof(m_SearchBuffer));
    }

    void LogPanel::DrawLogLines()
    {
        Refresh();

        ImGui::BeginChild(Internal::ScrollRegionName, ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

        for (const LogPanelLine &line : m_CachedLogs)
        {
            if (!PassesSearch(line))
            {
                continue;
            }

            ImGui::PushStyleColor(ImGuiCol_Text, Internal::LevelColor(line.level));
            ImGui::TextUnformatted(line.text.c_str());
            ImGui::PopStyleColor();
        }

        if (m_AutoScroll)
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
    }

    bool LogPanel::PassesSearch(const LogPanelLine &line) const
    {
        return Internal::ContainsIgnoreCase(line.text, m_SearchBuffer);
    }

    LogPanelLevel LogPanel::DetectLevel(std::string_view line)
    {
        if (line.find("[trace]") != std::string_view::npos)
        {
            return LogPanelLevel::Trace;
        }
        if (line.find("[info]") != std::string_view::npos)
        {
            return LogPanelLevel::Info;
        }
        if (line.find("[warning]") != std::string_view::npos || line.find("[warn]") != std::string_view::npos)
        {
            return LogPanelLevel::Warning;
        }
        if (line.find("[error]") != std::string_view::npos || line.find("[err]") != std::string_view::npos)
        {
            return LogPanelLevel::Error;
        }
        if (line.find("[critical]") != std::string_view::npos || line.find("[fatal]") != std::string_view::npos)
        {
            return LogPanelLevel::Critical;
        }

        return LogPanelLevel::Unknown;
    }
}