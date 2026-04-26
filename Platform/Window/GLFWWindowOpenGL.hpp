#pragma once

#include <functional>
#include <string_view>

#include <Platform/Window/IWindow.hpp>

struct GLFWwindow;

namespace Physara::Platform
{
    class GLFWWindowOpenGL final : public IWindow
    {
    public:
        GLFWWindowOpenGL() = default;
        ~GLFWWindowOpenGL() override;

        bool Create(std::string_view title, int width, int height) override;
        void Destroy() override;
        bool IsCloseRequested() const override;

        void PollEvents() override;
        void SwapBuffers() override;

        int GetWidth() const override;
        int GetHeight() const override;
        void SetVSync(bool enabled) override;
        void SetTitle(std::string_view title) override;

        void SetResizeCallback(ResizeCallback callback) override;
        void *GetNativeHandle() const override;

    private:
        GLFWwindow *m_window{nullptr};
        int m_width{0};
        int m_height{0};
        ResizeCallback m_resizeCallback{nullptr};
    };
}