#include "OpenGLImGuiBackend.hpp"

#include <Engine/Core/Log.hpp>

#include <glad/glad.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui.h>

namespace Physara::RHI
{
    bool OpenGLImGuiBackend::Initialize(RHIDevice *device, void *windowHandle)
    {
        // Editor只依赖IImGuiBackend; 具体imgui_impl_glfw/opengl3适配器封装在Backend内
        (void)device;

        if (m_Initialized)
        {
            PHYSARA_CORE_WARN("OpenGLImGuiBackend already initialized.");
            return true;
        }

        if (windowHandle == nullptr)
        {
            PHYSARA_CORE_ERROR("OpenGLImGuiBackend::Initialize failed: windowHandle is null.");
            return false;
        }

        if (ImGui::GetCurrentContext() == nullptr)
        {
            // 当前后端在没有外部ImGui context时负责创建并在Shutdown销毁
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            m_OwnsContext = true;
        }
        else
        {
            m_OwnsContext = false;
        }

        ImGuiIO &io = ImGui::GetIO();
        // Docking是编辑器工作台的基础能力; 键盘导航用于后续工具窗口和弹窗
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        if (!ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow *>(windowHandle), true))
        {
            PHYSARA_CORE_ERROR("ImGui_ImplGlfw_InitForOpenGL failed.");
            if (m_OwnsContext)
            {
                ImGui::DestroyContext();
                m_OwnsContext = false;
            }
            return false;
        }

        if (!ImGui_ImplOpenGL3_Init("#version 460"))
        {
            PHYSARA_CORE_ERROR("ImGui_ImplOpenGL3_Init failed.");
            ImGui_ImplGlfw_Shutdown();
            if (m_OwnsContext)
            {
                ImGui::DestroyContext();
                m_OwnsContext = false;
            }
            return false;
        }

        m_Initialized = true;
        return true;
    }

    void OpenGLImGuiBackend::BeginFrame()
    {
        if (!m_Initialized)
        {
            return;
        }

        // 后端NewFrame先更新renderer/platform层输入状态, EditorApp随后调用ImGui::NewFrame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }

    void OpenGLImGuiBackend::EndFrame()
    {
        if (!m_Initialized)
        {
            return;
        }

        ImGui::Render();
    }

    void OpenGLImGuiBackend::RenderDrawData()
    {
        if (!m_Initialized)
        {
            return;
        }

        if (ImDrawData *drawData = ImGui::GetDrawData())
        {
            // 将ImGui draw lists翻译为OpenGL draw calls. 此处是唯一接触imgui_impl_opengl3的位置, 其他地方只依赖imgui_impl_glfw和imgui核心接口
            ImGui_ImplOpenGL3_RenderDrawData(drawData);
        }
    }

    ImGuiTextureHandle OpenGLImGuiBackend::CreateTextureRGBA(std::uint32_t width, std::uint32_t height, const void *pixels)
    {
        if (!m_Initialized || pixels == nullptr || width == 0 || height == 0)
        {
            return 0;
        }

        // 纹理创建和销毁接口由Backend实现, Editor只持有ImGuiTextureHandle, 通过Backend上传像素数据并获取对应的ImGuiTextureHandle, 后续UI渲染时将该handle传回ImGui作为纹理ID使用
        GLuint texture = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &texture);
        glTextureStorage2D(texture, 1, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        glTextureSubImage2D(texture, 0, 0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                            GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        return static_cast<ImGuiTextureHandle>(texture);
    }

    void OpenGLImGuiBackend::DestroyTexture(ImGuiTextureHandle texture)
    {
        if (texture == 0)
        {
            return;
        }

        const GLuint glTexture = static_cast<GLuint>(texture);
        glDeleteTextures(1, &glTexture);
    }

    void OpenGLImGuiBackend::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        if (m_OwnsContext && ImGui::GetCurrentContext() != nullptr)
        {
            ImGui::DestroyContext();
        }

        m_Initialized = false;
        m_OwnsContext = false;
    }
}