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
        constexpr const char *PresentationPanelName = "Scene View##Presentation";
        constexpr const char *ViewportChildName = "SceneViewViewport";
        constexpr float MinViewportSize = 1.f;

        void AddOverlayText(ImDrawList *drawList, const ImVec2 &pos, const char *text)
        {
            drawList->AddText(pos, IM_COL32(232, 244, 230, 255), text);
        }
    }

    SceneViewPanel::SceneViewPanel(EditorContext &context, const ShortcutRegistry &shortcutRegistry)
        : m_Context(context), m_ShortcutRegistry(shortcutRegistry)
    {
    }

    void SceneViewPanel::Draw()
    {
        const bool presentation = m_Context.ui.displayMode == EditorDisplayMode::ViewportPresentation;
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
        const char *windowName = Internal::PanelName;

        if (presentation)
        {
            const ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            windowFlags = ImGuiWindowFlags_NoTitleBar |
                          ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_NoDocking |
                          ImGuiWindowFlags_NoBringToFrontOnFocus;
            windowName = Internal::PresentationPanelName;
        }

        ImGui::Begin(windowName, nullptr, windowFlags);

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
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const ImTextureID textureId =
                static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(m_PreviewTextureId));
            ImGui::Image(textureId, ImVec2(width, height));
            DrawViewportToolbar(origin, width);
            DrawOverlay(origin, width, height);
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

        DrawViewportToolbar(origin, width);
        DrawOverlay(origin, width, height);

        ImGui::Dummy(size);
    }

    void SceneViewPanel::DrawOverlay(const ImVec2 &origin, float width, float height)
    {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const bool presentation = m_Context.ui.displayMode == EditorDisplayMode::ViewportPresentation;
        const float overlayWidth = std::min(presentation ? 520.f : 390.f, std::max(width - 24.f, 0.f));
        const float overlayHeight = presentation ? 150.f : 134.f;
        if (overlayWidth > 120.f && height > overlayHeight + 24.f)
        {
            const ImVec2 overlayMin(origin.x + 12.f, origin.y + (presentation ? 12.f : 54.f));
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

            if (presentation)
            {
                y += lineHeight;
                const ShortcutAction *toggle = m_ShortcutRegistry.FindAction("viewport.presentation.toggle");
                const ShortcutAction *exit = m_ShortcutRegistry.FindAction("viewport.presentation.exit");
                const ShortcutAction *capture = m_ShortcutRegistry.FindAction("capture.current_view");

                const ShortcutAction *help = m_ShortcutRegistry.FindAction("help.shortcuts");

                std::snprintf(line, sizeof(line), "Keys: %s help, %s toggle, %s exit, %s capture",
                              help != nullptr ? help->keyChord.c_str() : "F1",
                              toggle != nullptr ? toggle->keyChord.c_str() : "F11",
                              exit != nullptr ? exit->keyChord.c_str() : "Esc",
                              capture != nullptr ? capture->keyChord.c_str() : "F12");
                Internal::AddOverlayText(drawList, ImVec2(x, y), line);
            }
        }
    }

    void SceneViewPanel::DrawViewportToolbar(const ImVec2 &origin, float width)
    {
        if (m_Context.ui.displayMode == EditorDisplayMode::ViewportPresentation || width < 500.f)
        {
            return;
        }

        constexpr float toolbarWidth = 660.f;
        constexpr float toolbarHeight = 30.f;
        const ImVec2 toolbarPos(origin.x + 12.f, origin.y + 12.f);
        const ImVec2 toolbarSize(std::min(toolbarWidth, std::max(width - 24.f, 0.f)), toolbarHeight);

        ImGui::SetNextWindowPos(toolbarPos);
        ImGui::SetNextWindowSize(toolbarSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 4.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.f, 3.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7.f, 2.f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(14, 20, 18, 205));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(143, 164, 151, 130));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(232, 244, 230, 255));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, IM_COL32(169, 187, 174, 255));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(45, 66, 56, 230));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(72, 102, 84, 240));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(95, 134, 110, 255));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_NoDocking |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav;

        if (ImGui::Begin("SceneViewToolbarOverlay", nullptr, flags))
        {
            ImGui::TextUnformatted("Gizmo");
            ImGui::SameLine();

            auto drawOperationButton = [this](const char *label, GizmoOperation operation)
            {
                if (m_Context.settings.gizmoOperation == operation)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(101, 145, 117, 255));
                }
                if (ImGui::SmallButton(label))
                {
                    m_Context.settings.gizmoOperation = operation;
                }
                if (m_Context.settings.gizmoOperation == operation)
                {
                    ImGui::PopStyleColor();
                }
            };

            drawOperationButton("Move", GizmoOperation::Translate);
            ImGui::SameLine();
            drawOperationButton("Rotate", GizmoOperation::Rotate);
            ImGui::SameLine();
            drawOperationButton("Scale", GizmoOperation::Scale);

            ImGui::SameLine(0.f, 14.f);
            ImGui::TextUnformatted("Space");
            ImGui::SameLine();

            auto drawSpaceButton = [this](const char *label, GizmoSpace space)
            {
                if (m_Context.settings.gizmoSpace == space)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(101, 145, 117, 255));
                }
                if (ImGui::SmallButton(label))
                {
                    m_Context.settings.gizmoSpace = space;
                }
                if (m_Context.settings.gizmoSpace == space)
                {
                    ImGui::PopStyleColor();
                }
            };

            drawSpaceButton("Local", GizmoSpace::Local);
            ImGui::SameLine();
            drawSpaceButton("World", GizmoSpace::World);

            ImGui::SameLine(0.f, 14.f);
            if (ImGui::SmallButton("Panels"))
            {
                ImGui::OpenPopup("SceneViewPanelsPopup");
            }

            ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(18, 28, 24, 245));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(232, 244, 230, 255));
            ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(58, 85, 70, 255));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(78, 112, 92, 255));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(101, 145, 117, 255));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, IM_COL32(199, 232, 203, 255));
            if (ImGui::BeginPopup("SceneViewPanelsPopup"))
            {
                ImGui::MenuItem("Hierarchy", nullptr, &m_Context.ui.panels.hierarchy);
                ImGui::MenuItem("Renderer Settings", nullptr, &m_Context.ui.panels.rendererSettings);
                ImGui::MenuItem("Inspector", nullptr, &m_Context.ui.panels.inspector);
                ImGui::MenuItem("Content Browser", nullptr, &m_Context.ui.panels.contentBrowser);
                ImGui::MenuItem("Log", nullptr, &m_Context.ui.panels.log);
                ImGui::EndPopup();
            }
            ImGui::PopStyleColor(6);

            ImGui::SameLine();
            if (ImGui::SmallButton("F1"))
            {
                m_Context.ui.showHelpShortcuts = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Help / Shortcuts");
            }
        }
        ImGui::End();

        ImGui::PopStyleColor(7);
        ImGui::PopStyleVar(3);
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