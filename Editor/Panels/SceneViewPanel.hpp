#pragma once

#include <cstdint>
#include <functional>

#include <imgui/imgui.h>

#include <Editor/Core/EditorContext.hpp>
#include <Editor/Core/ShortcutRegistry.hpp>
#include <Editor/Camera/EditorCamera.hpp>
#include <Engine/RHI/Core/IImGuiBackend.hpp>

namespace Physara::Editor
{
    struct SceneViewIconSet
    {
        RHI::ImGuiTextureHandle translate{0};
        RHI::ImGuiTextureHandle rotate{0};
        RHI::ImGuiTextureHandle scale{0};
        RHI::ImGuiTextureHandle panel{0};
        RHI::ImGuiTextureHandle shortcut{0};
        RHI::ImGuiTextureHandle info{0};
    };

    class SceneViewPanel final
    {
    public:
        using ViewportResizeCallback = std::function<void(std::uint32_t width, std::uint32_t height)>;
        using InputForwardCallback = std::function<void(const EditorCameraInputFrame &input)>;

        SceneViewPanel(EditorContext &context, const ShortcutRegistry &shortcutRegistry);

        void Draw();
        void SetPreviewTexture(RHI::ImGuiTextureHandle texture);
        void SetIconSet(const SceneViewIconSet &icons);
        void SetViewportResizeCallback(ViewportResizeCallback callback);
        void SetInputForwardCallback(InputForwardCallback callback);

    private:
        void DrawViewport();
        void DrawPlaceholder(float width, float height);
        void DrawOverlay(const ImVec2 &origin, float width, float height);
        void DrawViewportToolbar(const ImVec2 &origin, float width);
        void DrawLeftToolbar(const ImVec2 &origin);
        void DrawRightToolbar(const ImVec2 &origin, float width);
        void DrawPanelMenu(const ImVec2 &origin, float width);
        void DrawCompactCheckbox(const char *label, bool &value);
        bool DrawIconButton(const char *id, RHI::ImGuiTextureHandle icon, const char *fallback,
                            const char *tooltip, bool active);
        void UpdateSceneViewState(float width, float height);
        void ForwardInput();

    private:
        EditorContext &m_Context;
        const ShortcutRegistry &m_ShortcutRegistry;
        RHI::ImGuiTextureHandle m_PreviewTexture{0};
        SceneViewIconSet m_Icons{};
        ViewportResizeCallback m_ResizeCallback{};
        InputForwardCallback m_InputCallback{};
        bool m_NavigationCaptureActive{false};
    };
}