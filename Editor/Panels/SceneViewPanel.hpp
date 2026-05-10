#pragma once

#include <cstdint>
#include <functional>

#include <imgui/imgui.h>

#include <Editor/Core/EditorContext.hpp>
#include <Editor/Core/ShortcutRegistry.hpp>

namespace Physara::Editor
{
    struct SceneViewInputSnapshot
    {
        float mouseX{0.f};
        float mouseY{0.f};
        bool hovered{false};
        bool focused{false};
        bool leftMouseClicked{false};
        bool rightMouseDown{false};
        float mouseWheel{0.f};
    };

    class SceneViewPanel final
    {
    public:
        using ViewportResizeCallback = std::function<void(std::uint32_t width, std::uint32_t height)>;
        using InputForwardCallback = std::function<void(const SceneViewInputSnapshot &input)>;

        SceneViewPanel(EditorContext &context, const ShortcutRegistry &shortcutRegistry);

        void Draw();
        void SetPreviewTextureId(void *textureId);
        void SetViewportResizeCallback(ViewportResizeCallback callback);
        void SetInputForwardCallback(InputForwardCallback callback);

    private:
        void DrawViewport();
        void DrawPlaceholder(float width, float height);
        void DrawOverlay(const ImVec2 &origin, float width, float height);
        void DrawViewportToolbar(const ImVec2 &origin, float width);
        void UpdateSceneViewState(float width, float height);
        void ForwardInput();

    private:
        EditorContext &m_Context;
        const ShortcutRegistry &m_ShortcutRegistry;
        void *m_PreviewTextureId{nullptr};
        ViewportResizeCallback m_ResizeCallback{};
        InputForwardCallback m_InputCallback{};
    };
}