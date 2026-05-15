#include "SceneViewPanel.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <utility>

#include <imgui/imgui.h>

namespace Physara::Editor
{
    namespace SceneViewPanelDetail
    {
        constexpr const char *PanelName = "Scene View";
        constexpr const char *PresentationPanelName = "Scene View##Presentation";
        constexpr const char *ViewportChildName = "SceneViewViewport";
        constexpr float MinViewportSize = 1.f;
        constexpr float OverlayPadding = 12.f;
        constexpr float IconSize = 18.f;
        constexpr float IconButtonSize = 26.f;

        void AddOverlayText(ImDrawList *drawList, const ImVec2 &pos, const char *text)
        {
            drawList->AddText(pos, IM_COL32(232, 244, 230, 255), text);
        }

        void ShowTooltip(const char *text)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(9.f, 6.f));
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 6.f);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(8, 14, 15, 245));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(112, 184, 207, 190));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(238, 250, 255, 255));
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(text);
            ImGui::EndTooltip();
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
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
        const char *windowName = SceneViewPanelDetail::PanelName;

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
            windowName = SceneViewPanelDetail::PresentationPanelName;
        }

        ImGui::Begin(windowName, nullptr, windowFlags);

        DrawViewport();

        ImGui::End();
    }

    void SceneViewPanel::SetPreviewTexture(RHI::ImGuiTextureHandle texture)
    {
        m_PreviewTexture = texture;
    }

    void SceneViewPanel::SetIconSet(const SceneViewIconSet &icons)
    {
        m_Icons = icons;
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
        ImGui::BeginChild(SceneViewPanelDetail::ViewportChildName, ImVec2(0.f, 0.f), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float width = std::max(avail.x, 0.f);
        const float height = std::max(avail.y, 0.f);

        UpdateSceneViewState(width, height);

        if (m_PreviewTexture != 0 && width >= SceneViewPanelDetail::MinViewportSize && height >= SceneViewPanelDetail::MinViewportSize)
        {
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const ImTextureID textureId = static_cast<ImTextureID>(m_PreviewTexture);
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
        if (m_Context.ui.cleanSceneView || !m_Context.ui.showSceneViewInfo)
        {
            return;
        }

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const bool presentation = m_Context.ui.displayMode == EditorDisplayMode::ViewportPresentation;
        constexpr float paddingX = 14.f;
        constexpr float paddingY = 13.f;
        const float lineHeight = ImGui::GetTextLineHeight() + 5.f;

        char sizeLine[128]{};
        char fpsLine[128]{};
        char cameraLine[128]{};
        char hoveredLine[128]{};
        char focusedLine[128]{};
        char keysLine[160]{};

        std::snprintf(sizeLine, sizeof(sizeLine), "Size: %.f x %.f", width, height);
        std::snprintf(fpsLine, sizeof(fpsLine), "FPS: %.1f", ImGui::GetIO().Framerate);
        const char *cameraMode = "Orbit";
        if (m_Context.sceneView.playFlyMode)
        {
            cameraMode = "Play Fly";
        }
        else if (m_Context.sceneView.flyCameraMode)
        {
            cameraMode = "RMB Navigate";
        }
        std::snprintf(cameraLine, sizeof(cameraLine), "Camera: %s", cameraMode);
        std::snprintf(hoveredLine, sizeof(hoveredLine), "Hovered: %s", m_Context.sceneView.hovered ? "true" : "false");
        std::snprintf(focusedLine, sizeof(focusedLine), "Focused: %s", m_Context.sceneView.focused ? "true" : "false");

        float maxTextWidth = std::max({ImGui::CalcTextSize(sizeLine).x,
                                       ImGui::CalcTextSize(fpsLine).x,
                                       ImGui::CalcTextSize(cameraLine).x,
                                       ImGui::CalcTextSize(hoveredLine).x,
                                       ImGui::CalcTextSize(focusedLine).x});

        if (presentation)
        {
            const ShortcutAction *toggle = m_ShortcutRegistry.FindAction("viewport.presentation.toggle");
            const ShortcutAction *exit = m_ShortcutRegistry.FindAction("viewport.presentation.exit");
            const ShortcutAction *capture = m_ShortcutRegistry.FindAction("capture.current_view");
            const ShortcutAction *help = m_ShortcutRegistry.FindAction("help.shortcuts");

            std::snprintf(keysLine, sizeof(keysLine), "Keys: %s help, %s toggle, %s exit, %s capture",
                          help != nullptr ? help->keyChord.c_str() : "F1",
                          toggle != nullptr ? toggle->keyChord.c_str() : "F11",
                          exit != nullptr ? exit->keyChord.c_str() : "Esc",
                          capture != nullptr ? capture->keyChord.c_str() : "F12");
            maxTextWidth = std::max(maxTextWidth, ImGui::CalcTextSize(keysLine).x);
        }

        const float overlayWidth = std::min(maxTextWidth + paddingX * 2.f, std::max(width - 24.f, 0.f));
        const float overlayHeight = paddingY * 2.f + lineHeight * (presentation ? 6.f : 5.f);
        if (overlayWidth > 80.f && height > overlayHeight + 24.f)
        {
            const ImVec2 overlayMin(origin.x + width - SceneViewPanelDetail::OverlayPadding - overlayWidth,
                                    origin.y + (presentation ? 50.f : 50.f));
            const ImVec2 overlayMax(overlayMin.x + overlayWidth, overlayMin.y + overlayHeight);
            drawList->AddRectFilled(overlayMin, overlayMax, IM_COL32(14, 20, 18, 192), 8.f);
            drawList->AddRect(overlayMin, overlayMax, IM_COL32(143, 164, 151, 120), 8.f);

            float y = overlayMin.y + paddingY;
            const float x = overlayMin.x + paddingX;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), sizeLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), fpsLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), cameraLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), hoveredLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), focusedLine);

            if (presentation)
            {
                y += lineHeight;
                SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), keysLine);
            }
        }
    }

    void SceneViewPanel::DrawViewportToolbar(const ImVec2 &origin, float width)
    {
        if (m_Context.ui.cleanSceneView || width < 180.f)
        {
            return;
        }

        DrawLeftToolbar(origin);
        DrawRightToolbar(origin, width);
        DrawPanelMenu(origin, width);
    }

    void SceneViewPanel::DrawLeftToolbar(const ImVec2 &origin)
    {
        ImGui::SetNextWindowPos(ImVec2(origin.x + SceneViewPanelDetail::OverlayPadding, origin.y + SceneViewPanelDetail::OverlayPadding));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 4.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 3.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 4.f));
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
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav;

        if (ImGui::Begin("SceneViewTransformOverlay", nullptr, flags))
        {
            auto drawOperationButton = [this](const char *id, RHI::ImGuiTextureHandle icon, const char *fallback,
                                              const char *tooltip, GizmoOperation operation)
            {
                const bool active = m_Context.settings.gizmoOperation == operation;
                if (DrawIconButton(id, icon, fallback, tooltip, active))
                {
                    m_Context.settings.gizmoOperation = active ? GizmoOperation::None : operation;
                }
            };

            drawOperationButton("##TranslateTool", m_Icons.translate, "T", "Translate", GizmoOperation::Translate);
            ImGui::SameLine();
            drawOperationButton("##RotateTool", m_Icons.rotate, "R", "Rotate", GizmoOperation::Rotate);
            ImGui::SameLine();
            drawOperationButton("##ScaleTool", m_Icons.scale, "S", "Scale", GizmoOperation::Scale);

            ImGui::SameLine(0.f, 8.f);
            const bool localSpace = m_Context.settings.gizmoSpace == GizmoSpace::Local;
            if (DrawIconButton("##SpaceToggle", 0, localSpace ? "L" : "W",
                               localSpace ? "Local Space. Click to switch to World." : "World Space. Click to switch to Local.",
                               true))
            {
                m_Context.settings.gizmoSpace = localSpace ? GizmoSpace::World : GizmoSpace::Local;
            }
        }
        ImGui::End();

        ImGui::PopStyleColor(7);
        ImGui::PopStyleVar(3);
    }

    void SceneViewPanel::DrawRightToolbar(const ImVec2 &origin, float width)
    {
        ImGui::SetNextWindowPos(ImVec2(origin.x + width - SceneViewPanelDetail::OverlayPadding, origin.y + SceneViewPanelDetail::OverlayPadding),
                                ImGuiCond_Always, ImVec2(1.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 4.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 3.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 4.f));
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
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav;

        if (ImGui::Begin("SceneViewUtilityOverlay", nullptr, flags))
        {
            if (DrawIconButton("##PanelToggle", m_Icons.panel, "P", "Panels", m_Context.ui.showSceneViewPanelMenu))
            {
                m_Context.ui.showSceneViewPanelMenu = !m_Context.ui.showSceneViewPanelMenu;
            }
            ImGui::SameLine();
            if (DrawIconButton("##ShortcutToggle", m_Icons.shortcut, "F1", "Help / Shortcuts", m_Context.ui.showHelpShortcuts))
            {
                m_Context.ui.showHelpShortcuts = !m_Context.ui.showHelpShortcuts;
            }
            ImGui::SameLine();
            if (DrawIconButton("##InfoToggle", m_Icons.info, "i", "Viewport Info", m_Context.ui.showSceneViewInfo))
            {
                m_Context.ui.showSceneViewInfo = !m_Context.ui.showSceneViewInfo;
            }
        }
        ImGui::End();

        ImGui::PopStyleColor(7);
        ImGui::PopStyleVar(3);
    }

    void SceneViewPanel::DrawPanelMenu(const ImVec2 &origin, float width)
    {
        if (!m_Context.ui.showSceneViewPanelMenu)
        {
            return;
        }

        const float panelMenuY = m_Context.ui.showSceneViewInfo ? 194.f : 48.f;
        ImGui::SetNextWindowPos(ImVec2(origin.x + width - SceneViewPanelDetail::OverlayPadding, origin.y + panelMenuY),
                                ImGuiCond_Always, ImVec2(1.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.f, 8.f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 5.f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(14, 20, 18, 230));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(143, 164, 151, 130));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(232, 244, 230, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(45, 66, 56, 230));
        ImGui::PushStyleColor(ImGuiCol_CheckMark, IM_COL32(199, 232, 203, 255));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_NoDocking |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav;

        if (ImGui::Begin("SceneViewPanelMenuOverlay", nullptr, flags))
        {
            DrawCompactCheckbox("Hierarchy", m_Context.ui.panels.hierarchy);
            DrawCompactCheckbox("Renderer Settings", m_Context.ui.panels.rendererSettings);
            DrawCompactCheckbox("Inspector", m_Context.ui.panels.inspector);
            DrawCompactCheckbox("Content Browser", m_Context.ui.panels.contentBrowser);
            DrawCompactCheckbox("Log", m_Context.ui.panels.log);
        }
        ImGui::End();

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
    }

    void SceneViewPanel::DrawCompactCheckbox(const char *label, bool &value)
    {
        const float boxSize = ImGui::GetTextLineHeight();
        const float rowHeight = boxSize + 4.f;
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        const ImVec2 rowSize(boxSize + 8.f + textSize.x, rowHeight);

        ImGui::InvisibleButton(label, rowSize);
        if (ImGui::IsItemClicked())
        {
            value = !value;
        }

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const ImVec2 boxMin(rowMin.x, rowMin.y + (rowHeight - boxSize) * 0.5f);
        const ImVec2 boxMax(boxMin.x + boxSize, boxMin.y + boxSize);
        const ImU32 fillColor = value ? IM_COL32(82, 126, 104, 245) : IM_COL32(29, 44, 39, 230);
        const ImU32 borderColor = ImGui::IsItemHovered() ? IM_COL32(139, 205, 224, 230) : IM_COL32(143, 164, 151, 150);
        drawList->AddRectFilled(boxMin, boxMax, fillColor, 4.f);
        drawList->AddRect(boxMin, boxMax, borderColor, 4.f);

        if (value)
        {
            const float x0 = boxMin.x + boxSize * 0.25f;
            const float y0 = boxMin.y + boxSize * 0.53f;
            const float x1 = boxMin.x + boxSize * 0.43f;
            const float y1 = boxMin.y + boxSize * 0.70f;
            const float x2 = boxMin.x + boxSize * 0.76f;
            const float y2 = boxMin.y + boxSize * 0.30f;
            drawList->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(238, 250, 242, 255), 2.f);
            drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(238, 250, 242, 255), 2.f);
        }

        drawList->AddText(ImVec2(boxMax.x + 8.f, rowMin.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f),
                          IM_COL32(232, 244, 230, 255), label);
    }

    bool SceneViewPanel::DrawIconButton(const char *id, RHI::ImGuiTextureHandle icon, const char *fallback,
                                        const char *tooltip, bool active)
    {
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(91, 133, 149, 245));
        }

        bool pressed = false;
        const ImVec2 buttonSize(SceneViewPanelDetail::IconButtonSize, SceneViewPanelDetail::IconButtonSize);
        if (icon != 0)
        {
            pressed = ImGui::ImageButton(id, static_cast<ImTextureID>(icon),
                                         ImVec2(SceneViewPanelDetail::IconSize, SceneViewPanelDetail::IconSize),
                                         ImVec2(0.f, 0.f), ImVec2(1.f, 1.f),
                                         ImVec4(0.f, 0.f, 0.f, 0.f), ImVec4(0.68f, 0.86f, 0.96f, 1.f));
        }
        else
        {
            pressed = ImGui::Button(fallback, buttonSize);
        }

        if (active)
        {
            ImGui::PopStyleColor();
        }

        if (ImGui::IsItemHovered())
        {
            SceneViewPanelDetail::ShowTooltip(tooltip);
        }

        return pressed;
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
            width >= SceneViewPanelDetail::MinViewportSize && height >= SceneViewPanelDetail::MinViewportSize)
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

        const ImGuiIO &io = ImGui::GetIO();

        EditorCameraInputFrame snapshot{};
        snapshot.mouseDeltaX = io.MouseDelta.x;
        snapshot.mouseDeltaY = io.MouseDelta.y;
        snapshot.hovered = m_Context.sceneView.hovered;
        snapshot.focused = m_Context.sceneView.focused;
        snapshot.rightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        snapshot.gravePressed = ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false);
        snapshot.escapePressed = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
        snapshot.moveForward = ImGui::IsKeyDown(ImGuiKey_W);
        snapshot.moveBackward = ImGui::IsKeyDown(ImGuiKey_S);
        snapshot.moveLeft = ImGui::IsKeyDown(ImGuiKey_A);
        snapshot.moveRight = ImGui::IsKeyDown(ImGuiKey_D);
        snapshot.moveUp = ImGui::IsKeyDown(ImGuiKey_E);
        snapshot.moveDown = ImGui::IsKeyDown(ImGuiKey_Q);
        snapshot.speedBoost = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
        snapshot.mouseWheel = io.MouseWheel;

        m_InputCallback(snapshot);
    }
}