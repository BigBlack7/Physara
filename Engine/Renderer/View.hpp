#pragma once

/// @file
/// @brief 持有主 View 的持久化相机与比例请求, 以及最后接受的诊断颜色.
/// Editor 提供离屏像素请求, Runtime 按本次合成的提交回执发布有效比例和尺寸.
/// 相机实体有效性由 Scene/View 的组合调用者确认, 本阶段不计算场景相机投影.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <variant>

#include <Core/Error.hpp>
#include <Core/Identity.hpp>
#include <RHI.hpp>
#include <Scene/Scene.hpp>
#include "FrameGraph.hpp"

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

    /// @brief 持久化主 View 请求. 相机引用由会话根据候选 Scene 解释.
    struct ViewDocumentRequests
    {
        Scene::Pose cameraPose;
        std::optional<Scene::EntityId> cameraEntityId;
        float renderScale = MaxRenderScale;

        bool operator==(const ViewDocumentRequests &) const = default;
    };

    /// @brief 已校验的 View 文档更新. 只可由创建它的 View 在同一请求代发布.
    class ViewDocumentUpdate final
    {
    public:
        ViewDocumentUpdate(ViewDocumentUpdate &&) noexcept = default;
        ViewDocumentUpdate &operator=(ViewDocumentUpdate &&) noexcept = default;

    private:
        friend class View;
        ViewDocumentUpdate(ViewId view, std::uint64_t revision, ViewDocumentRequests value) noexcept;

        ViewId m_View;
        std::uint64_t m_Revision;
        ViewDocumentRequests m_Value;
    };

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

    /// @brief 当前仅提供静态诊断颜色, 不表示真实场景输出或可推进的时域历史.
    enum class ViewOutputRole : std::uint8_t
    {
        DiagnosticColor
    };

    /// @brief 诊断颜色代及其身份. generation 随新候选分配, ViewId 保持稳定.
    /// @note 候选代的 texture 为空. 图执行并获得有效提交后才交付持久句柄.
    struct ViewOutput
    {
        ViewId view;
        ViewOutputRole role = ViewOutputRole::DiagnosticColor;
        std::uint64_t generation = 0;
        ViewExtent extent;
        RHI::TextureHandle texture;
    };

    /// @brief 本轮诊断候选或安全 UI 的只读旧输出, 两者都不代表真实场景历史.
    /// @note 普通候选保存准备时的比例及请求代. 复用或旧输出拥有颜色句柄, 新像素借用本槽 arena.
    /// 不含跨帧 GraphId. IsRetained 区分旧输出快照, 不能仅凭纹理非空判断本轮发布资格.
    class NoSceneOutputs final
    {
    public:
        /// @brief 只在本次准备中借用的颜色代, 返回空时由 UI 决定占位.
        [[nodiscard]] const ViewOutput *Diagnostic() const noexcept { return m_Output ? &*m_Output : nullptr; }
        /// @brief 只读旧输出快照不代表本次请求候选, 只能供安全 UI 读取, 不能再次 Publish.
        [[nodiscard]] bool IsRetained() const noexcept { return m_Retained; }
        /// @brief 普通候选颜色匹配的比例请求代. 仅在 Diagnostic 非空且非 IsRetained 时使用.
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
        /// @brief 将候选上传或已接受颜色的导入加入当前图, 不创建原生纹理.
        /// @pre Diagnostic 非空, PrepareUpload 已成功, 像素区段借用至图执行结束.
        /// @note 返回本图版本, 由实际消费者决定活性与持久导出. 声明本身不发布有效设置.
        [[nodiscard]] GraphResult<TextureId> Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads) const;

    private:
        friend class View;
        std::optional<ViewOutput> m_Output;
        std::size_t m_UploadBytes = 0;
        std::span<const std::byte> m_Pixels;
        bool m_UploadPending = false;
        bool m_Retained = false;
        ViewSettingReason m_Rejection = ViewSettingReason::None;
        float m_Scale = MaxRenderScale;
        std::uint64_t m_RequestRevision = 0;
    };

    /// @brief 计算紧密 RGBA8 像素字节数. 零尺寸或超出 32 MiB 诊断预算时返回 0.
    [[nodiscard]] std::size_t DiagnosticPixelBytes(ViewExtent extent) noexcept;

    /// @brief 在 owner 线程管理持久化请求及独立 revision, 准备并发布诊断输出.
    /// @note 同尺寸可复用已接受的颜色. GPU 在途访问由原提交的引用保活.
    class View final
    {
    public:
        /// @brief 按宿主能力建立逻辑 View, 纹理创建由本轮图执行负责.
        /// @return 身份代耗尽时返回空. 视图输出随首次有效请求准备.
        [[nodiscard]] static std::optional<View> Create(std::uint32_t maxTextureSize) noexcept;
        View(const View &) = delete;
        View &operator=(const View &) = delete;
        View(View &&) noexcept = default;
        View &operator=(View &&) = delete;

        [[nodiscard]] ViewId Id() const noexcept { return m_Id; }
        /// @brief 按值读取比例请求, 最后确认的有效设置及当前原因.
        [[nodiscard]] ViewSettings Settings() const noexcept;
        /// @brief 当前持久请求的借用, 不得跨请求更新保留其内部字符串引用.
        [[nodiscard]] const ViewDocumentRequests &DocumentRequests() const noexcept { return m_Document; }
        [[nodiscard]] std::uint64_t DocumentRevision() const noexcept { return m_DocumentRevision; }
        [[nodiscard]] std::uint64_t CameraRevision() const noexcept { return m_CameraRevision; }

        /// @brief 只校验值域. 具体相机实体归 Scene, 由组合调用者校验.
        [[nodiscard]] static std::optional<Core::BoundedText>
            ValidateDocumentRequests(const ViewDocumentRequests &requests) noexcept;
        /// @brief 预检全部请求和版号, 允许调用者与 Scene 事务一起发布.
        /// @note 返回值拥有候选请求, 准备本身不改写 owner; 有效输出仍由图形提交确认.
        [[nodiscard]] std::variant<ViewDocumentUpdate, Core::BoundedText>
            PrepareDocumentUpdate(ViewId view, ViewDocumentRequests requests) const noexcept;
        /// @pre 来自此 View, 且准备后本 View 的持久请求未变更. 不影响已采纳的诊断资源.
        void PublishDocumentUpdate(ViewDocumentUpdate &&update) noexcept;
        /// @brief 取得最后接受颜色的拥有型只读快照, 缺失时仍明确表示安全 UI 输入.
        /// @note 不产生新代或上传, 不改变请求及有效设置. 旧图身份不进入快照.
        /// 快照可跨后续 View 换代持有; 当前诊断 profile 不原地覆写已接受颜色.
        [[nodiscard]] NoSceneOutputs LastAcceptedOutput() const noexcept;

        /// @brief 在帧边界接收此 View 的有限比例. 同值请求直接成功.
        /// @return 身份或比例无效, 或请求代耗尽时返回原因, 保留已有状态.
        [[nodiscard]] std::optional<Core::BoundedText> RequestRenderScale(ViewId view, float scale) noexcept;

        /// @brief 本轮没有提交候选颜色时记录真实等待原因, 已确认的当前请求保持有效.
        void Defer(ViewSettingReason reason) noexcept;

        /// @brief 复用已接受的同尺寸颜色, 或为本次有效布局建立新代候选.
        /// @note 请求缺失或 View 与比例请求代失配时, 返回无颜色并等待当前布局.
        /// @param remainingUploadBytes 本轮可用的诊断像素额度, 由 Runtime 按槽记账.
        /// @return 无有效范围或上传额度不足时返回无颜色的结果. 身份代耗尽时返回原因.
        [[nodiscard]] std::variant<NoSceneOutputs, Core::BoundedText>
            Prepare(const std::optional<ViewportRequest> &request, std::size_t remainingUploadBytes);
        /// @brief 接受本轮匹配的颜色, 发布对应比例和像素尺寸. 复用的同尺寸颜色也可确认新比例.
        /// @pre outputs 属于本轮准备, 且被已接受的完整合成实际采样.
        /// acceptedTexture 是同次成功图导出的该版本, 本入口不自行验证提交票据或等待 GPU.
        /// @note 已过期的 View 或请求代保持原状态. 旧代的 GPU 访问由原 RHI 包持有.
        void Publish(NoSceneOutputs &&outputs, const RHI::TextureHandle &acceptedTexture) noexcept;

    private:
        View(ViewId id, std::uint32_t maxTextureSize) noexcept
            : m_Id(id), m_MaxTextureSize(maxTextureSize) {}
        void ApplyRenderScale(float scale) noexcept;

        ViewId m_Id;
        std::uint32_t m_MaxTextureSize;
        std::uint64_t m_LastGeneration = 0;
        std::uint64_t m_RequestRevision = 0;
        ViewDocumentRequests m_Document;
        std::uint64_t m_DocumentRevision = 0;
        std::uint64_t m_CameraRevision = 0;
        float m_EffectiveScale = MaxRenderScale;
        ViewSettingReason m_Reason = ViewSettingReason::LayoutPending;
        std::optional<ViewOutput> m_Diagnostic;
    };
}
