#include "GLFWWindowOpenGL.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace Physara::Platform
{
    GLFWWindowOpenGL::~GLFWWindowOpenGL()
    {
        Destroy();
    }

    bool GLFWWindowOpenGL::Create(std::string_view title, int width, int height)
    {
        if (m_Window != nullptr)
        {
            Destroy();
        }

        if (glfwInit() == GLFW_FALSE)
        {
            throw std::runtime_error("GLFWWindowOpenGL::Create failed: glfwInit() failed.");
        }

        // 固定OpenGL 4.6 Core Profile
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_Window = glfwCreateWindow(width, height, std::string(title).c_str(), nullptr, nullptr);
        if (m_Window == nullptr)
        {
            glfwTerminate();
            throw std::runtime_error("GLFWWindowOpenGL::Create failed: glfwCreateWindow() returned null.");
        }

        m_Width = width;
        m_Height = height;

        m_Context.window = this;
        m_Context.input = nullptr;

        glfwSetWindowUserPointer(m_Window, &m_Context);
        glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow *window, int w, int h)
                                       {
                                           auto *ctx = static_cast<GLFWWindowContext *>(glfwGetWindowUserPointer(window));
                                           if (ctx == nullptr || ctx->window == nullptr)
                                           {
                                               return;
                                           }

                                           auto *self = ctx->window;
                                           self->m_Width = w;
                                           self->m_Height = h;

                                           if (self->m_ResizeCallback)
                                           {
                                               self->m_ResizeCallback(w, h);
                                           }
                                           // End
                                       });

        // Physara Logo: Assets/Icons/icon_physara.png(失败时跳过, 不阻止窗口创建)
        {
            const std::string iconPath = std::string(ASSETS_PATH) + "Icons/icon_physara.png";
            int iconWidth = 0;
            int iconHeight = 0;
            int iconChannels = 0;
            unsigned char *pixels = stbi_load(iconPath.c_str(), &iconWidth, &iconHeight, &iconChannels, 4);

            if (pixels != nullptr)
            {
                GLFWimage icon{};
                icon.width = iconWidth;
                icon.height = iconHeight;
                icon.pixels = pixels;
                glfwSetWindowIcon(m_Window, 1, &icon);
                stbi_image_free(pixels);
            }
        }

        // 居中到主显示器工作区
        if (GLFWmonitor *monitor = glfwGetPrimaryMonitor())
        {
            int mx = 0, my = 0, mw = 0, mh = 0;
            glfwGetMonitorWorkarea(monitor, &mx, &my, &mw, &mh);

            int ww = 0, wh = 0;
            glfwGetWindowSize(m_Window, &ww, &wh);

            glfwSetWindowPos(m_Window, mx + (mw - ww) / 2, my + (mh - wh) / 2);
        }

        return true;
    }

    void GLFWWindowOpenGL::Destroy()
    {
        if (m_Window != nullptr)
        {
            glfwDestroyWindow(m_Window);
            m_Window = nullptr;
        }

        glfwTerminate();

        m_Width = 0;
        m_Height = 0;
        m_ResizeCallback = nullptr;
    }

    bool GLFWWindowOpenGL::IsCloseRequested() const
    {
        return (m_Window == nullptr) ? true : (glfwWindowShouldClose(m_Window) != 0);
    }

    void GLFWWindowOpenGL::PollEvents()
    {
        glfwPollEvents();
    }

    void GLFWWindowOpenGL::SwapBuffers()
    {
        if (m_Window == nullptr)
        {
            throw std::runtime_error("GLFWWindowOpenGL::SwapBuffers failed: window is null.");
        }

        glfwSwapBuffers(m_Window);
    }

    int GLFWWindowOpenGL::GetWidth() const
    {
        return m_Width;
    }

    int GLFWWindowOpenGL::GetHeight() const
    {
        return m_Height;
    }

    void GLFWWindowOpenGL::SetVSync(bool enabled)
    {
        glfwSwapInterval(enabled ? 1 : 0);
    }

    void GLFWWindowOpenGL::SetTitle(std::string_view title)
    {
        if (m_Window == nullptr)
        {
            throw std::runtime_error("GLFWWindowOpenGL::SetTitle failed: window is null.");
        }

        glfwSetWindowTitle(m_Window, std::string(title).c_str());
    }

    void GLFWWindowOpenGL::SetResizeCallback(ResizeCallback callback)
    {
        m_ResizeCallback = std::move(callback);
    }

    void *GLFWWindowOpenGL::GetNativeHandle() const
    {
        return m_Window;
    }
}