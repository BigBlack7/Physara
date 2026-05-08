#include "EditorApp.hpp"

#include <filesystem>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

namespace Physara::Editor
{
    void EditorApp::Init(RHI::IImGuiBackend *backend)
    {
        m_Backend = backend;
        m_LayoutInitialized = false;
        m_DockspaceId = 0;
    }

    void EditorApp::OnUIRender()
    {
        if (m_Backend == nullptr)
        {
            return;
        }

        m_Backend->BeginFrame();
        ImGui::NewFrame();

        const ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
        ImGui::DockSpaceOverViewport(dockspaceId, ImGui::GetMainViewport(), ImGuiDockNodeFlags_None);
        m_DockspaceId = static_cast<std::uint32_t>(dockspaceId);

        if (!m_LayoutInitialized)
        {
            InitDefaultLayout();
            m_LayoutInitialized = true;
        }

        // Panels will be drawn here in Phase 4.
        ImGui::ShowDemoWindow();

        m_Backend->EndFrame();
        m_Backend->RenderDrawData();
    }

    void EditorApp::InitDefaultLayout()
    {
        ImGuiIO &io = ImGui::GetIO();
        if (io.IniFilename != nullptr && std::filesystem::exists(io.IniFilename))
        {
            return;
        }

        ImGuiID dockspace = m_DockspaceId != 0
                                ? static_cast<ImGuiID>(m_DockspaceId)
                                : ImGui::GetID("MainDockSpace");

        ImGui::DockBuilderRemoveNode(dockspace);
        ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->Size);

        ImGuiID center = dockspace;
        ImGuiID left = 0;
        ImGuiID right = 0;
        ImGuiID bottom = 0;

        const float leftRatio = 0.15f;
        const float rightRatio = 0.20f / (1.0f - leftRatio);
        const float bottomRatio = 0.25f;
        const float bottomLeftRatio = 0.6f;

        ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, leftRatio, &left, &center);

        ImGuiID rightCenter = 0;
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, rightRatio, &right, &rightCenter);

        ImGuiID topCenter = 0;
        ImGui::DockBuilderSplitNode(rightCenter, ImGuiDir_Down, bottomRatio, &bottom, &topCenter);

        ImGuiID bottomLeft = 0;
        ImGuiID bottomRight = 0;
        ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Left, bottomLeftRatio, &bottomLeft, &bottomRight);

        ImGui::DockBuilderDockWindow("Hierarchy", left);
        ImGui::DockBuilderDockWindow("Scene", topCenter);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("Content Browser", bottomLeft);
        ImGui::DockBuilderDockWindow("Log", bottomRight);

        ImGui::DockBuilderFinish(dockspace);
    }
}