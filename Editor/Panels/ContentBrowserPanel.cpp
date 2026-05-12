#include "ContentBrowserPanel.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include <imgui/imgui.h>

#include <Engine/Core/Log.hpp>
#include <Engine/Resource/AssetPath.hpp>
#include <Engine/Resource/Loaders/GLTFLoader.hpp>
#include <Engine/Scene/Scene.hpp>
#include <Engine/Scene/SceneSerializer.hpp>
#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr const char *PanelName = "Content Browser";
        constexpr float DirectoryTreeWidth = 100.f;
        constexpr float TileSize = 92.f;
        constexpr float TileIconSize = 42.f;
        constexpr float TilePadding = 8.f;
        constexpr float TreeArrowWidth = 14.f;
        constexpr float TreeLabelSpacing = 4.f;

        struct BrowserEntry
        {
            std::string name{};
            std::filesystem::path path{};
            Engine::AssetPath::AssetKind kind{Engine::AssetPath::AssetKind::File};
            bool isDirectory{false};
        };

        std::filesystem::path ClampToAssetsRoot(const std::filesystem::path &path,
                                                const std::filesystem::path &root)
        {
            return Platform::FileSystem::ClampToRoot(path.string(), root.string());
        }

        bool IsInsideModelsDirectory(const std::filesystem::path &path, const std::filesystem::path &assetsRoot)
        {
            return Platform::FileSystem::IsSubPath((assetsRoot / "Models").string(), path.string());
        }

        ImU32 IconColor(Engine::AssetPath::AssetKind kind)
        {
            using enum Engine::AssetPath::AssetKind;
            switch (kind)
            {
            case Folder:
                return IM_COL32(185, 158, 84, 255);
            case Scene:
                return IM_COL32(96, 139, 119, 255);
            case Mesh:
                return IM_COL32(112, 127, 158, 255);
            case Texture:
                return IM_COL32(139, 120, 160, 255);
            case Shader:
                return IM_COL32(119, 140, 86, 255);
            case File:
                return IM_COL32(118, 128, 121, 255);
            }

            return IM_COL32(118, 128, 121, 255);
        }

        void DrawDisclosureTriangle(ImDrawList *drawList, const ImVec2 &min, float size, bool open, ImU32 color)
        {
            const float half = size * 0.5f;
            const ImVec2 center(min.x + half, min.y + half);

            if (open)
            {
                drawList->AddTriangleFilled(
                    ImVec2(center.x - 4.f, center.y - 2.f),
                    ImVec2(center.x + 4.f, center.y - 2.f),
                    ImVec2(center.x, center.y + 4.f),
                    color);
            }
            else
            {
                drawList->AddTriangleFilled(
                    ImVec2(center.x - 2.f, center.y - 4.f),
                    ImVec2(center.x - 2.f, center.y + 4.f),
                    ImVec2(center.x + 4.f, center.y),
                    color);
            }
        }

        std::string TileName(const BrowserEntry &entry)
        {
            if (entry.kind == Engine::AssetPath::AssetKind::Scene)
            {
                const std::string name = entry.name;
                constexpr std::string_view sceneSuffix = ".scene.json";
                if (name.size() > sceneSuffix.size() &&
                    Platform::FileSystem::NormalizeForCompare(name).ends_with(sceneSuffix))
                {
                    return name.substr(0, name.size() - sceneSuffix.size());
                }
            }

            return entry.path.stem().string().empty() || entry.isDirectory
                       ? entry.name
                       : entry.path.stem().string();
        }

        std::vector<BrowserEntry> CollectEntries(const std::filesystem::path &directory)
        {
            std::vector<BrowserEntry> entries{};
            const std::vector<std::string> names = Platform::FileSystem::ListDirectory(directory.string());
            entries.reserve(names.size());

            for (const std::string &name : names)
            {
                const std::filesystem::path path = (directory / name).lexically_normal();
                const bool isDirectory = Platform::FileSystem::IsDirectory(path.string());
                entries.push_back(BrowserEntry{
                    name,
                    path,
                    Engine::AssetPath::Classify(path, isDirectory),
                    isDirectory});
            }

            std::sort(entries.begin(), entries.end(), [](const BrowserEntry &lhs, const BrowserEntry &rhs)
                      {
                          if (lhs.isDirectory != rhs.isDirectory)
                          {
                              return lhs.isDirectory;
                          }
                          return Platform::FileSystem::NormalizeForCompare(lhs.name) <
                                 Platform::FileSystem::NormalizeForCompare(rhs.name);
                      });

            return entries;
        }
    }

    ContentBrowserPanel::ContentBrowserPanel(EditorContext &context, const IconManager &iconManager,
                                             Engine::AssetManager &assetManager)
        : m_Context(context), m_IconManager(iconManager), m_AssetManager(assetManager) {}

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
        else
        {
            m_Context.currentContentPath =
                Internal::ClampToAssetsRoot(m_Context.currentContentPath, m_Context.assetsRootPath);
        }

        ImGui::BeginChild("ContentBrowserDirectoryTree", ImVec2(Internal::DirectoryTreeWidth, 0.f), true);
        DrawDirectoryTree();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("ContentBrowserEntries", ImVec2(0.f, 0.f), false);
        DrawBrowserToolbar();
        ImGui::Separator();
        DrawEntryGrid();
        DrawLoadSceneConfirmation();
        DrawLoadModelConfirmation();
        ImGui::EndChild();

        ImGui::End();
    }

    bool ContentBrowserPanel::CanNavigateToParent() const
    {
        return Platform::FileSystem::IsSubPath(m_Context.assetsRootPath.string(), m_Context.currentContentPath.string()) &&
               !Platform::FileSystem::IsSamePath(m_Context.currentContentPath.string(), m_Context.assetsRootPath.string());
    }

    void ContentBrowserPanel::NavigateToParent()
    {
        if (!CanNavigateToParent())
        {
            return;
        }

        const std::filesystem::path parent = m_Context.currentContentPath.parent_path().lexically_normal();
        m_Context.currentContentPath = Internal::ClampToAssetsRoot(parent, m_Context.assetsRootPath);
    }

    void ContentBrowserPanel::DrawDirectoryTree()
    {
        DrawDirectoryNode(m_Context.assetsRootPath, 0);
    }

    void ContentBrowserPanel::DrawDirectoryNode(const std::filesystem::path &directory, int depth)
    {
        const bool selected = Platform::FileSystem::IsSamePath(directory.string(), m_Context.currentContentPath.string());
        const std::vector<Internal::BrowserEntry> entries = Internal::CollectEntries(directory);
        const bool hasChildDirectories = std::any_of(entries.begin(), entries.end(), [](const Internal::BrowserEntry &entry)
                                                     { return entry.isDirectory; });
        const bool isRoot = Platform::FileSystem::IsSamePath(directory.string(), m_Context.assetsRootPath.string());
        const std::string label = isRoot ? "Assets" : GetDisplayName(directory);
        const std::string id = directory.lexically_normal().generic_string();

        ImGui::PushID(id.c_str());

        ImGuiStorage *storage = ImGui::GetStateStorage();
        const ImGuiID openId = ImGui::GetID("Open");
        bool open = storage->GetBool(openId, isRoot);

        const float rowHeight = ImGui::GetTextLineHeight() + 5.f;
        const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
        const float arrowWidth = hasChildDirectories ? Internal::TreeArrowWidth : 0.f;
        const float labelOffset = arrowWidth + (hasChildDirectories ? Internal::TreeLabelSpacing : 0.f);
        const float rowWidth = labelOffset + labelSize.x + 8.f;

        const float baseCursorX = ImGui::GetCursorPosX();
        ImGui::SetCursorPosX(baseCursorX + static_cast<float>(depth) * Internal::TreeArrowWidth);

        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("DirectoryTreeRow", ImVec2(rowWidth, rowHeight));

        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        const float mouseLocalX = ImGui::GetMousePos().x - rowMin.x;

        if (clicked)
        {
            m_Context.currentContentPath = directory.lexically_normal();
            if (hasChildDirectories && mouseLocalX <= Internal::TreeArrowWidth)
            {
                open = !open;
                storage->SetBool(openId, open);
            }
        }
        if (doubleClicked && hasChildDirectories && mouseLocalX > Internal::TreeArrowWidth)
        {
            open = !open;
            storage->SetBool(openId, open);
        }

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const ImVec2 rowMax(rowMin.x + rowWidth, rowMin.y + rowHeight);
        if (selected || hovered)
        {
            const ImU32 color = selected
                                    ? ImGui::GetColorU32(ImGuiCol_Header)
                                    : ImGui::GetColorU32(ImGuiCol_HeaderHovered);
            drawList->AddRectFilled(rowMin, rowMax, color, 5.f);
        }

        if (hasChildDirectories)
        {
            Internal::DrawDisclosureTriangle(
                drawList,
                ImVec2(rowMin.x, rowMin.y + (rowHeight - Internal::TreeArrowWidth) * 0.5f),
                Internal::TreeArrowWidth,
                open,
                ImGui::GetColorU32(ImGuiCol_Text));
        }

        drawList->AddText(
            ImVec2(rowMin.x + labelOffset + 4.f, rowMin.y + (rowHeight - labelSize.y) * 0.5f),
            ImGui::GetColorU32(ImGuiCol_Text),
            label.c_str());

        ImGui::PopID();

        if (open && hasChildDirectories)
        {
            for (const Internal::BrowserEntry &entry : entries)
            {
                if (entry.isDirectory)
                {
                    DrawDirectoryNode(entry.path, depth + 1);
                }
            }
        }
    }

    void ContentBrowserPanel::DrawBrowserToolbar()
    {
        ImGui::BeginDisabled(!CanNavigateToParent());
        if (ImGui::Button("<"))
        {
            NavigateToParent();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        DrawRelativePath();
    }

    void ContentBrowserPanel::DrawRelativePath()
    {
        ImGui::TextUnformatted(GetRelativeDisplayPath(m_Context.currentContentPath, m_Context.assetsRootPath).c_str());
    }

    void ContentBrowserPanel::DrawEntryGrid()
    {
        std::vector<Internal::BrowserEntry> entries = Internal::CollectEntries(m_Context.currentContentPath);
        if (Internal::IsInsideModelsDirectory(m_Context.currentContentPath, m_Context.assetsRootPath))
        {
            entries.erase(std::remove_if(entries.begin(), entries.end(), [](const Internal::BrowserEntry &entry)
                                         { return !entry.isDirectory && !Engine::AssetPath::IsModelFile(entry.path); }),
                          entries.end());
        }

        if (entries.empty())
        {
            ImGui::TextUnformatted("This folder is empty.");
            return;
        }

        const float contentWidth = ImGui::GetContentRegionAvail().x;
        const int columns = std::max(1, static_cast<int>(contentWidth / Internal::TileSize));
        int column = 0;

        for (const Internal::BrowserEntry &entry : entries)
        {
            ImGui::PushID(entry.path.lexically_normal().generic_string().c_str());

            const ImVec2 tileMin = ImGui::GetCursorScreenPos();
            const ImVec2 tileSize(Internal::TileSize - 8.f, Internal::TileSize);
            ImGui::InvisibleButton("AssetTile", tileSize);
            const bool hovered = ImGui::IsItemHovered();
            const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
            const bool activated = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

            ImDrawList *drawList = ImGui::GetWindowDrawList();
            const ImVec2 tileMax(tileMin.x + tileSize.x, tileMin.y + tileSize.y);
            if (hovered)
            {
                drawList->AddRectFilled(tileMin, tileMax, IM_COL32(198, 217, 192, 180), 7.f);
            }

            const ImVec2 iconMin(
                tileMin.x + (tileSize.x - Internal::TileIconSize) * 0.5f,
                tileMin.y + Internal::TilePadding);
            const ImVec2 iconMax(iconMin.x + Internal::TileIconSize, iconMin.y + Internal::TileIconSize);
            const RHI::ImGuiTextureHandle icon = m_IconManager.GetFileIcon(entry.path, entry.isDirectory);
            if (icon != 0)
            {
                drawList->AddImage(
                    static_cast<ImTextureID>(icon),
                    iconMin,
                    iconMax,
                    ImVec2(0.f, 0.f),
                    ImVec2(1.f, 1.f),
                    IM_COL32(255, 255, 255, 255));
            }
            else
            {
                drawList->AddRectFilled(iconMin, iconMax, Internal::IconColor(entry.kind), 8.f);
            }

            const std::string name = Internal::TileName(entry);
            const ImVec2 textSize = ImGui::CalcTextSize(name.c_str(), nullptr, true, tileSize.x - 8.f);
            drawList->AddText(
                ImGui::GetFont(),
                ImGui::GetFontSize(),
                ImVec2(tileMin.x + (tileSize.x - textSize.x) * 0.5f, iconMax.y + 7.f),
                ImGui::GetColorU32(ImGuiCol_Text),
                name.c_str(),
                nullptr,
                tileSize.x - 8.f,
                nullptr);

            if (activated && entry.isDirectory)
            {
                m_Context.currentContentPath = entry.path.lexically_normal();
            }
            else if (clicked && entry.kind == Engine::AssetPath::AssetKind::Scene)
            {
                RequestSceneLoad(entry.path);
            }
            else if (clicked && entry.kind == Engine::AssetPath::AssetKind::Mesh)
            {
                RequestModelLoad(entry.path);
            }

            ++column;
            if (column < columns)
            {
                ImGui::SameLine();
            }
            else
            {
                column = 0;
            }

            ImGui::PopID();
        }
    }

    void ContentBrowserPanel::RequestSceneLoad(const std::filesystem::path &path)
    {
        m_PendingSceneLoadPath = path.lexically_normal();
        m_OpenLoadScenePopup = true;
    }

    void ContentBrowserPanel::RequestModelLoad(const std::filesystem::path &path)
    {
        m_PendingModelLoadPath = path.lexically_normal();
        m_OpenLoadModelPopup = true;
    }

    void ContentBrowserPanel::DrawLoadSceneConfirmation()
    {
        if (m_OpenLoadScenePopup)
        {
            ImGui::OpenPopup("Load Scene?");
            m_OpenLoadScenePopup = false;
        }

        if (ImGui::BeginPopupModal("Load Scene?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("Load this scene and replace the current editor scene?");
            ImGui::Spacing();
            ImGui::TextUnformatted(GetRelativeDisplayPath(m_PendingSceneLoadPath, m_Context.assetsRootPath).c_str());
            ImGui::Separator();

            if (ImGui::Button("Load", ImVec2(96.f, 0.f)))
            {
                if (m_Context.activeScene != nullptr)
                {
                    if (Engine::SceneSerializer::Deserialize(*m_Context.activeScene, m_PendingSceneLoadPath))
                    {
                        m_Context.currentScenePath = m_PendingSceneLoadPath;
                        m_Context.selectedEntity = m_Context.activeScene->GetSceneCameraEntityId();
                        PHYSARA_INFO("Loaded scene: {}", m_PendingSceneLoadPath.string());
                    }
                    else
                    {
                        PHYSARA_ERROR("Failed to load scene: {}", m_PendingSceneLoadPath.string());
                    }
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(96.f, 0.f)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void ContentBrowserPanel::DrawLoadModelConfirmation()
    {
        if (m_OpenLoadModelPopup)
        {
            ImGui::OpenPopup("Import GLTF?");
            m_OpenLoadModelPopup = false;
        }

        if (ImGui::BeginPopupModal("Import GLTF?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("Import this GLTF into the current scene?");
            ImGui::Spacing();
            ImGui::TextUnformatted(GetRelativeDisplayPath(m_PendingModelLoadPath, m_Context.assetsRootPath).c_str());
            ImGui::Separator();

            if (ImGui::Button("Import", ImVec2(96.f, 0.f)))
            {
                if (m_Context.activeScene != nullptr)
                {
                    Engine::Entity root = Engine::GLTFLoader::LoadToScene(*m_Context.activeScene,
                                                                           m_PendingModelLoadPath,
                                                                           &m_AssetManager);
                    if (root)
                    {
                        m_Context.selectedEntity = root.GetHandle();
                        PHYSARA_INFO("Imported GLTF: {}", m_PendingModelLoadPath.string());
                    }
                    else
                    {
                        PHYSARA_ERROR("Failed to import GLTF: {}", m_PendingModelLoadPath.string());
                    }
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(96.f, 0.f)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    std::string ContentBrowserPanel::GetDisplayName(const std::filesystem::path &path)
    {
        if (path.empty())
        {
            return {};
        }

        const std::string filename = path.filename().string();
        return filename.empty() ? path.string() : filename;
    }

    std::string ContentBrowserPanel::GetRelativeDisplayPath(const std::filesystem::path &path,
                                                            const std::filesystem::path &root)
    {
        std::error_code error{};
        const std::filesystem::path relativePath = std::filesystem::relative(path, root, error);
        if (error || relativePath.empty() || relativePath == ".")
        {
            return "Assets";
        }

        return (std::filesystem::path{"Assets"} / relativePath).generic_string();
    }
}