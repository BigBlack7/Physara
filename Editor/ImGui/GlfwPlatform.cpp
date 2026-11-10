#include "GlfwPlatform.hpp"

#include <algorithm>
#include <cassert>
#include <limits>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>

#include <GlfwAccess.hpp>
#include <Input.hpp>

/// @file
/// @brief 接合官方 GLFW backend, 同步窗口事实并推进 ImGui 的帧准备.
/// Session 在平台接入后选择字体准备方式. 析构先恢复原生回调, 再销毁借用宿主窗口的 UI context.

namespace Physara::Editor::Detail
{
    GlfwPlatform::~GlfwPlatform()
    {
        if (m_Context != nullptr)
        {
            assert(ImGui::GetCurrentContext() == m_Context);
            // 创建失败可能只留下 context. 已建立的平台适配先恢复 GLFW 回调及 Win32 窗口过程.
            if (ImGui::GetIO().BackendPlatformUserData != nullptr)
            {
                ImGui_ImplGlfw_Shutdown();
            }
            ImGui::DestroyContext(m_Context);
        }
    }

    std::optional<Core::BoundedText> GlfwPlatform::Attach(Platform::DeviceHost &host)
    {
        // 先核对窗口借用和单 context 前提, 再取得 UI 资源或安装回调.
        auto *window = Platform::Detail::GlfwAccess::Borrow(host);
        if (window == nullptr)
        {
            return Core::BoundedText("Editor initialization requires a ready host on its owner thread");
        }
        if (ImGui::GetCurrentContext() != nullptr)
        {
            return Core::BoundedText("Editor initialization found an active ImGui context");
        }
        if (!IMGUI_CHECKVERSION())
        {
            return Core::BoundedText("Editor ImGui version or data layout mismatch");
        }

        m_Context = ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        // 布局和面板停靠限于主窗口, 停靠设置保存在会话内存中.
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.IniFilename = nullptr;

        // 宿主回调已安装. 官方 backend 负责串接和恢复, 保留宿主 user pointer.
        if (!ImGui_ImplGlfw_InitForOther(window, true))
        {
            return Core::BoundedText("Editor GLFW input adapter initialization failed");
        }
        m_Window = window;
        return std::nullopt;
    }

    void GlfwPlatform::DiscardInput(bool focused)
    {
        // 先关闭交互事件入队, 再清掉旧队列和已应用的键鼠状态.
        auto &io = ImGui::GetIO();
        io.SetAppAcceptingEvents(false);
        io.ClearEventsQueue();
        io.ClearInputKeys();
        io.ClearInputMouse();
        // 固定版本的 ClearInputMouse 保留点击历史. 中断多击序列, 独立于恢复帧的时间步长.
        constexpr double expiredClickTime = -std::numeric_limits<double>::infinity();
        std::ranges::fill(io.MouseClickedTime, expiredClickTime);
        std::ranges::fill(io.MouseReleasedTime, expiredClickTime);
        std::ranges::fill(io.MouseClickedLastCount, 0);
        // 活动项和拖放载荷由 context 单独维护, 随输入失效一起清理.
        ImGui::ClearActiveID();
        ImGui::ClearDragDrop();
        // 焦点通知独立于键鼠入队开关, 用发布末值重建当前焦点事实.
        io.AddFocusEvent(focused);
    }

    void GlfwPlatform::ResumeInput()
    {
        // 归还可能把指针移回捕获前的位置, GLFW 可以过滤重复坐标通知.
        // 在下一批事件前采样一次实际逻辑位置, 专供 ImGui 建立命中基线.
        Platform::LogicalPoint position;
        glfwGetCursorPos(m_Window, &position.x, &position.y);
        auto &io = ImGui::GetIO();
        io.SetAppAcceptingEvents(true);
        io.AddMousePosEvent(static_cast<float>(position.x), static_cast<float>(position.y));
    }

    void GlfwPlatform::BeginFrame(const Platform::WindowState &window, bool resumed)
    {
        // 先让 backend 更新时钟和窗口相关状态, 再将帧尺寸统一到宿主发布.
        ImGui_ImplGlfw_NewFrame();
        auto &io = ImGui::GetIO();
        const auto width = static_cast<float>(window.clientSizeLogical.width);
        const auto height = static_cast<float>(window.clientSizeLogical.height);
        // 与业务命中使用同一次发布. 像素比例独立于 contentScale.
        io.DisplaySize = {width, height};
        io.DisplayFramebufferScale = {
            width > 0.0f ? static_cast<float>(window.framebufferSizePixels.width) / width : 1.0f,
            height > 0.0f ? static_cast<float>(window.framebufferSizePixels.height) / height : 1.0f};
        if (resumed)
        {
            // backend 已更新自己的时钟, 恢复首帧沿用它的启动步长.
            io.DeltaTime = 1.0f / 60.0f;
        }
        // 使用 Session 已选定的 atlas 模式, 在布局之前推进固定 ImGui 的帧内状态.
        ImGui::NewFrame();
    }
}
