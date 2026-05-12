#include "IconManager.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>

#include <stb/stb_image.h>

#include <Engine/Core/Log.hpp>

namespace Physara::Editor
{
    IconManager::~IconManager()
    {
        Shutdown();
    }

    void IconManager::Initialize(RHI::IImGuiBackend *backend, const std::filesystem::path &iconsRoot)
    {
        Shutdown();

        m_Backend = backend;
        m_IconsRoot = iconsRoot;

        if (m_Backend == nullptr)
        {
            PHYSARA_WARN("IconManager initialized without ImGui backend.");
            return;
        }

        LoadBuiltinIcon(EditorIcon::File, "icon_file.png");
        LoadBuiltinIcon(EditorIcon::Folder, "icon_folder.png");
        LoadBuiltinIcon(EditorIcon::Scene, "icon_scene.png");
        LoadBuiltinIcon(EditorIcon::Mesh, "icon_mesh.png");
        LoadBuiltinIcon(EditorIcon::Texture, "icon_texture.png");
        LoadBuiltinIcon(EditorIcon::Translate, "icon_translate.png");
        LoadBuiltinIcon(EditorIcon::Rotate, "icon_rotation.png");
        LoadBuiltinIcon(EditorIcon::Scale, "icon_scale.png");
        LoadBuiltinIcon(EditorIcon::Panel, "icon_panel.png");
        LoadBuiltinIcon(EditorIcon::Shortcut, "icon_shortcut.png");
        LoadBuiltinIcon(EditorIcon::Info, "icon_info.png");
        LoadBuiltinIcon(EditorIcon::Physara, "icon_physara.png");
    }

    void IconManager::Shutdown()
    {
        if (m_Backend != nullptr)
        {
            for (const auto &[icon, texture] : m_Icons)
            {
                (void)icon;
                if (texture != 0)
                {
                    m_Backend->DestroyTexture(texture);
                }
            }
        }

        m_Icons.clear();
        m_IconsRoot.clear();
        m_Backend = nullptr;
    }

    RHI::ImGuiTextureHandle IconManager::GetIcon(EditorIcon icon) const
    {
        const auto it = m_Icons.find(icon);
        if (it != m_Icons.end() && it->second != 0)
        {
            return it->second;
        }

        if (icon != EditorIcon::File)
        {
            return GetIcon(EditorIcon::File);
        }

        return 0;
    }

    RHI::ImGuiTextureHandle IconManager::GetFileIcon(const std::filesystem::path &path, bool isDirectory) const
    {
        if (isDirectory)
        {
            return GetIcon(EditorIcon::Folder);
        }

        if (IsSceneFile(path))
        {
            return GetIcon(EditorIcon::Scene);
        }

        const std::string extension = ToLower(path.extension().string());
        if (extension == ".gltf" || extension == ".glb")
        {
            return GetIcon(EditorIcon::Mesh);
        }
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".exr")
        {
            return GetIcon(EditorIcon::Texture);
        }

        return GetIcon(EditorIcon::File);
    }

    void IconManager::LoadBuiltinIcon(EditorIcon icon, std::string_view fileName)
    {
        const std::filesystem::path path = m_IconsRoot / fileName;

        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char *pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
        if (pixels == nullptr)
        {
            PHYSARA_WARN("Failed to load editor icon: {}", path.string());
            return;
        }

        const RHI::ImGuiTextureHandle texture =
            m_Backend->CreateTextureRGBA(static_cast<std::uint32_t>(width),
                                         static_cast<std::uint32_t>(height),
                                         pixels);
        stbi_image_free(pixels);

        if (texture == 0)
        {
            PHYSARA_WARN("Failed to upload editor icon: {}", path.string());
            return;
        }

        m_Icons[icon] = texture;
    }

    std::string IconManager::ToLower(std::string_view text)
    {
        std::string result{text};
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    bool IconManager::IsSceneFile(const std::filesystem::path &path)
    {
        return ToLower(path.filename().string()).ends_with(".scene.json");
    }
}