#include "LogPanel.hpp"

#include <Engine/Core/Log.hpp>
#include <imgui/imgui.h>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr const char *PanelName = "Log";
    }

    void LogPanel::Draw()
    {
        ImGui::Begin(Internal::PanelName);

        if (ImGui::Button("Refresh"))
        {
            Refresh();
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear View"))
        {
            m_CachedLogs.clear();
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto Scroll", &m_AutoScroll);

        ImGui::Separator();

        if (m_CachedLogs.empty())
        {
            Refresh();
        }

        ImGui::BeginChild("LogScrollRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);

        for (const std::string &line : m_CachedLogs)
        {
            ImGui::TextUnformatted(line.c_str());
        }

        if (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();

        ImGui::End();
    }

    void LogPanel::Refresh()
    {
        m_CachedLogs = Engine::Log::GetRecentLogs();
    }
}