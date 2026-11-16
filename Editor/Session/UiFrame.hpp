#pragma once

/// @file
/// @brief 编辑器封存帧的拥有型准备值, 当前槽的图绑定及同次执行的接受回执.
/// Runtime 负责目标初始化, 图编译执行和宿主交接, UI 只声明依赖并确认自己的请求.

#include <filesystem>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <Core/Error.hpp>
#include <Core/Identity.hpp>
#include <Renderer/View.hpp>

namespace Physara::Editor
{
    namespace Detail
    {
        struct UiFrameData;
        class UiRenderer;
    }

    /// @brief 设备由宿主持有, 包路径只在创建期间借用, mainView 为稳定逻辑身份.
    struct UiRendererDesc
    {
        RHI::Device &device;
        const std::filesystem::path &packagePath;
        Renderer::ViewId mainView;
    };

    struct UiTextureTag;
    /// @brief 普通图消费匹配候选或带原因的占位. 安全图只读同一 View 的旧输出.
    enum class UiFrameMode : std::uint8_t
    {
        Normal, ///< 解析当前请求匹配的候选, 完整接受后可确认 View.
        SafeUi ///< 失败主图未 Open 时的独立图, 只读旧输出且没有 View 发布资格.
    };

    /// @brief 最近失败当前处于哪种恢复状态, Normal 表示后继正常帧已经恢复.
    enum class FrameRecovery : std::uint8_t
    {
        Normal,
        SafeUi,
        Cancelled,
        Stopped
    };

    /// @brief 会话保留的一份有界失败快照, 不借用帧 arena, GraphId 或原生资源.
    /// 主错与一个附加摘要分别拥有正文; recovery 随后继成功/取消更新, 不保存逐帧队列.
    struct FrameDiagnostic
    {
        Core::BoundedText primary;
        std::optional<Core::BoundedText> additional;
        FrameRecovery recovery = FrameRecovery::Cancelled;
    };

    /// @brief 本会话的槽位和不复用的代, 与当前图的资源身份及原生纹理名无关.
    using UiTextureId = Core::Identity<UiTextureTag>;
    using UiTextureResult = std::variant<UiTextureId, Core::BoundedText>;

    /// @brief 外部 owner 提供的有效内容和进出状态, 范围用于当帧规范导入和采样.
    struct UiTextureImport
    {
        Renderer::ImportState states;
        Renderer::TextureRange range;
    };

    /// @brief 当前图中的规范纹理版本. 多个 token 可以引用同一个版本及范围.
    struct UiFrameTexture
    {
        Renderer::TextureId texture;
        Renderer::TextureRange range;
    };

    /// @brief 当前槽使用期的解析表. Reset 只可在结束候选图或同槽真实完成后调用.
    /// @note 表项不进入跨帧 registry. 旧图的索引即使相同, 也不能用于新图代.
    struct FrameSlotUiFrameBindings
    {
        /// @brief token 到当前生产版本的映射. 空 token 专用于共享诊断占位.
        struct Token
        {
            UiTextureId token;
            UiFrameTexture value;
        };
        std::uint64_t graphGeneration = 0;
        std::vector<Token> tokens = std::vector<Token>(0); ///< 包含上传候选及已解析身份, 不代表全部会被采样.
        std::vector<UiFrameTexture> sampled = std::vector<UiFrameTexture>(0); ///< 普通可见 draw 的去重读取, 顺序即绑定索引.

        /// @brief 清除本图的身份和读取, 保留容量供后续安全使用期复用.
        void Reset() noexcept;
        /// @brief 按准备时的图代读取去重采样. 代或索引不匹配时返回空.
        /// @note 返回指针借用到表被修改或销毁, 不持有物理资源.
        [[nodiscard]] const UiFrameTexture *Find(std::uint64_t generation, std::size_t index) const noexcept;
    };

    /// @brief 同图执行和关闭提交后的回执, 计划和输出只在 Finish 中借用.
    /// @pre commands, plan, outputs 和 ticket 来自本准备值的同一次图执行和提交.
    /// @note executedWithoutErrors 包含 Close, Execute 诊断及宿主 colorValid, 前缀票据不足以确认.
    struct UiSubmission
    {
        RHI::CommandList &commands;
        const Renderer::CompiledGraphPlan &plan;
        const Renderer::GraphExecutionOutputList *outputs = nullptr;
        std::uint64_t ticket = 0;
        bool executedWithoutErrors = false;
    };

    /// @brief 持有本轮请求和绘制值, 借用帧槽已复制的载荷, 最多向一张图声明一次.
    /// 普通外部驻留引用保留准备时的原纹理和导入事实, 后续注销不影响本准备值构图.
    /// @note owner 线程调用. 下一次 Advance 或会话销毁取消尚未确认的准备值.
    /// 候选图须在准备值及载荷失效前结束执行, GPU 已发依赖由 RHI 包继续保活.
    class PreparedUiFrame final
    {
    public:
        /// @brief 析构或移动赋值替换活动准备值时, 取消尚未确认的请求和临时身份.
        ~PreparedUiFrame();
        PreparedUiFrame(PreparedUiFrame &&) noexcept;
        PreparedUiFrame &operator=(PreparedUiFrame &&) noexcept;
        PreparedUiFrame(const PreparedUiFrame &) = delete;
        PreparedUiFrame &operator=(const PreparedUiFrame &) = delete;

        /// @brief 至少一条普通可见 draw 引用本轮 View 候选, 用于接受提交后的 View 发布判断.
        [[nodiscard]] bool UsesViewOutput() const noexcept;
        /// @brief 按本轮请求数预留图对象上界, 载荷 arena 由 Prepare 单独预留.
        [[nodiscard]] std::size_t GraphStorageBytes() const noexcept;
        /// @brief 在场景缓冲上传前提交 UI 的纹理生产者. 如需声明后续 UI, 使用同图及相同绑定表.
        /// @pre 本准备值尚未声明纹理, target 属于当前图且 bindings 为空; 同一准备值只声明一次.
        /// @note 提前声明只建立生产者和图代, 不确认上传; 失败后放弃整图并取消准备值.
        [[nodiscard]] Renderer::GraphFailure DeclareTextureUploads(Renderer::RenderGraph &graph,
            Renderer::TextureId target, FrameSlotUiFrameBindings &bindings, std::vector<Renderer::GraphUpload> &uploads);
        /// @brief 追加纹理/几何上传和 Raster, 返回原窗口对象的合成版本.
        /// @pre bindings 为本次图的空表或已通过 DeclareTextureUploads 追加的表, diagnostics 借用至执行结束.
        /// color 是本图已初始化的目标版本; 若 UsesViewOutput 为 true, diagnostic 须提供同图颜色版本.
        /// @note 只形成 CPU 声明. 回调仍借用本准备值的投影和回执地址, Finish/Cancel 前须结束图执行.
        /// 失败不回滚已追加的声明, 调用方须放弃候选图并取消准备值, 重试使用新准备值.
        [[nodiscard]] Renderer::GraphResult<Renderer::TextureId> BuildGraph(
            Renderer::RenderGraph &graph, Renderer::TextureId color, Renderer::TextureId diagnostic,
            FrameSlotUiFrameBindings &bindings, std::vector<Renderer::GraphUpload> &uploads,
            Renderer::GraphExecutionDiagnostics diagnostics);
        /// @brief 按存活终结 Pass 和成功导出逐项确认请求, 同时消费准备值.
        /// @return 回执匹配且完整颜色被接受时为 true, 不表示 GPU 已完成.
        /// @note 不匹配回执也会取消并消费活动准备值. 已被 Advance 取消的值直接返回 false.
        [[nodiscard]] bool Finish(const UiSubmission &submission) noexcept;
        /// @brief 撤销本轮临时 token 并释放准备值持有的引用, 不撤销图已导入或 RHI 已发的引用.
        /// @note 已发命令仍由调用者关闭并提交或受控退出.
        void Cancel() noexcept;

    private:
        friend class Detail::UiRenderer;
        explicit PreparedUiFrame(std::unique_ptr<Detail::UiFrameData> data) noexcept;
        std::unique_ptr<Detail::UiFrameData> m_Data;
    };

    /// @brief 空值表示没有封存 UI, 其余为可构图的准备值或有界准备失败.
    using PrepareUiResult = std::variant<std::monostate, PreparedUiFrame, Core::BoundedText>;
}
