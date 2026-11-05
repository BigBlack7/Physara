#pragma once

/// @file
/// @brief 原生适配层对宿主窗口的受限借用入口, 不向 Engine 暴露 GLFW 类型.

#include "DeviceHost.hpp"

struct GLFWwindow;

namespace Physara::Platform::Detail
{
    /// @brief 供原生消费者或 Editor 私有适配访问窗口, 不转移窗口或 GLFW 生命周期所有权.
    struct GlfwAccess
    {
        /// @brief Ready 且在 owner 线程时返回非拥有指针, 否则为空; 关闭/回滚立即结束借用.
        [[nodiscard]] static GLFWwindow *Borrow(DeviceHost &host) noexcept;
    };
}
