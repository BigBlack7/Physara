#include "SceneViewPanel.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <utility>

#include <imgui/imgui.h>

namespace Physara::Editor
{
    namespace Internal
    {
        constexpr const char *PanelName = "Scene View";
        constexpr const char *ViewportChildName = "SceneViewViewport";
        constexpr float MinViewportSize = 1.f;

        void AddOverlayText(ImDrawList *drawList, const ImVec2 &pos, const char *text)
        {
            drawList->AddText(pos, IM_COL32(232, 244, 230, 255), text);
        }
    }

    SceneViewPanel::SceneViewPanel(EditorContext &context) : m_Context(context) {}

    void SceneViewPanel::Draw()
    {
        ImGui::Begin(Internal::PanelName);

        DrawToolbar();
        ImGui::Separator();
        DrawViewport();

        ImGui::End();
    }

    void SceneViewPanel::SetPreviewTextureId(void *textureId)
    {
        m_PreviewTextureId = textureId;
    }

    void SceneViewPanel::SetViewportResizeCallback(ViewportResizeCallback callback)
    {
        m_ResizeCallback = std::move(callback);
    }

    void SceneViewPanel::SetInputForwardCallback(InputForwardCallback callback)
    {
        m_InputCallback = std::move(callback);
    }

    void SceneViewPanel::DrawToolbar()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 1.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 4.f));

        ImGui::TextUnformatted("Gizmo:");
        ImGui::SameLine();

        int opIndex = static_cast<int>(m_Context.settings.gizmoOperation);
        ImGui::RadioButton("Translate", &opIndex, static_cast<int>(GizmoOperation::Translate));
        ImGui::SameLine();
        ImGui::RadioButton("Rotate", &opIndex, static_cast<int>(GizmoOperation::Rotate));
        ImGui::SameLine();
        ImGui::RadioButton("Scale", &opIndex, static_cast<int>(GizmoOperation::Scale));
        m_Context.settings.gizmoOperation = static_cast<GizmoOperation>(opIndex);

        ImGui::SameLine(0.f, 16.f);
        ImGui::TextUnformatted("Space:");
        ImGui::SameLine();

        int spaceIndex = static_cast<int>(m_Context.settings.gizmoSpace);
        ImGui::RadioButton("Local", &spaceIndex, static_cast<int>(GizmoSpace::Local));
        ImGui::SameLine();
        ImGui::RadioButton("World", &spaceIndex, static_cast<int>(GizmoSpace::World));
        m_Context.settings.gizmoSpace = static_cast<GizmoSpace>(spaceIndex);

        ImGui::PopStyleVar(2);
    }

    void SceneViewPanel::DrawViewport()
    {
        ImGui::BeginChild(Internal::ViewportChildName, ImVec2(0.f, 0.f), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float width = std::max(avail.x, 0.f);
        const float height = std::max(avail.y, 0.f);

        UpdateSceneViewState(width, height);

        if (m_PreviewTextureId != nullptr && width >= Internal::MinViewportSize && height >= Internal::MinViewportSize)
        {
            const ImTextureID textureId =
                static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(m_PreviewTextureId));
            ImGui::Image(textureId, ImVec2(width, height));
        }
        else
        {
            DrawPlaceholder(width, height);
        }

        ForwardInput();

        ImGui::EndChild();
    }

    void SceneViewPanel::DrawPlaceholder(float width, float height)
    {
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 size(width, height);
        const ImVec2 max(origin.x + size.x, origin.y + size.y);

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(origin, max, IM_COL32(34, 40, 39, 255), 8.f);
        drawList->AddRect(origin, max, IM_COL32(108, 126, 116, 255), 8.f);

        const float overlayWidth = std::min(390.f, std::max(width - 24.f, 0.f));
        const float overlayHeight = 134.f;
        if (overlayWidth > 120.f && height > overlayHeight + 24.f)
        {
            const ImVec2 overlayMin(origin.x + 12.f, origin.y + 12.f);
            const ImVec2 overlayMax(overlayMin.x + overlayWidth, overlayMin.y + overlayHeight);
            drawList->AddRectFilled(overlayMin, overlayMax, IM_COL32(14, 20, 18, 192), 8.f);
            drawList->AddRect(overlayMin, overlayMax, IM_COL32(143, 164, 151, 120), 8.f);

            char line[128]{};
            float y = overlayMin.y + 13.f;
            const float x = overlayMin.x + 14.f;
            const float lineHeight = ImGui::GetTextLineHeight() + 5.f;

            Internal::AddOverlayText(drawList, ImVec2(x, y), "Scene View");
            y += lineHeight;

            std::snprintf(line, sizeof(line), "Size: %.f x %.f", width, height);
            Internal::AddOverlayText(drawList, ImVec2(x, y), line);
            y += lineHeight;

            std::snprintf(line, sizeof(line), "FPS: %.1f", ImGui::GetIO().Framerate);
            Internal::AddOverlayText(drawList, ImVec2(x, y), line);
            y += lineHeight;

            std::snprintf(line, sizeof(line), "Hovered: %s", m_Context.sceneView.hovered ? "true" : "false");
            Internal::AddOverlayText(drawList, ImVec2(x, y), line);
            y += lineHeight;

            std::snprintf(line, sizeof(line), "Focused: %s", m_Context.sceneView.focused ? "true" : "false");
            Internal::AddOverlayText(drawList, ImVec2(x, y), line);
        }

        ImGui::Dummy(size);
    }

    void SceneViewPanel::UpdateSceneViewState(float width, float height)
    {
        m_Context.sceneView.sizeChanged =
            (width != m_Context.sceneView.width) ||
            (height != m_Context.sceneView.height);

        m_Context.sceneView.width = width;
        m_Context.sceneView.height = height;
        m_Context.sceneView.hovered = ImGui::IsWindowHovered();
        m_Context.sceneView.focused = ImGui::IsWindowFocused();

        if (m_Context.sceneView.sizeChanged && m_ResizeCallback &&
            width >= Internal::MinViewportSize && height >= Internal::MinViewportSize)
        {
            m_ResizeCallback(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
        }
    }

    void SceneViewPanel::ForwardInput()
    {
        if (!m_InputCallback)
        {
            return;
        }

        const ImVec2 mousePos = ImGui::GetMousePos();
        const ImVec2 viewportMin = ImGui::GetWindowPos();

        SceneViewInputSnapshot snapshot{};
        snapshot.mouseX = mousePos.x - viewportMin.x;
        snapshot.mouseY = mousePos.y - viewportMin.y;
        snapshot.hovered = m_Context.sceneView.hovered;
        snapshot.focused = m_Context.sceneView.focused;
        snapshot.leftMouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        snapshot.rightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        snapshot.mouseWheel = ImGui::GetIO().MouseWheel;

        m_InputCallback(snapshot);
    }
}