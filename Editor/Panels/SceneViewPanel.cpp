#include "SceneViewPanel.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <string_view>
#include <utility>

#include <imgui/imgui.h>

#include <Engine/Renderer/RenderPath.hpp>

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
        constexpr int ShadowMapDebugView = 4;

        bool IsShadowMapInspector(const EditorContext &context)
        {
            return context.settings.postProcess.debugViewIndex == ShadowMapDebugView;
        }

        void AddOverlayText(ImDrawList *drawList, const ImVec2 &pos, const char *text)
        {
            drawList->AddText(pos, IM_COL32(232, 244, 230, 255), text);
        }

        void DrawShadowMapInspectorLabel(
            ImDrawList *drawList,
            const ImVec2 &origin,
            float height,
            int cascadeIndex)
        {
            char label[96]{};
            std::snprintf(
                label,
                sizeof(label),
                "Shadow Map - Cascade %d - Camera navigation locked",
                cascadeIndex + 1);
            const ImVec2 textSize = ImGui::CalcTextSize(label);
            const ImVec2 boxMin(
                origin.x + OverlayPadding,
                origin.y + height - OverlayPadding - textSize.y - 14.f);
            const ImVec2 boxMax(boxMin.x + textSize.x + 20.f, boxMin.y + textSize.y + 14.f);
            drawList->AddRectFilled(boxMin, boxMax, IM_COL32(14, 20, 18, 210), 7.f);
            drawList->AddRect(boxMin, boxMax, IM_COL32(143, 164, 151, 130), 7.f);
            AddOverlayText(drawList, ImVec2(boxMin.x + 10.f, boxMin.y + 7.f), label);
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

    SceneViewPanel::SceneViewPanel(EditorContext &context, const ShortcutRegistry &shortcutRegistry, Engine::AssetManager &assetManager)
        : m_Context(context), m_ShortcutRegistry(shortcutRegistry), m_AssetManager(assetManager)
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
        m_LightProxyPass.SetBillboardIcon(icons.lightBillboard);
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
            ImDrawList *drawList = ImGui::GetWindowDrawList();
            drawList->AddImage(textureId,
                               origin,
                               ImVec2(origin.x + width, origin.y + height),
                               ImVec2(0.f, 1.f),
                               ImVec2(1.f, 0.f));
            if (m_Context.settings.viewport.pipelineBenchmarkEnabled)
            {
                DrawOverlay(origin, width, height);
            }
            else if (SceneViewPanelDetail::IsShadowMapInspector(m_Context))
            {
                SceneViewPanelDetail::DrawShadowMapInspectorLabel(
                    drawList,
                    origin,
                    height,
                    m_Context.settings.postProcess.shadowMapCascadeIndex);
            }
            else
            {
                m_LightProxyPass.Draw(m_Context, origin, width, height);
                m_Gizmo.Draw(m_Context, origin, width, height);
                DrawViewportToolbar(origin, width);
                DrawOverlay(origin, width, height);
                HandlePicking(origin, width, height);
            }
            ImGui::Dummy(ImVec2(width, height));
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
        const bool benchmarkEnabled = m_Context.settings.viewport.pipelineBenchmarkEnabled;
        if (!benchmarkEnabled && (m_Context.ui.cleanSceneView || !m_Context.ui.showSceneViewInfo))
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
        char pathLine[128]{};
        char cameraLine[128]{};
        char drawLine[160]{};
        char visibleLine[224]{};
        char cpuLine[160]{};
        char frameLine[192]{};
        char uiLine[192]{};
        char passLine[192]{};
        char passDrawLine[192]{};
        char glLine[320]{};
        char submitLine[256]{};
        char gpuLine[256]{};
        char postGpuLine[192]{};
        char benchmarkLine[256]{};
        char uploadLine[160]{};
        char hoveredLine[128]{};
        char focusedLine[128]{};
        char keysLine[160]{};

        const Engine::FrameStatistics &stats = m_Context.sceneView.rendererStats;
        const EditorFrameStatistics &frameStats = m_Context.frameStats;
        const Engine::RenderPath renderPath = static_cast<Engine::RenderPath>(
            std::clamp(m_Context.settings.postProcess.renderPathIndex, 0, 2));
        const std::string_view renderPathName = Engine::RenderPathName(renderPath);
        const double uploadMegabytes = static_cast<double>(stats.TotalUploadBytes()) / (1024.0 * 1024.0);
        const double gBufferMegabytes = static_cast<double>(stats.deferredGBufferBytes) / (1024.0 * 1024.0);
        const double indirectRunAverage = stats.indirectRuns > 0u
                                              ? static_cast<double>(stats.indirectRunCommands) /
                                                    static_cast<double>(stats.indirectRuns)
                                              : 0.0;
        std::snprintf(sizeLine, sizeof(sizeLine), "Size: %.f x %.f", width, height);
        std::snprintf(fpsLine, sizeof(fpsLine), "FPS: %.1f", ImGui::GetIO().Framerate);
        std::snprintf(
            pathLine,
            sizeof(pathLine),
            "Render Path: %.*s",
            static_cast<int>(renderPathName.size()),
            renderPathName.data());
        std::snprintf(drawLine, sizeof(drawLine), "Draws/Cmds: %llu/%u, Instances: %llu, Tris: %llu",
                      static_cast<unsigned long long>(stats.drawCalls),
                      stats.drawBatches,
                      static_cast<unsigned long long>(stats.instances),
                      static_cast<unsigned long long>(stats.triangles));
        std::snprintf(visibleLine, sizeof(visibleLine), "Visible: %u  O/U/T: %u/%u/%u  Lights: %u/%u  Clusters/Refs/Max/Ov: %u/%u/%u/%u  Mat/Sets: %u/%u",
                      stats.visibleSubmissions,
                      stats.opaqueItems,
                      stats.unlitItems,
                      stats.transparentItems,
                      stats.lightCount,
                      stats.localLightCount,
                      stats.clusterCount,
                      stats.clusterLightReferences,
                      stats.maxLightsPerCluster,
                      stats.clusterOverflowedLightReferences,
                      stats.materialInstances,
                      stats.materialResourceSets);
        std::snprintf(cpuLine, sizeof(cpuLine), "CPU: scene %.2f (collect %.2f, cluster %.2f), graph build/exec %.2f/%.2f ms",
                      stats.sceneBuildCpuMs,
                      stats.sceneCollectionCpuMs,
                      stats.clusterBuildCpuMs,
                      stats.renderGraphBuildCpuMs,
                      stats.renderGraphExecuteCpuMs);
        std::snprintf(frameLine, sizeof(frameLine), "Frame: %.2f ms  UI build %.2f, Scene %.2f, UI draw %.2f",
                      frameStats.frameCpuMs,
                      frameStats.uiBuildCpuMs,
                      frameStats.sceneRenderCpuMs,
                      frameStats.uiRenderCpuMs);
        std::snprintf(uiLine, sizeof(uiLine), "UI: lists/cmds %u/%u, vtx/idx %u/%u, backend %.2f ms",
                      frameStats.imgui.drawLists,
                      frameStats.imgui.drawCommands,
                      frameStats.imgui.vertexCount,
                      frameStats.imgui.indexCount,
                      frameStats.imgui.renderCpuMs);
        if (renderPath == Engine::RenderPath::Deferred)
        {
            std::snprintf(passLine, sizeof(passLine), "Pass: GBuf %.2f, Light %.2f, Fwd %.2f, Trans %.2f, Post %.2f ms",
                          stats.deferredGBufferCpuMs,
                          stats.deferredLightingCpuMs,
                          stats.forwardOpaqueCpuMs,
                          stats.forwardTransparentCpuMs,
                          stats.postProcessCpuMs);
            std::snprintf(passDrawLine, sizeof(passDrawLine), "Pass Draws: Sh/GBuf/Light/Fwd/Tr/Post %llu/%llu/%llu/%llu/%llu/%llu",
                          static_cast<unsigned long long>(stats.shadowDrawCalls),
                          static_cast<unsigned long long>(stats.deferredGBufferDrawCalls),
                          static_cast<unsigned long long>(stats.deferredLightingDrawCalls),
                          static_cast<unsigned long long>(stats.forwardOpaqueDrawCalls),
                          static_cast<unsigned long long>(stats.forwardTransparentDrawCalls),
                          static_cast<unsigned long long>(stats.postProcessDrawCalls));
        }
        else
        {
            std::snprintf(passLine, sizeof(passLine), "Pass: Fwd %.2f, Sky %.2f, Trans %.2f, Post %.2f ms",
                          stats.forwardOpaqueCpuMs,
                          stats.skyboxCpuMs,
                          stats.forwardTransparentCpuMs,
                          stats.postProcessCpuMs);
            std::snprintf(passDrawLine, sizeof(passDrawLine), "Pass Draws: Sh/Fwd/Sky/Tr/Post %llu/%llu/%llu/%llu/%llu",
                          static_cast<unsigned long long>(stats.shadowDrawCalls),
                          static_cast<unsigned long long>(stats.forwardOpaqueDrawCalls),
                          static_cast<unsigned long long>(stats.skyboxDrawCalls),
                          static_cast<unsigned long long>(stats.forwardTransparentDrawCalls),
                          static_cast<unsigned long long>(stats.postProcessDrawCalls));
        }
        std::snprintf(glLine, sizeof(glLine), "GL: RP %llu, Draw/MDI/Cmd %llu/%llu/%llu, P/VAO/Prim %llu/%llu/%llu, VB/IB %llu/%llu, Set %llu, Tex/Samp %llu/%llu, IBuf %llu, Bar C/E/S %llu/%llu/%llu",
                      static_cast<unsigned long long>(stats.backend.renderPasses),
                      static_cast<unsigned long long>(stats.backend.drawCalls),
                      static_cast<unsigned long long>(stats.backend.indirectDrawCalls),
                      static_cast<unsigned long long>(stats.backend.indirectDrawCommands),
                      static_cast<unsigned long long>(stats.backend.programBinds),
                      static_cast<unsigned long long>(stats.backend.vaoBinds),
                      static_cast<unsigned long long>(stats.backend.renderPrimitiveBinds),
                      static_cast<unsigned long long>(stats.backend.vertexBufferBinds),
                      static_cast<unsigned long long>(stats.backend.indexBufferBinds),
                      static_cast<unsigned long long>(stats.backend.resourceSetBinds),
                      static_cast<unsigned long long>(stats.backend.textureBinds),
                      static_cast<unsigned long long>(stats.backend.samplerBinds),
                      static_cast<unsigned long long>(stats.backend.indirectBufferBinds),
                      static_cast<unsigned long long>(stats.backend.barrierCandidates),
                      static_cast<unsigned long long>(stats.backend.barriers),
                      static_cast<unsigned long long>(stats.backend.barriersSuppressed));
        std::snprintf(
            submitLine,
            sizeof(submitLine),
            "Submit: direct %llu, MDI runs/cmd/avg/max %llu/%llu/%.1f/%llu, breaks M/G/I/S %llu/%llu/%llu/%llu",
            static_cast<unsigned long long>(stats.directSubmittedCommands),
            static_cast<unsigned long long>(stats.indirectRuns),
            static_cast<unsigned long long>(stats.indirectRunCommands),
            indirectRunAverage,
            static_cast<unsigned long long>(stats.maxIndirectRunCommands),
            static_cast<unsigned long long>(stats.indirectMergeBreaks),
            static_cast<unsigned long long>(stats.indirectGeometryBreaks),
            static_cast<unsigned long long>(stats.indirectInvalidBreaks),
            static_cast<unsigned long long>(stats.indirectShortRuns));
        if (stats.benchmarkEnabled)
        {
            std::snprintf(
                gpuLine,
                sizeof(gpuLine),
                "GPU: frame %.2f  Sh/Fwd/Sky/GBuf/Light/Tr/Grid/Post %.2f/%.2f/%.2f/%.2f/%.2f/%.2f/%.2f/%.2f ms",
                stats.gpuFrameMs,
                stats.shadowGpuMs,
                stats.forwardOpaqueGpuMs,
                stats.skyboxGpuMs,
                stats.deferredGBufferGpuMs,
                stats.deferredLightingGpuMs,
                stats.forwardTransparentGpuMs,
                stats.worldGridGpuMs,
                stats.postProcessGpuMs);
            std::snprintf(
                postGpuLine,
                sizeof(postGpuLine),
                "Post GPU: prefilter/downsample/upsample/composite %.2f/%.2f/%.2f/%.2f ms",
                stats.bloomPrefilterGpuMs,
                stats.bloomDownsampleGpuMs,
                stats.bloomUpsampleGpuMs,
                stats.postProcessCompositeGpuMs);
            if (stats.benchmarkComplete)
            {
                std::snprintf(
                    benchmarkLine,
                    sizeof(benchmarkLine),
                    "Benchmark: complete  CPU med/p95 %.2f/%.2f  GPU med/p95 %.2f/%.2f ms",
                    stats.benchmarkCpuMedianMs,
                    stats.benchmarkCpuP95Ms,
                    stats.benchmarkGpuMedianMs,
                    stats.benchmarkGpuP95Ms);
            }
            else if (stats.benchmarkWarmupFrame < stats.benchmarkWarmupFrames)
            {
                std::snprintf(
                    benchmarkLine,
                    sizeof(benchmarkLine),
                    "Benchmark: warmup %u/%u",
                    stats.benchmarkWarmupFrame,
                    stats.benchmarkWarmupFrames);
            }
            else
            {
                std::snprintf(
                    benchmarkLine,
                    sizeof(benchmarkLine),
                    "Benchmark: sampling %u/%u",
                    stats.benchmarkSampleFrame,
                    stats.benchmarkSampleFrames);
            }
        }
        else
        {
            std::snprintf(gpuLine, sizeof(gpuLine), "GPU: timestamp sampling disabled");
            std::snprintf(postGpuLine, sizeof(postGpuLine), "Post GPU: timestamp sampling disabled");
            std::snprintf(benchmarkLine, sizeof(benchmarkLine), "Benchmark: off");
        }
        std::snprintf(uploadLine, sizeof(uploadLine), "Upload: %.2f MB, chunks %llu  Mesh/Prim/Tex: %u/%u/%u  GBuffer %.2f MB",
                      uploadMegabytes,
                      static_cast<unsigned long long>(stats.bufferUploadChunks),
                      stats.meshUploads,
                      stats.meshPrimitiveUploads,
                      stats.textureUploads,
                      gBufferMegabytes);
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
                                       ImGui::CalcTextSize(pathLine).x,
                                       ImGui::CalcTextSize(drawLine).x,
                                       ImGui::CalcTextSize(visibleLine).x,
                                       ImGui::CalcTextSize(cpuLine).x,
                                       ImGui::CalcTextSize(frameLine).x,
                                       ImGui::CalcTextSize(uiLine).x,
                                       ImGui::CalcTextSize(passLine).x,
                                       ImGui::CalcTextSize(passDrawLine).x,
                                       ImGui::CalcTextSize(glLine).x,
                                       ImGui::CalcTextSize(submitLine).x,
                                       ImGui::CalcTextSize(gpuLine).x,
                                       ImGui::CalcTextSize(postGpuLine).x,
                                       ImGui::CalcTextSize(benchmarkLine).x,
                                       ImGui::CalcTextSize(uploadLine).x,
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
        const float overlayHeight = paddingY * 2.f + lineHeight * (presentation ? 20.f : 19.f);
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

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), pathLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), drawLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), visibleLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), cpuLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), frameLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), uiLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), passLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), passDrawLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), glLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), submitLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), gpuLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), postGpuLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), benchmarkLine);
            y += lineHeight;

            SceneViewPanelDetail::AddOverlayText(drawList, ImVec2(x, y), uploadLine);
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
            const float y1 = boxMin.y + boxSize * 0.7f;
            const float x2 = boxMin.x + boxSize * 0.76f;
            const float y2 = boxMin.y + boxSize * 0.3f;
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

    void SceneViewPanel::HandlePicking(const ImVec2 &origin, float width, float height)
    {
        if (m_Context.sceneView.inputCaptured || m_NavigationCaptureActive || m_Gizmo.IsUsingOrHovered())
        {
            return;
        }

        if (!m_Context.sceneView.hovered || !ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            return;
        }

        const ImGuiIO &io = ImGui::GetIO();
        const ImVec2 mouse = io.MousePos;
        if (mouse.x < origin.x || mouse.y < origin.y || mouse.x > origin.x + width || mouse.y > origin.y + height)
        {
            return;
        }
        const bool overTopToolbarBand = mouse.y <= origin.y + SceneViewPanelDetail::OverlayPadding + SceneViewPanelDetail::IconButtonSize + 10.f;
        const bool overLeftToolbar = overTopToolbarBand && mouse.x <= origin.x + 190.f;
        const bool overRightToolbar = overTopToolbarBand && mouse.x >= origin.x + width - 150.f;
        if (overLeftToolbar || overRightToolbar)
        {
            return;
        }

        PickingRequest request{};
        request.viewportPosition = {mouse.x - origin.x, mouse.y - origin.y};
        request.appendSelection = io.KeyShift;
        request.toggleSelection = io.KeyCtrl;
        const Engine::EntityId picked = m_Picking.Pick(m_Context, request, &m_AssetManager);
        m_Picking.Select(m_Context, picked, request);
    }

    void SceneViewPanel::ForwardInput()
    {
        if (!m_InputCallback)
        {
            return;
        }

        if (SceneViewPanelDetail::IsShadowMapInspector(m_Context) ||
            m_Context.settings.viewport.pipelineBenchmarkEnabled)
        {
            m_NavigationCaptureActive = false;
            m_Context.sceneView.inputCaptured = false;
            EditorCameraInputFrame snapshot{};
            snapshot.escapePressed = true;
            m_InputCallback(snapshot);
            return;
        }

        const ImGuiIO &io = ImGui::GetIO();
        const bool rightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && m_Context.sceneView.hovered)
        {
            m_NavigationCaptureActive = true;
        }
        if (!rightMouseDown)
        {
            m_NavigationCaptureActive = false;
        }

        const bool captured = m_NavigationCaptureActive || m_Context.sceneView.playFlyMode;
        m_Context.sceneView.inputCaptured = captured;
        if (captured)
        {
            ImGui::SetNextFrameWantCaptureMouse(true);
            ImGui::SetNextFrameWantCaptureKeyboard(true);
        }

        EditorCameraInputFrame snapshot{};
        snapshot.mouseDeltaX = io.MouseDelta.x;
        snapshot.mouseDeltaY = io.MouseDelta.y;
        snapshot.hovered = m_Context.sceneView.hovered || captured;
        snapshot.focused = m_Context.sceneView.focused || captured;
        snapshot.rightMouseDown = rightMouseDown && m_NavigationCaptureActive;
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
