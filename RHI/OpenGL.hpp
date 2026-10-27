#pragma once

#include <cstdint>

#include "RHI.hpp"

namespace Physara::RHI::OpenGL
{
    // 只借用外部 owner; user 和回调必须覆盖全部 Device CPU 引用, 不抛出且不重入.
    // 回调同时确认 owner 线程和确切 context, 后端不负责窗口、入口加载或 context 迁移.
    struct ContextAccess
    {
        void *user = nullptr;
        bool (*isCurrent)(void *) noexcept = nullptr;
    };

    struct DeviceDesc
    {
        MessageCallback *messageCallback = nullptr;
        ContextAccess context;
    };

    enum class ShutdownMode : std::uint8_t
    {
        Normal,
        ContextUnavailable
    };

    // 首发每个 context 只装配一个 Device. 工厂须在 owning/current 线程调用.
    // nativeError 借用到返回, 先置 0, GL 失败保存首个实际错误; bad_alloc 返回空.
    [[nodiscard]] DeviceHandle CreateDevice(const DeviceDesc &desc, std::uint32_t *nativeError = nullptr);

    // 先停止使用者并释放资源及额外 Device 引用, 保留唯一根引用调用此入口.
    // Normal 需要 owning/current; ContextUnavailable 只撤销 CPU 状态, owner 随后必须释放 context.
    // 预检拒绝不改变设备; 清理已开始时即使失败也进入 Stopped. 无业务 GPU 完成含义.
    // 清理失败后 owner 必须销毁所属 context, 由它回收未成功删除的原生对象.
    [[nodiscard]] bool ShutdownDevice(RHI::Device *device, ShutdownMode mode, std::uint32_t *nativeError = nullptr);
}
