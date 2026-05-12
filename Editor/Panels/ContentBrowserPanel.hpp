#pragma once

#include <filesystem>
#include <string>

#include <Editor/Core/EditorContext.hpp>
#include <Editor/Core/IconManager.hpp>
#include <Engine/Resource/AssetManager.hpp>

namespace Physara::Editor
{
    class ContentBrowserPanel final
    {
    public:
        ContentBrowserPanel(EditorContext &context, const IconManager &iconManager, Physara::Engine::AssetManager &assetManager);

        void Draw();

    private:
        bool CanNavigateToParent() const;
        void NavigateToParent();
        void DrawDirectoryTree();
        void DrawDirectoryNode(const std::filesystem::path &directory, int depth);
        void DrawBrowserToolbar();
        void DrawRelativePath();
        void DrawEntryGrid();
        void RequestSceneLoad(const std::filesystem::path &path);
        void RequestModelLoad(const std::filesystem::path &path);
        void DrawLoadSceneConfirmation();
        void DrawLoadModelConfirmation();

        static std::string GetDisplayName(const std::filesystem::path &path);
        static std::string GetRelativeDisplayPath(const std::filesystem::path &path,
                                                  const std::filesystem::path &root);

    private:
        EditorContext &m_Context;
        const IconManager &m_IconManager;
        Physara::Engine::AssetManager &m_AssetManager;
        std::filesystem::path m_PendingSceneLoadPath{};
        std::filesystem::path m_PendingModelLoadPath{};
        bool m_OpenLoadScenePopup{false};
        bool m_OpenLoadModelPopup{false};
    };
}