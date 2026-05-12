#pragma once

#include <filesystem>
#include <string_view>
#include <unordered_map>

#include <Engine/RHI/Core/IImGuiBackend.hpp>

namespace Physara::Editor
{
    enum class EditorIcon
    {
        File,
        Folder,
        Scene,
        Mesh,
        Texture,
        Translate,
        Rotate,
        Scale,
        Panel,
        Shortcut,
        Info,
        Physara
    };

    class IconManager final
    {
    public:
        IconManager() = default;
        ~IconManager();

        IconManager(const IconManager &) = delete;
        IconManager &operator=(const IconManager &) = delete;

        void Initialize(RHI::IImGuiBackend *backend, const std::filesystem::path &iconsRoot);
        void Shutdown();

        [[nodiscard]] RHI::ImGuiTextureHandle GetIcon(EditorIcon icon) const;
        [[nodiscard]] RHI::ImGuiTextureHandle GetFileIcon(const std::filesystem::path &path, bool isDirectory) const;

    private:
        void LoadBuiltinIcon(EditorIcon icon, std::string_view fileName);

    private:
        RHI::IImGuiBackend *m_Backend{nullptr};
        std::filesystem::path m_IconsRoot{};
        std::unordered_map<EditorIcon, RHI::ImGuiTextureHandle> m_Icons{};
    };
}