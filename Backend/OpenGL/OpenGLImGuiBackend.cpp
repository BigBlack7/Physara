#include "OpenGLImGuiBackend.hpp"

#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui.h>

namespace Physara::RHI
{
    bool OpenGLImGuiBackend::Initialize(RHIDevice *device, void *windowHandle)
    {
        (void)device;

        if (windowHandle == nullptr || ImGui::GetCurrentContext() != nullptr)
        {
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();

        if (!ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow *>(windowHandle), true))
        {
            ImGui::DestroyContext();
            return false;
        }

        if (!ImGui_ImplOpenGL3_Init("#version 460"))
        {
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            return false;
        }

        return true;
    }

    void OpenGLImGuiBackend::BeginFrame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }

    void OpenGLImGuiBackend::EndFrame()
    {
        ImGui::Render();
    }

    void OpenGLImGuiBackend::RenderDrawData()
    {
        if (ImDrawData *drawData = ImGui::GetDrawData())
        {
            ImGui_ImplOpenGL3_RenderDrawData(drawData);
        }
    }

    void OpenGLImGuiBackend::Shutdown()
    {
        if (ImGui::GetCurrentContext() == nullptr)
        {
            return;
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}
