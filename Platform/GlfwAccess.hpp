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
        /// @brief 接入前已发布的最后序号及下一次发布中已有的事件前缀长度.
        struct InputBoundary
        {
            std::uint64_t publishedSequence = 0;
            std::size_t pendingCount = 0;
        };

        /// @brief Ready 且在 owner 线程时返回非拥有指针, 否则为空. 关闭或回滚结束借用.
        /// @param inputBoundary 可选的输出. 成功借用时读取序号及前缀长度, 保留宿主的输入 bank.
        [[nodiscard]] static GLFWwindow *Borrow(DeviceHost &host, InputBoundary *inputBoundary = nullptr) noexcept;
    };
}
