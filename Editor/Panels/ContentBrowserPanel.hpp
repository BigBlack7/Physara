#pragma once

#include <filesystem>
#include <string>

#include <Editor/Core/EditorContext.hpp>

namespace Physara::Editor
{
    class ContentBrowserPanel final
    {
    public:
        explicit ContentBrowserPanel(EditorContext &context);

        void Draw();

    private:
        bool CanNavigateToParent() const;
        void NavigateToParent();
        void DrawDirectoryTree();
        void DrawDirectoryNode(const std::filesystem::path &directory, int depth);
        void DrawBrowserToolbar();
        void DrawRelativePath();
        void DrawEntryGrid();

        static std::string GetDisplayName(const std::filesystem::path &path);
        static std::string GetRelativeDisplayPath(const std::filesystem::path &path,
                                                  const std::filesystem::path &root);

    private:
        EditorContext &m_Context;
    };
}