#pragma once

/// @file
/// @brief 当前主 View 的逻辑身份和诊断颜色输出. 请求和输出均为独立于窗口的像素范围.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

#include <Core/Error.hpp>
#include <Core/Identity.hpp>
#include <RHI.hpp>

namespace Physara::Core
{
    class FrameArena;
}

namespace Physara::Renderer
{
    struct ViewIdentityTag;
    /// @brief 跨布局和纹理换代保持稳定的逻辑 View 身份.
    using ViewId = Core::Identity<ViewIdentityTag>;

    /// @brief 离屏目标的物理像素宽高. Editor 从本帧布局请求生成有效范围.
    struct ViewExtent
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        bool operator==(const ViewExtent &) const = default;
    };

    /// @brief 一个布局边界发布的离屏像素请求.
    struct ViewportRequest
    {
        ViewId view;
        ViewExtent extent;
    };

    /// @brief Runtime 对诊断纹理像素记账的总字节上限, 同时约束单张诊断图的尺寸.
    inline constexpr std::size_t DiagnosticUploadBudget = 32 * 1024 * 1024;

    enum class ViewOutputRole : std::uint8_t
    {
        DiagnosticColor
    };

    /// @brief 本帧可绑定的颜色句柄及其身份. generation 随尺寸换代, ViewId 保持稳定.
    /// @note 候选代须先在同一 Graphics 列表写入像素, 后续 UI 命令才能采样.
    struct ViewOutput
    {
        ViewId view;
        ViewOutputRole role = ViewOutputRole::DiagnosticColor;
        std::uint64_t generation = 0;
        ViewExtent extent;
        RHI::TextureHandle texture;
    };

    /// @brief 无场景渲染结果时携带可选的诊断颜色及该次写入所需的数据.
    /// @note 候选像素借用本槽的 arena, 调用方在同一槽使用期内完成录制.
    class NoSceneOutputs final
    {
    public:
        /// @brief 只在本次准备中借用的颜色代, 返回空时由 UI 决定占位.
        [[nodiscard]] const ViewOutput *Diagnostic() const noexcept { return m_Output ? &*m_Output : nullptr; }
        /// @brief 本次新代需要的紧密像素字节数. 复用已有代或暂时无候选时为 0.
        [[nodiscard]] std::size_t UploadBytes() const noexcept { return m_UploadBytes; }
        /// @brief 诊断上传剩余额度不足, 可在后续布局请求中再次准备.
        [[nodiscard]] bool UploadPending() const noexcept { return m_UploadPending; }

        /// @brief 在已预留的本槽空间中生成新代像素. 复用已有代时直接成功.
        /// @return 可供本轮录制时为 true. 空间不足时返回 false, 由调用方结束本次准备.
        [[nodiscard]] bool PrepareUpload(Core::FrameArena &arena) noexcept;
        /// @brief 在同一列表的首个采样前写入候选代. 上游负责检查 RHI 执行区间.
        /// @pre PrepareUpload 已成功, 帧槽字节仍有效, 列表来自同一 Device.
        void RecordUpload(RHI::CommandList &commands) const;

    private:
        friend class View;
        std::optional<ViewOutput> m_Output;
        std::size_t m_UploadBytes = 0;
        std::span<const std::byte> m_Pixels;
        bool m_UploadPending = false;
    };

    /// @brief 计算紧密 RGBA8 像素字节数. 零尺寸或超出 32 MiB 诊断预算时返回 0.
    [[nodiscard]] std::size_t DiagnosticPixelBytes(ViewExtent extent) noexcept;

    /// @brief 在 Device 的 owner 线程管理一个 View 的已接受诊断颜色代.
    class View final
    {
    public:
        /// @brief 为当前宿主创建带新逻辑身份的 View, 设备由宿主持有.
        /// @return 身份代耗尽时返回空. 视图输出随首次有效请求准备.
        [[nodiscard]] static std::optional<View> Create(RHI::Device &device) noexcept;
        View(const View &) = delete;
        View &operator=(const View &) = delete;
        View(View &&) noexcept = default;
        View &operator=(View &&) = delete;

        [[nodiscard]] ViewId Id() const noexcept { return m_Id; }

        /// @brief 复用已接受的同尺寸颜色, 或为本次有效布局建立新代候选.
        /// @pre 非空请求属于此 View, 布局方已检查设备支持的像素范围.
        /// @return 无有效范围或上传额度不足时返回无颜色的结果. 纹理或身份创建失败时返回原因.
        [[nodiscard]] std::variant<NoSceneOutputs, Core::BoundedText>
            Prepare(const std::optional<ViewportRequest> &request, std::size_t remainingUploadBytes);
        /// @brief 发布该次完整录制实际上传的诊断代, 供下一帧同尺寸请求复用.
        /// @note 旧代的 GPU 访问由原 RHI 包持有. 场景历史在本阶段没有提交输入.
        /// @pre 本次工作已被 Device 接受, outputs 属于本 View 和对应录制.
        void Publish(NoSceneOutputs &&outputs) noexcept;

    private:
        View(RHI::Device &device, ViewId id) noexcept : m_Device(device), m_Id(id) {}

        RHI::Device &m_Device;
        ViewId m_Id;
        std::uint64_t m_LastGeneration = 0;
        std::optional<ViewOutput> m_Diagnostic;
    };
}
