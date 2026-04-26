#pragma once

#include <functional>
#include <string_view>

namespace Physara::Platform
{
    class IWindow
    {
    public:
        using ResizeCallback = std::function<void(int, int)>;
        virtual ~IWindow() = default;

        // 生命周期
        virtual bool Create(std::string_view title, int width, int height) = 0;
        virtual void Destroy() = 0;
        virtual bool IsCloseRequested() const = 0;

        // 每帧操作
        virtual void PollEvents() = 0;
        virtual void SwapBuffers() = 0;

        // 窗口属性
        virtual int GetWidth() const = 0;
        virtual int GetHeight() const = 0;
        virtual void SetVSync(bool enabled) = 0;
        virtual void SetTitle(std::string_view title) = 0;

        // 回调
        virtual void SetResizeCallback(ResizeCallback callback) = 0;

        // 底层句柄(GLFWwindow*以void*形式返回, 供Device/Input/ImGui使用)
        virtual void *GetNativeHandle() const = 0;
    };
}