#pragma once

/// @file
/// @brief 私有 UI 纹理注册表和逐请求上传协议. 图身份只保存在当前准备值及帧槽中.

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <imgui.h>
#include "Session/UiFrame.hpp"

namespace Physara::Editor::Detail
{
    /// @brief 按固定 ImGui 的 64 位 ImTextureID 协议编码槽位和代, 不编码 RHI 指针.
    [[nodiscard]] ImTextureID EncodeUiTexture(UiTextureId id) noexcept;

    /// @brief 请求元数据借用到确认, 紧密 RGBA8 像素借用当前帧槽, 终结 Pass 用于逐项确认.
    struct UiTextureUpload
    {
        ImTextureData *request = nullptr;
        UiTextureId created;
        RHI::TextureHandle target; ///< 更新保留原代, 创建在执行前为空.
        RHI::TextureSlice region;
        std::span<const std::byte> pixels;
        Renderer::GraphTextureWrite written;
        [[nodiscard]] std::size_t ByteCount() const noexcept;
    };

    /// @brief 同次准备的请求集合. bytes 只计托管像素, 占位使用静态像素并由存活计划单独记账.
    struct UiTextureUploads
    {
        std::vector<UiTextureUpload> items = std::vector<UiTextureUpload>(0);
        std::size_t bytes = 0;
        bool usesPlaceholder = false;
        Renderer::GraphTextureWrite placeholder;
    };

    /// @brief 视口解析状态及共享占位原因. RetainedOutput 只加标签, 仍采样已接受的旧图像.
    /// 相机, 可绘表面及可见灯表各有独立的占位原因. LightTable 拒绝的字段详情保存在帧诊断中.
    enum class UiFallback : std::uint8_t
    {
        None, InvalidIdentity, MissingOutput, UploadPending, InvalidExtent, TextureSizeLimit, DiagnosticBudget,
        CameraUnavailable, InvalidProjection, InvalidImageRect, InvalidSampling, ScenePreparationFailed,
        ScenePipelineUnavailable, SceneResidencyPending, SceneResidencyLimit, SceneAttachmentLimit,
        SceneUnsupported, SceneUploadLimit, RetainedOutput, FrameFailed, SceneLightTableRejected
    };

    /// @brief 成功准备的外部驻留引用. 持有原对象及导入事实, 注销只阻止后续准备的新解析.
    /// @note 不保存 GraphId 或 BindingSet. 构图导入后由图继续保活, 已发访问再沿 RHI 包退休.
    struct UiResidentSnapshot
    {
        UiTextureId token;
        RHI::TextureHandle texture;
        UiTextureImport imported;
    };

    /// @brief 仅在封存帧解析期间借用 View 显示色及主视口命令. 独立 Image 同样参与颜色消费.
    /// residents 独立持有外部引用, 解析成功后移交批次, 准备失败时随本值释放.
    struct UiFrameBindings
    {
        const Renderer::ViewOutput *output = nullptr;
        UiFallback missingView = UiFallback::MissingOutput; ///< 无本 View 输出时使用的占位原因.
        bool usesDiagnostic = false; ///< 沿用原字段名, 现在表示实际消费任一种 View 输出.
        const ImDrawCmd *viewportDraw = nullptr;
        UiFallback viewportFallback = UiFallback::None;
        std::vector<UiResidentSnapshot> residents = std::vector<UiResidentSnapshot>(0);
    };

    /// @brief 会话持久身份及已接受物理纹理. 准备失败撤销临时 token, GPU 引用由 RHI 持有.
    class UiTextures final
    {
    public:
        static constexpr int MaxTextureSize = 2048;
        explicit UiTextures(RHI::Device &device) noexcept : m_Device(device) {}
        /// @brief 建立共享布局, 投影缓冲和采样器. 部分失败的句柄由本对象析构收拢.
        [[nodiscard]] std::optional<Core::BoundedText> Initialize(const RHI::BindingLayoutDesc &layout);
        [[nodiscard]] RHI::BindingLayout *GetLayout() const noexcept { return m_Layout.Get(); }
        [[nodiscard]] RHI::Buffer *GetProjectionBuffer() const noexcept { return m_Projection.Get(); }
        [[nodiscard]] RHI::Sampler *GetSampler() const noexcept { return m_Sampler.Get(); }

        /// @brief 保存外部纹理强引用及 owner 提供的导入事实, 不在注册阶段创建采样绑定.
        /// @pre texture 属于本会话设备, imported 描述已定义的单 mip 颜色范围及真实进出状态.
        [[nodiscard]] UiTextureResult RegisterResident(RHI::Texture &texture, UiTextureImport imported);
        /// @brief 仅停止外部驻留项的新解析. 已准备快照继续保活, 托管请求和 View 项不能经此入口移除.
        [[nodiscard]] bool UnregisterResident(UiTextureId id) noexcept;
        /// @brief 为有效 View 登记稳定显示色角色, 测试图/场景切换不换 token, 不保存跨帧 GraphId.
        [[nodiscard]] UiTextureResult RegisterView(Renderer::ViewId view);
        /// @brief 校验全部请求并结束逻辑销毁, 未使用上传仍可在后续图中裁剪.
        [[nodiscard]] std::optional<Core::BoundedText> PlanUploads(const ImDrawData &drawData,
                                                                  UiTextureUploads &uploads);
        /// @brief 只复制像素并安装临时逻辑身份, 原生纹理留到图执行时创建.
        [[nodiscard]] std::optional<Core::BoundedText> PrepareUploads(UiTextureUploads &uploads,
                                                                     Core::FrameArena &arena);
        /// @brief 核对元数据与 token 是否仍关联, 不表示像素已上传或请求已获接受.
        [[nodiscard]] bool HasManagedTexture(const ImTextureData &source) noexcept;
        /// @brief 返回当前可用 token, 无效 token 表示共享占位, nullopt 表示必需字体缺失.
        /// 外部驻留项按 token 去重保存拥有型快照, 随成功准备值移交到批次.
        [[nodiscard]] std::optional<UiTextureId> Resolve(const ImDrawCmd &draw, UiTextureUploads &uploads,
                                                        UiFrameBindings &bindings);
        /// @brief 声明创建和局部更新, 将更新后的当前版本登记到帧槽映射.
        [[nodiscard]] Renderer::GraphFailure DeclareUploads(Renderer::RenderGraph &graph, UiTextureUploads &uploads,
            FrameSlotUiFrameBindings &bindings, std::vector<Renderer::GraphUpload> &writes);
        /// @brief 从规范导入的当前写版本解析 token. 不把 ImportTexture 返回的版本零当成最新内容.
        /// @param residents 本批准备时捕获的驻留快照, 构图不再要求这些 token 仍在 registry 中.
        [[nodiscard]] Renderer::GraphResult<UiFrameTexture> ResolveGraph(Renderer::RenderGraph &graph,
            UiTextureId token, Renderer::TextureId diagnostic, FrameSlotUiFrameBindings &bindings,
            std::span<const UiResidentSnapshot> residents);
        /// @brief 只确认同一成功执行中存活的终结写. 取消和裁剪均撤销尚未发布的 token.
        /// @pre 非空 submission 已由 PreparedUiFrame 核对整图成功, 非零票据及有效 outputs.
        /// @note 空回执只取消本轮协议借用, 不撤销已发 GPU 写入; 原请求像素仍由 ImGui 留存重试.
        void Finish(const UiTextureUploads &uploads, const UiSubmission *submission) noexcept;
        /// @brief 在 ImGui context 销毁前清除全部托管元数据关联, GPU 引用仍沿原提交退休.
        void Detach() noexcept;

    private:
        /// @brief 一个长期逻辑条目: 外部/托管项持有纹理, View 项只保存身份和输出角色.
        /// @note WantCreate 临时项已有 managed 关联及预算, 但 texture 要到有效提交后才填入.
        struct Entry
        {
            UiTextureId id;
            RHI::TextureHandle texture;
            UiTextureImport imported;
            ImTextureData *managed = nullptr;
            std::size_t managedBytes = 0;
            Renderer::ViewId view;
            Renderer::ViewOutputRole role = Renderer::ViewOutputRole::DisplayColor;
        };
        /// @brief 本次 CPU 解析的借用结果. 两字段均为空表示必需字体缺失, 由调用方拒绝整批.
        struct Sample
        {
            const Entry *entry = nullptr;
            UiFallback fallback = UiFallback::None;
        };

        [[nodiscard]] Entry *Find(UiTextureId id) noexcept;
        /// @brief 按命令身份选择真实采样或占位原因, 不在此创建 GPU 绑定或确认请求.
        [[nodiscard]] Sample FindSample(const ImDrawCmd &draw, const UiFrameBindings &bindings) noexcept;
        /// @brief 本批首次占位登记共享像素需求并报告 Warning, 不把缺失图像升级为整帧错误.
        void Placeholder(UiTextureUploads &uploads, UiFallback reason) const;
        /// @brief 从固定槽表分配不复用的代, 并接管条目强引用及托管字节记账.
        [[nodiscard]] UiTextureResult Register(Entry entry);
        /// @brief 仅解除 CPU 注册和预算. 已封存或已发引用继续由各自 owner 持有.
        void Erase(Entry &entry) noexcept;
        /// @brief 解除匹配的托管身份后回报 ImGui 销毁, 后续是否重建由其像素寿命决定.
        void Destroy(ImTextureData &source) noexcept;
        /// @brief 校验并记录一次完整创建或局部更新, 从本批剩余预算扣减, 不复制像素.
        [[nodiscard]] std::optional<Core::BoundedText> PlanUpload(ImTextureData &source, UiTextureUploads &uploads,
                                                                 std::size_t &remainingResident);

        RHI::Device &m_Device;
        RHI::BindingLayoutHandle m_Layout;
        RHI::BufferHandle m_Projection;
        RHI::SamplerHandle m_Sampler;
        RHI::TextureHandle m_Placeholder;
        std::array<Entry, 64> m_Entries;
        std::size_t m_ManagedBytes = 0;
    };
}
