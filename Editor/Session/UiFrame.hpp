#pragma once

/// @file
/// @brief 编辑器离屏 UI 的公开准备值和提交回执.
/// 会话借用宿主 Device, 准备值持有录制依赖, 调用者负责目标底色和命令列表的提交.

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <variant>

#include <Core/Error.hpp>
#include <Core/Identity.hpp>
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
    };

    struct UiTextureTag;
    /// @brief 本会话的纹理槽位和代. 槽位复用后, 旧身份无法解析到新条目.
    using UiTextureId = Core::Identity<UiTextureTag>;
    using UiTextureResult = std::variant<UiTextureId, Core::BoundedText>;

    /// @brief 调用者为完整录制提供的关闭和提交回执.
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

    /// @brief 独占本帧几何, 上传字节及录制引用. 准备值最多录制一次.
    /// @note 限宿主 owner 线程. 借用的纹理请求由 Finish 或 Cancel 结束.
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

        /// @brief 在调用者已开放的 Graphics 列表中调用上传和绘制命令.
        /// @pre 列表使用 Automatic 资源状态跟踪, 且由会话的 Device 创建.
        /// 目标及驻留纹理由调用者提供有效的跟踪前态.
        /// @return 空值表示 UI 录制区间无同步 Error/Fatal. 返回有界错误时取消准备值.
        /// @note 失败时已发前缀仍归调用者关闭并提交或受控收拢. Finish 只消费完整录制的回执.
        [[nodiscard]] std::optional<Core::BoundedText> Record(RHI::CommandList &commands);

        /// @brief 消费准备值, 根据本次回执确认纹理. 取消确认时保留待处理请求.
        /// @return 准备值仍有效且列表, 诊断和票据通过检查时为 true. GPU 完成由 Device 跟踪.
        [[nodiscard]] bool Finish(const UiSubmission &submission) noexcept;

        /// @brief 放弃本次准备值. 已发命令的提交或收拢仍由调用者负责.
        void Cancel() noexcept;

    private:
        friend class Detail::UiRenderer;
        explicit PreparedUiFrame(std::unique_ptr<Detail::UiFrameData> data) noexcept;
        std::unique_ptr<Detail::UiFrameData> m_Data;
    };

    /// @brief 空值表示本轮没有 UI 工作, 其余结果为可录制的准备值或有界原因.
    using PrepareUiResult = std::variant<std::monostate, PreparedUiFrame, Core::BoundedText>;
}
