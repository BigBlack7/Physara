#pragma once

/// @file
/// @brief 窗口目标和单次取得值. Runtime 持有 token, 绘制只消费公共 RHI 目标.

#include <cstdint>

#include "Input.hpp"

#include <Core/Identity.hpp>
#include <RHI.hpp>

namespace Physara::Platform
{
    struct WindowIdentityTag;
    using WindowId = Core::Identity<WindowIdentityTag>;

    /// @brief 单次窗口取得身份, 由宿主代, 目标代和递增的取得号共同确定.
    /// @note 同尺寸复用沿用目标代, 但每次 BeginPresentation 都分配新的取得号.
    struct PresentationToken
    {
        WindowId window;
        std::uint64_t targetGeneration = 0;
        std::uint64_t acquisition = 0;
        bool operator==(const PresentationToken &) const = default;
    };

    /// @brief 拥有型 RHI 目标快照, extentPixels 是 framebuffer 的物理像素宽高.
    /// @note 纹理为单层, 单 mip, 单样本 SRGBA8_UNORM, 初态和出口均为 ShaderResource.
    /// 每次取得的首个写入须清除或完整覆盖. 使用者在宿主 Shutdown 前释放句柄.
    struct PresentTarget
    {
        RHI::TextureHandle texture;
        RHI::FramebufferHandle framebuffer;
        WindowExtent extentPixels;
    };

    /// @brief 本次取得的目标及凭据. 目标供绘制使用, token 由 Runtime 交回同一宿主.
    struct PresentationFrame
    {
        PresentTarget target;
        PresentationToken token;
    };

    /// @brief 本轮无可用目标时的正常结果. Suspended 表示窗口不可渲染, Retry 表示槽仍在等待完成.
    enum class PresentationStatus : std::uint8_t
    {
        Suspended,
        Retry
    };

    /// @brief 本次闭合录制的接受票据及颜色资格.
    /// @note colorValid 为 true 时可确认本次 UI 上传. 为 false 时票据只用于已发前缀退休.
    struct PresentationSubmission
    {
        std::uint64_t ticket = 0;
        bool colorValid = false;
    };

    /// @brief 消费取得后的交换结果. ticket 记录设备接受, swapped 记录交换成功.
    /// @note attempted 为 true 时, presentWaitSeconds 记录实际交换调用所在的单调 CPU 秒区间.
    /// GPU 完成由设备对票据的观察确定.
    struct PresentationResult
    {
        std::uint64_t ticket = 0;
        bool swapped = false;
        bool attempted = false;
        double presentWaitSeconds = 0.0;
    };
}
