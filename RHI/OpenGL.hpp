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

    enum class ShutdownPrecondition : std::uint8_t
    {
        Ready,
        ResourceInUse,
        InvalidState
    };

    enum class DrainResult : std::uint8_t
    {
        Ready,
        Pending,
        InvalidState
    };

    // 使用者已停止/join. 纯 CPU、无分配、无状态修改; knownRootReferences 为调用方实际拥有的根.
    // 内部上传 lease 不算外部 owner; 额外资源、列表和 Device 引用仍拒绝.
    [[nodiscard]] ShutdownPrecondition CheckShutdownPreconditions(RHI::Device *device,
                                                                  unsigned long knownRootReferences) noexcept;
    // 预检成功后在 owner/current 调用. Pending 保留设备/context/回调供重试, 已停止接收新工作.
    // Ready 已撤销全部内部寿命环并 Stopped; nativeError 仍可能报告清理首错, owner 随后销毁 context.
    // ContextUnavailable 不执行 GL, 不声称 GPU 完成; owner 必须销毁确切 context.
    [[nodiscard]] DrainResult DrainDeviceWork(RHI::Device *device, ShutdownMode mode,
                                              std::uint32_t *nativeError = nullptr);

    // 首发每个 context 只装配一个 Device. 工厂须在 owning/current 线程调用.
    // nativeError 借用到返回, 先置 0, GL 失败保存首个实际错误; bad_alloc 返回空.
    [[nodiscard]] DeviceHandle CreateDevice(const DeviceDesc &desc, std::uint32_t *nativeError = nullptr);

    // 先停止使用者并释放外部资源/列表及额外 Device 引用, 保留唯一根引用调用此入口.
    // Normal 需要 owning/current; ContextUnavailable 只撤销 CPU 状态, owner 随后必须释放 context.
    // 预检拒绝不改变设备; Pending 返回 false 并保留根供重试, 不得直接释放根/context.
    // Ready 后最终删除带错时 owner 必须销毁所属 context, 由它回收未成功删除的原生对象.
    [[nodiscard]] bool ShutdownDevice(RHI::Device *device, ShutdownMode mode, std::uint32_t *nativeError = nullptr);
}
