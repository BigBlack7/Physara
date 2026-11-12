#pragma once

/// @file
/// @brief 编辑器离屏 UI 的公开准备值和提交回执.
/// 会话借用宿主 Device. 准备值持有录制依赖并借用帧槽字节, 调用者负责初始化底色和提交.
/// 提交回执用于确认本轮纹理协议, 诊断颜色的有效设置由调用者交给 View 发布.

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <variant>

#include <Core/Error.hpp>
#include <Core/Identity.hpp>
#include <Renderer/View.hpp>
#include <RHI.hpp>

namespace Physara::Editor
{
    namespace Detail
    {
        struct UiFrameData;
        class UiRenderer;
    }

    /// @brief RHI 会话装配输入. 设备须为宿主的就绪 Device, 包路径仅在创建期间借用.
    struct UiRendererDesc
    {
        RHI::Device &device;
        const std::filesystem::path &packagePath;
        Renderer::ViewId mainView; ///< 有效时启用产品的诊断视口.
    };

    struct UiTextureTag;
    /// @brief 本会话的纹理槽位和代. 槽位复用后, 旧身份无法解析到新条目.
    using UiTextureId = Core::Identity<UiTextureTag>;
    using UiTextureResult = std::variant<UiTextureId, Core::BoundedText>;

    /// @brief 调用者为完整录制提供的关闭和提交回执, 供确认纹理请求.
    /// @pre commands 为 Record 使用的列表, ticket 来自包含该次录制的 Execute, 提交失败时为 0.
    /// @note 调用者按实际命令区间填写 executedWithoutErrors, 包括 Close 和 Execute.
    /// 可用宿主的 CaptureRhiErrors 观察该区间的 Error/Fatal.
    /// 同一列表对象可跨录制复用, 调用者负责将票据和诊断关联到本次录制.
    struct UiSubmission
    {
        RHI::CommandList &commands;
        std::uint64_t ticket = 0;
        bool executedWithoutErrors = false;
    };

    /// @brief 持有录制引用, 借用帧槽的几何与上传字节. 准备值最多录制一次.
    /// @note 限宿主 owner 线程. 帧槽区段和纹理请求借用由 Finish 或 Cancel 结束.
    /// 下一次 Advance 或会话销毁会取消尚未确认的准备值, 外部句柄随之失效.
    class PreparedUiFrame final
    {
    public:
        /// @brief 析构或移动赋值替换准备值时, 取消原来尚未确认的工作.
        ~PreparedUiFrame();
        PreparedUiFrame(PreparedUiFrame &&) noexcept;
        PreparedUiFrame &operator=(PreparedUiFrame &&) noexcept;
        PreparedUiFrame(const PreparedUiFrame &) = delete;
        PreparedUiFrame &operator=(const PreparedUiFrame &) = delete;

        /// @brief 活动批次有普通 draw 解析到本帧诊断颜色时为 true, 供调用者安排本轮写入.
        /// @note 如候选代需要上传, 在同一列表调用 Record 前写入, 与准入顺序一致.
        /// 已接受的同尺寸诊断代可直接采样. 是否发布本次有效设置由完整提交回执决定.
        [[nodiscard]] bool UsesDiagnosticOutput() const noexcept;
        /// @brief 本轮以占位替代因上传容量暂缓的诊断输出, 调用者据此报告等待原因.
        [[nodiscard]] bool DiagnosticUploadPending() const noexcept;

        /// @brief 在调用者已开放的 Graphics 列表中调用上传和绘制命令.
        /// @pre 列表使用 Automatic 资源状态跟踪, 且由会话的 Device 创建.
        /// 列表的最小上传 chunk 与本次准备采用的大小一致, 中间没有其它录制占用预检容量.
        /// 目标及驻留纹理由调用者提供有效的跟踪前态. 合成前须初始化底色, 可在同一列表先清屏.
        /// @return 空值表示 UI 录制区间无同步 Error/Fatal. 返回有界错误时取消准备值.
        /// @note 失败时已发前缀仍归调用者关闭并提交或受控收拢. Finish 只消费完整录制的回执.
        [[nodiscard]] std::optional<Core::BoundedText> Record(RHI::CommandList &commands);

        /// @brief 消费准备值, 根据本次回执确认纹理. 取消确认时保留待处理请求.
        /// @return 准备值仍有效且列表, 诊断和票据通过检查时为 true. GPU 完成由 Device 跟踪.
        /// @note 活动准备值即使收到不匹配的回执也会结束, 请求留待下次准备.
        [[nodiscard]] bool Finish(const UiSubmission &submission) noexcept;

        /// @brief 放弃本次准备值. 已发命令的提交或收拢仍由调用者负责.
        void Cancel() noexcept;

    private:
        friend class Detail::UiRenderer;
        explicit PreparedUiFrame(std::unique_ptr<Detail::UiFrameData> data) noexcept;
        std::unique_ptr<Detail::UiFrameData> m_Data;
    };

    /// @brief 基础 UI 的上传写入等待设备回收. 没有开放列表, 后续帧可重试.
    enum class UiPrepareStatus : std::uint8_t
    {
        UploadPending
    };

    /// @brief 空值表示本轮没有 UI 工作, 其余结果为可录制的准备值, 可重试状态或有界原因.
    using PrepareUiResult = std::variant<std::monostate, PreparedUiFrame, UiPrepareStatus, Core::BoundedText>;
}
