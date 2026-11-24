#pragma once

/// @file
/// @brief Runtime 的一次窗口帧编排. 正常候选, 安全 UI 和取消共享同一图及宿主提交链.
/// Main 只调度安全帧槽, 输入和退出; Renderer/Editor 继续拥有渲染声明和 UI 请求.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>

#include <Core/Diagnostics.hpp>
#include <Core/FrameArena.hpp>
#include <DeviceHost.hpp>
#include <Renderer/Paths/Forward.hpp>
#include <Renderer/RenderGraph/GraphCompiler.hpp>
#include <Session/EditorSession.hpp>

#include "FrameStatistics.hpp"

namespace Physara::Runtime
{
    inline constexpr std::size_t FrameSlotCount = Platform::MaxPresentationSlots;

    /// @brief 保存本次帧候选的图编译能力和预算. Options 引用的格式表及边界状态表只在本对象存活期间有效.
    class EditorGraphProfile final
    {
    public:
        /// @pre 在就绪宿主的 owner 线程构造. sceneViews 为本次候选的场景视图数, 有场景时才追加结构化 UAV 和数组 image 能力.
        /// @param directionalViews 已声明级联深度的 View 数, 用于选择 D32 数组能力及整图附件预算.
        EditorGraphProfile(const Platform::DeviceHost &host, std::size_t sceneViews, bool diagnostic,
                           std::size_t directionalViews = 0);
        EditorGraphProfile(const EditorGraphProfile &) = delete;
        EditorGraphProfile &operator=(const EditorGraphProfile &) = delete;

        [[nodiscard]] const Renderer::GraphCompileOptions &Options() const noexcept { return m_Options; }

    private:
        // 基础 2D 四项、cube 及深度数组各一项、额外 2D 两项和 storage 2DArray 四项.
        std::array<Renderer::GraphTextureSupport, 12> m_Textures{};
        std::array<RHI::ResourceStates, 3> m_States{RHI::ResourceStates::Common, RHI::ResourceStates::ShaderResource};
        Renderer::GraphCompileOptions m_Options;
    };

    /// @brief 同槽 Ready 后才可复用的 CPU 载荷, 图存储和最近提交记账.
    /// @note Platform 拥有 Event 和目标代. 本值只保存 Runtime 的存储与预算, 不自行判定 GPU 完成.
    /// Scene、各路 View 和 Forward 独立于两份 arena, 图重试继续使用冻结记录.
    struct FrameSlot
    {
        Renderer::SceneFrameStorage sceneFrame; ///< 在输入边界合并索引, 仅在同槽 Ready 后更新快照.
        Renderer::ViewWork mainView; ///< 本轮主 View 的独立索引区间.
        Renderer::ViewWork offscreenView; ///< 只在本轮有附加请求时准备.
        Renderer::ForwardFrame forward; ///< 版本引用保留到该槽实际完成, 上传字节借用止于录制结束.
        Core::FrameArena arena;
        Core::FrameArena graphArena; ///< 图析构回退不影响 UI/上传载荷.
        Editor::FrameSlotUiFrameBindings bindings;
        std::uint64_t lastSubmission = 0; ///< 本次取得的接受票据, 0 表示本轮尚未提交; 非零不代表颜色有效.
        std::size_t diagnosticUploadBytes = 0; ///< 同槽 Ready 后归还诊断上传额度.
    };

    /// @brief 调用方借出本轮的 View 及其像素请求, 以 ShaderRead 导出这路显示色.
    /// @note RenderFrame 在返回前写回 reason 和 submission. 非零票据关联刚发布给该 View 的颜色.
    /// 零票据时仍可用 LastAcceptedOutput 取得该 View 先前的颜色, 首次接受前该值为空.
    struct OffscreenViewRequest
    {
        Renderer::View &view;
        std::optional<Renderer::ViewportRequest> layout;
        Renderer::ViewSettingReason reason = Renderer::ViewSettingReason::LayoutPending;
        std::uint64_t submission{};
    };

    /// @brief 当前输入的事务已结算, 将增量同步合并到所有槽后确认消费.
    /// @pre editor 已装配活动 Scene, slots 包含本 Runtime 的全部帧槽.
    void CollectSceneChanges(Editor::EditorSession &editor, std::span<FrameSlot, FrameSlotCount> slots) noexcept;

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

    /// @brief 在录制外推进一次原生预算, 有本帧计划时在复核源修订后发布, 再交付只读统计.
    /// @param hasFramePlan 本轮已取得呈现槽并收齐可规划 View 的需求时为真. 槽忙或暂停时为假, 只推进预热.
    /// @pre 允许发布时, 各路尚未冻结本帧的程序表. 最后一次原生准备返回后再轮询已到达的文件通知.
    void AdvanceFrameShaders(Platform::DeviceHost &host, Renderer::ForwardRenderer &forward,
        FrameStatistics &statistics, Shader::SourceReload *reload, std::optional<Shader::ReloadResult> startupFailure,
        bool hasFramePlan) noexcept;

    /// @brief 从封存 UI 推进一次取得, 执行前最多建立一张独立安全图.
    /// @pre slot 对应 presentation 的安全取得, commands 来自同一宿主且无活动录制.
    /// editorFrame 来自本轮尚未失效的 Advance, editor 已装配 Scene 文档.
    /// remainingDiagnosticBytes 已扣除其它槽的在途额度.
    /// @param editorSeconds 产生 editorFrame 的 Advance 实测秒数. 与本次 UI 准备时间共同归入 UI 阶段.
    /// @param statistics 此宿主的查询及统计 owner. 只在 View 接受本次场景输出后发布场景统计.
    /// @param graphPool 此设备会话共用的池. 本轮的正常候选和独立安全 UI 各按自己的编译计划准入.
    /// @param offscreenRequests 当前配置额外接受一路, 身份须与主 View 不同. 请求对象和 View 借用至返回.
    /// @param reload 可选开发源会话, 在原生准备前和候选切换前读取通知和当前请求.
    /// @return 正常或可恢复状态; 文本错误表示必须退出, 未证明完成的工作仍归 RHI/宿主收拢.
    /// @note 准备值和图的 CPU 借用在返回前结束, 已发 GPU 引用由原物理包保活.
    [[nodiscard]] std::variant<FrameStatus, Core::BoundedText> RenderFrame(
        Platform::DeviceHost &host, Editor::EditorSession &editor, Renderer::View &view,
        Renderer::ForwardRenderer &forward,
        const Editor::EditorFrame &editorFrame, const Platform::PresentationFrame &presentation, FrameSlot &slot,
        std::size_t remainingDiagnosticBytes, RHI::CommandList &commands, Core::Diagnostics &diagnostics,
        FrameStatistics &statistics, Renderer::GraphResourcePool &graphPool, double editorSeconds,
        std::span<OffscreenViewRequest> offscreenRequests = {}, Shader::SourceReload *reload = nullptr,
        std::optional<Shader::ReloadResult> startupShaderFailure = {});
}
