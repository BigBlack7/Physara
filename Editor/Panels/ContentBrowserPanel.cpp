#include "ContentBrowserPanel.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include <imgui/imgui.h>

#include <Engine/Core/Log.hpp>
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

        enum class AssetKind
        {
            Folder,
            Scene,
            Mesh,
            Texture,
            Shader,
            File
        };

        struct BrowserEntry
        {
            std::string name{};
            std::filesystem::path path{};
            AssetKind kind{AssetKind::File};
            bool isDirectory{false};
        };

        std::string ToLower(std::string_view text)
        {
            std::string result{text};
            std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return result;
        }

        bool IsSamePath(const std::filesystem::path &lhs, const std::filesystem::path &rhs)
        {
            return ToLower(lhs.lexically_normal().generic_string()) ==
                   ToLower(rhs.lexically_normal().generic_string());
        }

        std::string NormalizeDirectoryForCompare(const std::filesystem::path &path)
        {
            std::string result = ToLower(path.lexically_normal().generic_string());
            if (!result.empty() && result.back() != '/')
            {
                result.push_back('/');
            }

            return result;
        }

        bool IsPathInsideRoot(const std::filesystem::path &path, const std::filesystem::path &root)
        {
            const std::string pathNorm = NormalizeDirectoryForCompare(path);
            const std::string rootNorm = NormalizeDirectoryForCompare(root);
            return pathNorm.rfind(rootNorm, 0) == 0;
        }

        std::filesystem::path ClampToAssetsRoot(const std::filesystem::path &path,
                                                const std::filesystem::path &root)
        {
            const std::filesystem::path normalizedRoot = root.lexically_normal();
            const std::filesystem::path normalizedPath = path.lexically_normal();
            return IsPathInsideRoot(normalizedPath, normalizedRoot) ? normalizedPath : normalizedRoot;
        }

        bool IsSceneFile(const std::filesystem::path &path)
        {
            return ToLower(path.filename().string()).ends_with(".scene.json");
        }

        AssetKind DetectAssetKind(const std::filesystem::path &path, bool isDirectory)
        {
            if (isDirectory)
            {
                return AssetKind::Folder;
            }

            if (IsSceneFile(path))
            {
                return AssetKind::Scene;
            }

            const std::string extension = ToLower(path.extension().string());
            if (extension == ".gltf" || extension == ".glb")
            {
                return AssetKind::Mesh;
            }
            if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".exr")
            {
                return AssetKind::Texture;
            }
            if (extension == ".glsl" || extension == ".vert" || extension == ".frag" || extension == ".comp")
            {
                return AssetKind::Shader;
            }

            return AssetKind::File;
        }

        ImU32 IconColor(AssetKind kind)
        {
            switch (kind)
            {
            case AssetKind::Folder:
                return IM_COL32(185, 158, 84, 255);
            case AssetKind::Scene:
                return IM_COL32(96, 139, 119, 255);
            case AssetKind::Mesh:
                return IM_COL32(112, 127, 158, 255);
            case AssetKind::Texture:
                return IM_COL32(139, 120, 160, 255);
            case AssetKind::Shader:
                return IM_COL32(119, 140, 86, 255);
            case AssetKind::File:
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
            if (entry.kind == AssetKind::Scene)
            {
                const std::string name = entry.name;
                constexpr std::string_view sceneSuffix = ".scene.json";
                if (name.size() > sceneSuffix.size() &&
                    ToLower(name).ends_with(sceneSuffix))
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
                    DetectAssetKind(path, isDirectory),
                    isDirectory});
            }

            std::sort(entries.begin(), entries.end(), [](const BrowserEntry &lhs, const BrowserEntry &rhs)
                      {
                          if (lhs.isDirectory != rhs.isDirectory)
                          {
                              return lhs.isDirectory;
                          }
                          return ToLower(lhs.name) < ToLower(rhs.name);
                      });

            return entries;
        }
    }

    ContentBrowserPanel::ContentBrowserPanel(EditorContext &context, const IconManager &iconManager)
        : m_Context(context), m_IconManager(iconManager) {}

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
        ImGui::EndChild();

        ImGui::End();
    }

    bool ContentBrowserPanel::CanNavigateToParent() const
    {
        return Internal::IsPathInsideRoot(m_Context.currentContentPath, m_Context.assetsRootPath) &&
               !Internal::IsSamePath(m_Context.currentContentPath, m_Context.assetsRootPath);
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
        const bool selected = Internal::IsSamePath(directory, m_Context.currentContentPath);
        const std::vector<Internal::BrowserEntry> entries = Internal::CollectEntries(directory);
        const bool hasChildDirectories = std::any_of(entries.begin(), entries.end(), [](const Internal::BrowserEntry &entry)
                                                     { return entry.isDirectory; });
        const bool isRoot = Internal::IsSamePath(directory, m_Context.assetsRootPath);
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
        const std::vector<Internal::BrowserEntry> entries = Internal::CollectEntries(m_Context.currentContentPath);

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
            else if (clicked && entry.kind == Internal::AssetKind::Scene)
            {
                RequestSceneLoad(entry.path);
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