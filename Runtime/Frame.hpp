#pragma once

/// @file
/// @brief Runtime 的一次窗口帧编排. 正常候选, 安全 UI 和取消共享同一图及宿主提交链.
/// Main 只调度安全帧槽, 输入和退出; Renderer/Editor 继续拥有渲染声明和 UI 请求.

#include <cstddef>
#include <cstdint>
#include <variant>

#include <Core/Diagnostics.hpp>
#include <Core/FrameArena.hpp>
#include <DeviceHost.hpp>
#include <Renderer/Paths/Forward.hpp>
#include <Session/EditorSession.hpp>

#include "FrameStatistics.hpp"

namespace Physara::Runtime
{
    inline constexpr std::size_t FrameSlotCount = Platform::MaxPresentationSlots;

    /// @brief 同槽 Ready 后才可复用的 CPU 载荷, 图存储和最近提交记账.
    /// @note Platform 拥有 Event 和目标代. 本值只保存 Runtime 的存储与预算, 不自行判定 GPU 完成.
    /// sceneFrame/mainView/forward 独立于两份 arena, 图重试不重新提取 Scene 或复用在途 Draw 存储.
    struct FrameSlot
    {
        Renderer::SceneFrameStorage sceneFrame; ///< 同槽 Ready 后按 Scene 和内容版本更新.
        Renderer::ViewWork mainView; ///< 本轮主 View 的独立索引区间.
        Renderer::ForwardFrame forward; ///< 版本引用保留到该槽实际完成, 上传字节借用止于录制结束.
        Core::FrameArena arena;
        Core::FrameArena graphArena; ///< 图析构回退不影响 UI/上传载荷.
        Editor::FrameSlotUiFrameBindings bindings;
        std::uint64_t lastSubmission = 0; ///< 本次取得的接受票据, 0 表示本轮尚未提交; 非零不代表颜色有效.
        std::size_t diagnosticUploadBytes = 0; ///< 同槽 Ready 后归还诊断上传额度.
    };

    /// @brief Rendered/SafeUi 包括接受后因窗口变化跳过交换, 不表示 GPU 已完成.
    /// Cancelled 可持有失败前缀票据; UploadPending 已归还无录制取得.
    enum class FrameStatus
    {
        Rendered,
        SafeUi,
        Cancelled,
        UploadPending
    };

    /// @brief 有界轮询各槽, 只在 Retry 时尝试其它槽, 失败不视作忙.
    [[nodiscard]] Platform::DeviceHost::PresentationAcquire
        AcquireFrame(Platform::DeviceHost &host, std::size_t &next);

    /// @brief 从封存 UI 推进一次取得, 执行前最多建立一张独立安全图.
    /// @pre slot 对应 presentation 的安全取得, commands 来自同一宿主且无活动录制.
    /// editorFrame 来自本轮尚未失效的 Advance, editor 已装配 Scene 文档.
    /// remainingDiagnosticBytes 已扣除其它槽的在途额度.
    /// @param editorSeconds 产生 editorFrame 的 Advance 实测秒数. 与本次 UI 准备时间共同归入 UI 阶段.
    /// @param statistics 此宿主的查询及统计 owner. 只在 View 接受本次场景输出后发布场景统计.
    /// @return 正常或可恢复状态; 文本错误表示必须退出, 未证明完成的工作仍归 RHI/宿主收拢.
    /// @note 准备值和图的 CPU 借用在返回前结束, 已发 GPU 引用由原物理包保活.
    [[nodiscard]] std::variant<FrameStatus, Core::BoundedText> RenderFrame(
        Platform::DeviceHost &host, Editor::EditorSession &editor, Renderer::View &view,
        Renderer::ForwardRenderer &forward,
        const Editor::EditorFrame &editorFrame, const Platform::PresentationFrame &presentation, FrameSlot &slot,
        std::size_t remainingDiagnosticBytes, RHI::CommandList &commands, Core::Diagnostics &diagnostics,
        FrameStatistics &statistics, double editorSeconds);
}
