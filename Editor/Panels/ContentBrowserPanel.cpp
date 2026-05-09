#include "ContentBrowserPanel.hpp"

#include <filesystem>

#include <imgui/imgui.h>

#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr const char *PanelName = "Content Browser";
    }

    ContentBrowserPanel::ContentBrowserPanel(EditorContext &context) : m_Context(context) {}

    void ContentBrowserPanel::Draw()
    {
        ImGui::Begin(Internal::PanelName);

        if (m_Context.assetsRootPath.empty())
        {
            ImGui::TextUnformatted("Assets root is not initialized.");
            ImGui::End();
            return;
        }

        if (m_Context.currentContentPath.empty())
        {
            m_Context.currentContentPath = m_Context.assetsRootPath;
        }

        if (ImGui::Button("Up") && CanNavigateToParent())
        {
            NavigateToParent();
        }

        ImGui::SameLine();
        ImGui::Text("Path: %s", m_Context.currentContentPath.string().c_str());

        ImGui::Separator();

        const std::vector<std::string> entries =
            Platform::FileSystem::ListDirectory(m_Context.currentContentPath.string());

        for (const std::string &entry : entries)
        {
            const std::filesystem::path entryPath = m_Context.currentContentPath / entry;
            const bool isDirectory = Platform::FileSystem::IsDirectory(entryPath.string());

            const char *prefix = isDirectory ? "[Dir]" : "[File]";
            ImGui::Text("%s %s", prefix, entry.c_str());

            if (isDirectory && ImGui::IsItemHovered() &&
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                m_Context.currentContentPath = entryPath.lexically_normal();
            }
        }

        ImGui::End();
    }

    bool ContentBrowserPanel::CanNavigateToParent() const
    {
        return m_Context.currentContentPath != m_Context.assetsRootPath;
    }

    void ContentBrowserPanel::NavigateToParent()
    {
        if (!CanNavigateToParent())
        {
            return;
        }

        m_Context.currentContentPath =
            m_Context.currentContentPath.parent_path().lexically_normal();
    }
}