#pragma once

/// @file
/// @brief 私有 ImGui 纹理表和帧内上传协议.
/// 将稳定身份解析为采样绑定, 将像素及复制区域交给准备值. RHI 提交包保活已发工作.

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
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
        std::unique_ptr<std::uint8_t[]> pixels; ///< 区域内紧密排列的 RGBA8 字节, 由准备阶段填满.
    };

    /// @brief 一次准备的上传项. 首次解析无效图像时登记本批占位需求和诊断.
    struct UiTextureUploads
    {
        // 使用 count 构造, 使 MSVC Debug 的代理分配失败可传播给准备入口.
        UiTextureUploads() : items(0)
        {
        }

        std::vector<UiTextureUpload> items;
        bool usesPlaceholder = false;
    };

    /// @brief 拥有本会话驻留纹理和有界绑定缓存, 协议请求的像素由准备值单独拥有.
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

        /// @brief 先结束请求销毁的逻辑身份, 再为创建和局部更新复制所需像素.
        /// @note 失败时由准备值取消本轮上传及临时创建, 已处理的逻辑销毁继续生效.
        [[nodiscard]] std::optional<Core::BoundedText> Prepare(const ImDrawData &drawData, UiTextureUploads &uploads);

        /// @brief 解析本帧使用的绑定, 普通无效引用得到占位, 缺少必需字体时返回空.
        [[nodiscard]] RHI::BindingSetHandle Resolve(const ImTextureRef &ref, UiTextureUploads &uploads);

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
        /// @brief 纹理槽位当前的代和绑定. managed 仅标记由 ImGui 协议管理的条目.
        struct Entry
        {
            UiTextureId id;
            RHI::TextureHandle texture;
            RHI::BindingSetHandle binding;
            ImTextureData *managed = nullptr; ///< 由固定 ImGui 的 create/destroy 协议维持元数据寿命.
            std::size_t managedBytes = 0;
        };

        [[nodiscard]] Entry *Find(UiTextureId id) noexcept;
        [[nodiscard]] RHI::BindingSetHandle Bind(RHI::Texture *texture) const;
        [[nodiscard]] UiTextureResult Register(RHI::TextureHandle texture, ImTextureData *managed,
                                              std::size_t managedBytes);
        void Erase(Entry &entry) noexcept;
        void Destroy(ImTextureData &source) noexcept;
        [[nodiscard]] std::optional<Core::BoundedText> PrepareUpload(ImTextureData &source,
                                                                    UiTextureUploads &uploads,
                                                                    std::size_t &remainingBytes);

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
