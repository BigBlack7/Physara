#include "IconManager.hpp"

#include <cstdint>
#include <memory>

#include <Engine/Core/Log.hpp>
#include <Engine/Resource/AssetPath.hpp>
#include <Engine/Resource/Loaders/TextureLoader.hpp>

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

        if (Engine::AssetPath::IsSceneFile(path))
        {
            return GetIcon(EditorIcon::Scene);
        }

        if (Engine::AssetPath::IsModelFile(path))
        {
            return GetIcon(EditorIcon::Mesh);
        }
        if (Engine::AssetPath::IsTextureFile(path))
        {
            return GetIcon(EditorIcon::Texture);
        }

        return GetIcon(EditorIcon::File);
    }

    void IconManager::LoadBuiltinIcon(EditorIcon icon, std::string_view fileName)
    {
        const std::filesystem::path path = m_IconsRoot / fileName;
        const std::shared_ptr<Engine::Texture> textureData = Engine::TextureLoader::LoadRGBA8(path);
        if (!textureData || !textureData->IsLoaded())
        {
            PHYSARA_WARN("Failed to load editor icon: {}", path.string());
            return;
        }

        const RHI::ImGuiTextureHandle texture =
            m_Backend->CreateTextureRGBA(textureData->width,
                                         textureData->height,
                                         textureData->rgba8Pixels.data());

        if (texture == 0)
        {
            PHYSARA_WARN("Failed to upload editor icon: {}", path.string());
            return;
        }

        m_Icons[icon] = texture;
    }
}