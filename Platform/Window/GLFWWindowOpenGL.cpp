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
        if (m_window != nullptr)
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

        m_window = glfwCreateWindow(width, height, std::string(title).c_str(), nullptr, nullptr);
        if (m_window == nullptr)
        {
            glfwTerminate();
            throw std::runtime_error("GLFWWindowOpenGL::Create failed: glfwCreateWindow() returned null.");
        }

        m_width = width;
        m_height = height;

        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow *window, int w, int h)
                                       {
                                           auto *self = static_cast<GLFWWindowOpenGL *>(glfwGetWindowUserPointer(window));
                                           if (self == nullptr)
                                           {
                                               return;
                                           }

                                           self->m_width = w;
                                           self->m_height = h;

                                           if (self->m_resizeCallback)
                                           {
                                               self->m_resizeCallback(w, h);
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

            if(pixels != nullptr)
            {
                GLFWimage icon{};
                icon.width = iconWidth;
                icon.height = iconHeight;
                icon.pixels = pixels;
                glfwSetWindowIcon(m_window, 1, &icon);
                stbi_image_free(pixels);
            }
        }

        // 居中到主显示器工作区
        if (GLFWmonitor *monitor = glfwGetPrimaryMonitor())
        {
            int mx = 0, my = 0, mw = 0, mh = 0;
            glfwGetMonitorWorkarea(monitor, &mx, &my, &mw, &mh);

            int ww = 0, wh = 0;
            glfwGetWindowSize(m_window, &ww, &wh);

            glfwSetWindowPos(m_window, mx + (mw - ww) / 2, my + (mh - wh) / 2);
        }

        return true;
    }

    void GLFWWindowOpenGL::Destroy()
    {
        if (m_window!=nullptr)
        {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }

        glfwTerminate();

        m_width = 0;
        m_height = 0;
        m_resizeCallback = nullptr;
    }

    bool GLFWWindowOpenGL::IsCloseRequested() const
    {
        return (m_window == nullptr) ? true : (glfwWindowShouldClose(m_window) != 0);
    }

    void GLFWWindowOpenGL::PollEvents()
    {
        glfwPollEvents();
    }

    void GLFWWindowOpenGL::SwapBuffers()
    {
        if (m_window == nullptr)
        {
            throw std::runtime_error("GLFWWindowOpenGL::SwapBuffers failed: window is null.");
        }

        glfwSwapBuffers(m_window);
    }

    int GLFWWindowOpenGL::GetWidth() const
    {
        return m_width;
    }

    int GLFWWindowOpenGL::GetHeight() const
    {
        return m_height;
    }

    void GLFWWindowOpenGL::SetVSync(bool enabled)
    {
        glfwSwapInterval(enabled ? 1 : 0);
    }

    void GLFWWindowOpenGL::SetTitle(std::string_view title)
    {
        if (m_window == nullptr)
        {
            throw std::runtime_error("GLFWWindowOpenGL::SetTitle failed: window is null.");
        }

        glfwSetWindowTitle(m_window, std::string(title).c_str());
    }

    void GLFWWindowOpenGL::SetResizeCallback(ResizeCallback callback)
    {
        m_resizeCallback = std::move(callback);
    }

    void *GLFWWindowOpenGL::GetNativeHandle() const
    {
        return m_window;
    }
}