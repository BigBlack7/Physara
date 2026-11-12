#pragma once

/// @file
/// @brief 私有 ImGui 纹理表和帧内上传协议.
/// 将稳定身份解析为采样绑定, 将像素及复制区域交给准备值. RHI 提交包保活已发工作.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <imgui.h>

#include <RHI.hpp>

#include "Session/UiFrame.hpp"

namespace Physara::Platform
{
    class RhiErrorScope;
}

namespace Physara::Editor::Detail
{
    /// @brief 将会话分配的逻辑身份编码为固定 ImGui 的 64 位值, 高位是代, 低位是槽位.
    /// @pre 代为会话注册分配的非零 32 位值.
    [[nodiscard]] ImTextureID EncodeUiTexture(UiTextureId id) noexcept;

    /// @brief 一次管理纹理写入, CPU 像素为紧密排列的 RGBA8 字节.
    /// @note request 只用于本帧内确认或取消. 更新时先写 staging 的完整 mip, 再复制到 target.
    struct UiTextureUpload
    {
        ImTextureData *request = nullptr;
        UiTextureId created; ///< 首次创建的临时身份. 更新已有纹理时无效.
        RHI::TextureHandle target;
        RHI::TextureHandle staging;
        RHI::TextureSlice region; ///< target 的 mip 0 像素区域, 以左上为原点.
        std::span<const std::byte> pixels; ///< 借用本槽紧密排列的 RGBA8 区段, 在准备阶段填满.
        /// @brief 已校验区域的紧密写入大小, 准入及像素分配使用同一范围.
        [[nodiscard]] std::size_t ByteCount() const noexcept;
    };

    /// @brief 一次准备的上传计划. bytes 为 RGBA8 载荷, 用于 arena 预留.
    /// 首次解析无效图像时登记本批的占位上传需求.
    struct UiTextureUploads
    {
        // 使用 count 构造, 使 MSVC Debug 的代理分配失败可传播给准备入口.
        UiTextureUploads() : items(0)
        {
        }

        std::vector<UiTextureUpload> items;
        std::size_t bytes = 0;
        bool usesPlaceholder = false;
    };

    /// @brief 当帧采样失败的原因, 同时用于占位诊断和视口画面.
    enum class UiFallback : std::uint8_t
    {
        None,
        InvalidIdentity,
        MissingOutput,
        UploadPending
    };

    /// @brief 在本帧准备期间借用一个 View 输出. 解析后的 binding 由普通 draw 持有.
    /// viewportDraw 指向封存的图像命令. viewportFallback 同时决定该命令的占位绑定和可见标签.
    /// uploadPending 供同一逻辑 View 的其它引用解释颜色代为何缺失.
    struct UiFrameBindings
    {
        const Renderer::ViewOutput *output = nullptr;
        bool uploadPending = false;
        RHI::BindingSetHandle viewBinding;
        const ImDrawCmd *viewportDraw = nullptr;
        UiFallback viewportFallback = UiFallback::None;
    };

    /// @brief 拥有本会话驻留纹理和有界绑定缓存, 协议请求的像素复制到本次帧槽.
    /// @note 限宿主 owner 线程. 管理纹理的关联必须在 ImGui context 销毁前解除.
    class UiTextures final
    {
    public:
        static constexpr int MaxTextureSize = 2048;

        explicit UiTextures(RHI::Device &device) noexcept : m_Device(device)
        {
        }

        /// @brief 准备共享布局, 投影缓冲, sampler 和占位绑定. 部分初始化的资源由 owner 回收.
        [[nodiscard]] std::optional<Core::BoundedText> Initialize(const RHI::BindingLayoutDesc &layout);
        [[nodiscard]] RHI::BindingLayout *GetLayout() const noexcept { return m_Layout.Get(); }
        [[nodiscard]] RHI::Buffer *GetProjectionBuffer() const noexcept { return m_Projection.Get(); }

        /// @brief 为已初始化的外部 SRV 建立持有型条目. 注销释放注册表引用, 已准备绑定仍然有效.
        [[nodiscard]] UiTextureResult RegisterResident(RHI::Texture &texture);
        [[nodiscard]] bool UnregisterResident(UiTextureId id) noexcept;
        /// @brief 仅登记 View 和角色. 实际颜色代在当帧解析, 不进入跨帧表.
        [[nodiscard]] UiTextureResult RegisterView(Renderer::ViewId view);

        /// @brief 结束请求销毁的身份, 核对本轮各像素范围和累计预算, 供帧槽统一预留.
        [[nodiscard]] std::optional<Core::BoundedText> PlanUploads(const ImDrawData &drawData,
                                                                  UiTextureUploads &uploads);
        /// @brief 在已预留的本槽区段复制像素并建立纹理. 失败时由准备值取消临时身份.
        [[nodiscard]] std::optional<Core::BoundedText> PrepareUploads(UiTextureUploads &uploads,
                                                                     Core::FrameArena &arena);
        /// @brief 校验 ImGui 源与表项关联. 临时创建也可匹配, 上传状态由回执确认.
        [[nodiscard]] bool HasManagedTexture(const ImTextureData &source) noexcept;

        /// @brief 普通 draw 对诊断色或共享占位的采样需求. 托管纹理写入由请求计划单独列出.
        enum class SampleUse : std::uint8_t
        {
            Other,
            Diagnostic,
            Placeholder
        };

        /// @brief 取得可见命令所需的诊断或占位写入. 已规划的托管创建视为本帧可用.
        [[nodiscard]] SampleUse MeasureSample(const ImDrawCmd &draw, const UiTextureUploads &uploads,
                                              const UiFrameBindings &bindings) noexcept;

        /// @brief 解析驻留或本帧 View 绑定. 普通无效引用使用占位并向 Device 报告原因.
        /// @return 缺少必需字体或有效颜色的绑定创建失败时返回空.
        [[nodiscard]] RHI::BindingSetHandle Resolve(const ImDrawCmd &draw, UiTextureUploads &uploads,
                                                    UiFrameBindings &bindings);

        /// @brief 本批采样需要首次写入占位时返回紧密字节数, 其余情况返回 0.
        [[nodiscard]] std::size_t PlaceholderUploadBytes(bool used) const noexcept;
        /// @brief 判断是否有纹理请求写入或首次使用的占位上传.
        [[nodiscard]] bool HasUploads(const UiTextureUploads &uploads) const noexcept;
        /// @brief 在本轮 draw 之前上传占位和管理纹理. 写入成功后才从区域暂存复制.
        /// @pre errors 来自该列表所属宿主, 在本批首条命令前取样且仍有效.
        /// @return 收到同步错误时停止余下的命令并返回 false, 已发前缀保留在列表中.
        [[nodiscard]] bool Record(RHI::CommandList &commands, const UiTextureUploads &uploads,
                                  const Platform::RhiErrorScope &errors) const;

        /// @brief 只有有效执行和提交均成立时确认上传, 否则撤销本次创建的临时身份.
        void Finish(const UiTextureUploads &uploads, bool accepted) noexcept;
        /// @brief 在 ImGui context 仍存活且没有借用请求时解除所有管理纹理的关联.
        void Detach() noexcept;

    private:
        /// @brief 一个注册槽位. 驻留和托管条目持有实际绑定, 逻辑 View 条目只记录身份及角色.
        /// @note managed 借用 ImGui 维护的请求元数据, managedBytes 记录该纹理的驻留字节.
        struct Entry
        {
            UiTextureId id;
            RHI::TextureHandle texture;
            RHI::BindingSetHandle binding;
            ImTextureData *managed = nullptr; ///< 由固定 ImGui 的 create/destroy 协议维持元数据寿命.
            std::size_t managedBytes = 0;
            Renderer::ViewId view; ///< 有效时为逻辑 View 条目.
            Renderer::ViewOutputRole role = Renderer::ViewOutputRole::DiagnosticColor;
        };

        /// @brief 现有条目或占位原因. 两者均空时为待创建的托管纹理或缺失的必需字体.
        struct Sample
        {
            const Entry *entry = nullptr;
            UiFallback fallback = UiFallback::None;
        };

        [[nodiscard]] Entry *Find(UiTextureId id) noexcept;
        /// @brief 在预检和绑定阶段应用同一套身份、输出匹配及占位规则.
        [[nodiscard]] Sample FindSample(const ImDrawCmd &draw, const UiFrameBindings &bindings,
                                         std::span<const UiTextureUpload> planned = {}) noexcept;
        [[nodiscard]] RHI::BindingSetHandle Bind(RHI::Texture *texture) const;
        [[nodiscard]] RHI::BindingSetHandle Placeholder(UiTextureUploads &uploads, UiFallback reason) const;
        [[nodiscard]] UiTextureResult Register(Entry entry);
        void Erase(Entry &entry) noexcept;
        void Destroy(ImTextureData &source) noexcept;
        [[nodiscard]] std::optional<Core::BoundedText> PlanUpload(ImTextureData &source, UiTextureUploads &uploads,
                                                                 std::size_t &remainingResident);

        RHI::Device &m_Device;
        RHI::BindingLayoutHandle m_Layout;
        RHI::BufferHandle m_Projection;
        RHI::SamplerHandle m_Sampler;
        RHI::TextureHandle m_Placeholder;
        RHI::BindingSetHandle m_PlaceholderBinding;
        std::array<Entry, 64> m_Entries;
        std::size_t m_ManagedBytes = 0;
        bool m_PlaceholderReady = false;
    };
}
