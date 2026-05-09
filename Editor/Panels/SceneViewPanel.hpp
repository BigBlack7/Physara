#pragma once

#include <cstdint>
#include <functional>

#include <Editor/Core/EditorContext.hpp>

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

        explicit SceneViewPanel(EditorContext &context);

        void Draw();
        void SetPreviewTextureId(void *textureId);
        void SetViewportResizeCallback(ViewportResizeCallback callback);
        void SetInputForwardCallback(InputForwardCallback callback);

    private:
        void DrawToolbar();
        void DrawViewport();
        void DrawPlaceholder(float width, float height);
        void UpdateSceneViewState(float width, float height);
        void ForwardInput();

    private:
        EditorContext &m_Context;
        void *m_PreviewTextureId{nullptr};
        ViewportResizeCallback m_ResizeCallback{};
        InputForwardCallback m_InputCallback{};
    };
}