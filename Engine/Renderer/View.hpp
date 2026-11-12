#pragma once

/// @file
/// @brief 持有主 View 的请求比例及最后接受的诊断颜色.
/// Editor 提供离屏像素请求, Runtime 按本次合成的提交回执发布有效比例和尺寸.

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

    /// @brief 无单位渲染比例的闭区间. 初始请求使用上界.
    inline constexpr float MinRenderScale = 0.5f;
    inline constexpr float MaxRenderScale = 1.0f;

    /// @brief 离屏目标的物理像素宽高. Editor 从本帧布局请求生成有效范围.
    struct ViewExtent
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        bool operator==(const ViewExtent &) const = default;
    };

    /// @brief CPU UI 封存后按当前比例换算的一次离屏像素请求.
    /// @note View 用请求代确认换算依据, 颜色纹理另有自己的资源代.
    struct ViewportRequest
    {
        ViewId view;
        ViewExtent extent; ///< 逻辑区域存在但无法换算时为零, 由 View 报告失败.
        std::uint64_t requestRevision = 0; ///< 用于计算像素范围的比例请求代.
    };

    /// @brief 最后一次被接受的诊断比例和实际像素范围.
    struct EffectiveViewport
    {
        float scale;
        ViewExtent extent;
    };

    /// @brief 待确认或失败的具体原因. None 表示当前请求已获接受.
    enum class ViewSettingReason : std::uint8_t
    {
        None,
        LayoutPending,
        SlotPending,
        SubmissionPending,
        UploadPending,
        InvalidExtent,
        TextureSizeLimit,
        DiagnosticBudget
    };

    /// @brief 同一 View 的请求和确认快照, 可独立用于面板显示.
    /// effective 在首个匹配的诊断输出获接受前为空. 等待或拒绝时保留上次确认值.
    struct ViewSettings
    {
        ViewId view;
        float requestedScale = MaxRenderScale;
        std::optional<EffectiveViewport> effective;
        ViewSettingReason reason = ViewSettingReason::LayoutPending;
        std::uint64_t requestRevision = 0; ///< 随已接受的比例修改递增, 初始为零.
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

    /// @brief 本轮的诊断颜色或缺失原因, 同时保存准备时的比例及请求代.
    /// @note 拥有颜色句柄, 候选像素借用本槽的 arena, 在同一槽使用期内录制.
    class NoSceneOutputs final
    {
    public:
        /// @brief 只在本次准备中借用的颜色代, 返回空时由 UI 决定占位.
        [[nodiscard]] const ViewOutput *Diagnostic() const noexcept { return m_Output ? &*m_Output : nullptr; }
        /// @brief 候选颜色匹配的比例请求代. 仅在 Diagnostic 非空时使用.
        [[nodiscard]] std::uint64_t RequestRevision() const noexcept { return m_RequestRevision; }
        /// @brief 本次新代需要的紧密像素字节数. 复用已有代或暂时无候选时为 0.
        [[nodiscard]] std::size_t UploadBytes() const noexcept { return m_UploadBytes; }
        /// @brief 诊断上传剩余额度不足, 可在后续布局请求中再次准备.
        [[nodiscard]] bool UploadPending() const noexcept { return m_UploadPending; }
        /// @brief 本轮可见占位所需的范围拒绝原因, 无拒绝时为 None.
        [[nodiscard]] ViewSettingReason Rejection() const noexcept { return m_Rejection; }

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
        ViewSettingReason m_Rejection = ViewSettingReason::None;
        float m_Scale = MaxRenderScale;
        std::uint64_t m_RequestRevision = 0;
    };

    /// @brief 计算紧密 RGBA8 像素字节数. 零尺寸或超出 32 MiB 诊断预算时返回 0.
    [[nodiscard]] std::size_t DiagnosticPixelBytes(ViewExtent extent) noexcept;

    /// @brief 在 Device 的 owner 线程接收比例请求, 准备并发布诊断输出.
    /// @note 同尺寸可复用已接受的颜色. GPU 在途访问由原提交的引用保活.
    class View final
    {
    public:
        /// @brief 为当前宿主创建带新逻辑身份的 View, 设备由宿主持有.
        /// @return 身份代耗尽时返回空. 视图输出随首次有效请求准备.
        [[nodiscard]] static std::optional<View> Create(RHI::Device &device, std::uint32_t maxTextureSize) noexcept;
        View(const View &) = delete;
        View &operator=(const View &) = delete;
        View(View &&) noexcept = default;
        View &operator=(View &&) = delete;

        [[nodiscard]] ViewId Id() const noexcept { return m_Id; }
        /// @brief 按值读取比例请求, 最后确认的有效设置及当前原因.
        [[nodiscard]] ViewSettings Settings() const noexcept;

        /// @brief 在帧边界接收此 View 的有限比例. 同值请求直接成功.
        /// @return 身份或比例无效, 或请求代耗尽时返回原因, 保留已有状态.
        [[nodiscard]] std::optional<Core::BoundedText> RequestRenderScale(ViewId view, float scale) noexcept;

        /// @brief 本轮没有提交候选颜色时记录真实等待原因, 已确认的当前请求保持有效.
        void Defer(ViewSettingReason reason) noexcept;

        /// @brief 复用已接受的同尺寸颜色, 或为本次有效布局建立新代候选.
        /// @note 请求缺失或 View 与比例请求代失配时, 返回无颜色并等待当前布局.
        /// @param remainingUploadBytes 本轮可用的诊断像素额度, 由 Runtime 按槽记账.
        /// @return 无有效范围或上传额度不足时返回无颜色的结果. 纹理或身份创建失败时返回原因.
        [[nodiscard]] std::variant<NoSceneOutputs, Core::BoundedText>
            Prepare(const std::optional<ViewportRequest> &request, std::size_t remainingUploadBytes);
        /// @brief 接受本轮匹配的颜色, 发布对应比例和像素尺寸. 复用的同尺寸颜色也可确认新比例.
        /// @pre outputs 属于本轮准备, 且被已接受的完整合成实际采样.
        /// @note 已过期的 View 或请求代保持原状态. 旧代的 GPU 访问由原 RHI 包持有.
        void Publish(NoSceneOutputs &&outputs) noexcept;

    private:
        View(RHI::Device &device, ViewId id, std::uint32_t maxTextureSize) noexcept
            : m_Device(device), m_Id(id), m_MaxTextureSize(maxTextureSize) {}

        RHI::Device &m_Device;
        ViewId m_Id;
        std::uint32_t m_MaxTextureSize;
        std::uint64_t m_LastGeneration = 0;
        std::uint64_t m_RequestRevision = 0;
        float m_RequestedScale = MaxRenderScale;
        float m_EffectiveScale = MaxRenderScale;
        ViewSettingReason m_Reason = ViewSettingReason::LayoutPending;
        std::optional<ViewOutput> m_Diagnostic;
    };
}
