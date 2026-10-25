#pragma once

#include "DeviceHost.hpp"

struct GLFWwindow;

namespace Physara::Platform::Detail
{
    // 只供 native fixture 和 Editor 私有适配借用; 不进入 Engine 头/状态.
    struct GlfwAccess
    {
        // Ready 且在 owner 线程时返回非拥有指针, 关闭/回滚立即结束借用.
        [[nodiscard]] static GLFWwindow *Borrow(DeviceHost &host) noexcept;
    };
}
