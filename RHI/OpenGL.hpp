#pragma once

/// @file
/// @brief 不暴露 GL/GLFW 类型的后端互操作合同, 外部 owner 管理 context, Device 管理资源与工作退休.

#include <cstdint>

#include "RHI.hpp"

namespace Physara::RHI::OpenGL
{
    /// @brief 借用外部 owner 的 context 访问检查, 同时确认 owner 线程与确切 current context.
    /// @note user/回调须覆盖全部 Device CPU 引用, 不抛出, 不重入; 后端不迁移 context 或加载入口.
    struct ContextAccess
    {
        void *user = nullptr;
        bool (*isCurrent)(void *) noexcept = nullptr;
    };

    /// @brief 创建设备的借用依赖, callback 与 context 均由外部 owner 保证寿命.
    struct DeviceDesc
    {
        MessageCallback *messageCallback = nullptr;
        ContextAccess context;
    };

    /// @brief Normal 在有效 current context 上清理; ContextUnavailable 仅撤销 CPU 状态, 不调用 GL.
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

    /// @brief 退出收拢结果; Pending 要求保留 Device/context/回调并重试, InvalidState 未满足调用合同.
    enum class DrainResult : std::uint8_t
    {
        Ready,
        Pending,
        InvalidState
    };

    /// @brief 纯 CPU, 无分配且不改状态的关闭预检, 使用者须已停止/join.
    /// @param knownRootReferences 调用方实际拥有的根数, 内部上传 lease 不算外部 owner.
    /// @return 额外资源, 列表或 Device 引用存在时拒绝, 本检查不承担并发停止协议.
    [[nodiscard]] ShutdownPrecondition CheckShutdownPreconditions(RHI::Device *device,
                                                                  unsigned long knownRootReferences) noexcept;
    /// @brief 预检成功后在 owner 线程收拢, Normal 还要求 owning context current.
    /// @return Pending 停止新工作并保留原生环境; Ready 撤销内部寿命环且进入 Stopped.
    /// @note Ready 仍可能通过 nativeError 报告清理首错; ContextUnavailable 不声称 GPU 完成,
    /// 外部 owner 随后须销毁确切 context, 回收未成功删除的原生对象.
    [[nodiscard]] DrainResult DrainDeviceWork(RHI::Device *device, ShutdownMode mode,
                                              std::uint32_t *nativeError = nullptr);

    /// @brief 在 owning/current 线程为一个 context 创建唯一原生 Device.
    /// @param nativeError 可空输出, 先置零, GL 失败记录首个实际错误.
    /// @return 成功 owning handle, 参数/环境不支持或 bad_alloc 时为空.
    [[nodiscard]] DeviceHandle CreateDevice(const DeviceDesc &desc, std::uint32_t *nativeError = nullptr);

    /// @brief 复制当前帧的 SDR 颜色到默认后缓冲. 源 FBO 及附件依赖归入同一闭合物理包.
    /// @pre owner 线程上 owning context current, 无开放录制. source 属于同一设备, 已由该帧初始化,
    /// 为与后缓冲等大的单层, 单 mip, 单样本 SRGBA8_UNORM 颜色 FBO. 列表导出 ShaderResource.
    /// @param srgbBackBuffer 为当前默认后缓冲的实测编码, true 表示 sRGB, false 表示线性.
    /// @return true 表示复制成功, 随后须 Execute 同次录制并取得有效票据才可交换.
    /// false 时, 已发前缀由调用者沿有效提交或受控退出协议收拢.
    [[nodiscard]] bool PrepareWindowPresentation(Device *device, CommandList *closedCommands,
                                                  Framebuffer *source, bool srgbBackBuffer);

    /// @brief 收拢并停止仅保留一个根引用的设备, 调用前须释放外部资源, 列表和额外 Device 引用.
    /// @return 预检拒绝不改设备; Pending 返回 false 并保留根/context 供重试.
    /// @note Normal 需要 owning/current; ContextUnavailable 不调用 GL. Ready 后删除带错也须由 owner 销毁 context.
    [[nodiscard]] bool ShutdownDevice(RHI::Device *device, ShutdownMode mode, std::uint32_t *nativeError = nullptr);
}
