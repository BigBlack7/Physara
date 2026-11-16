#pragma once

/// @file
/// @brief 正式 Forward 准备和图声明入口. 设备缓存与完成驱动的帧槽准备分开保有资源.

#include <filesystem>
#include <memory>

#include "../Prepare/ViewPreparation.hpp"

namespace Physara::Renderer
{
    inline constexpr std::size_t ForwardAttachmentBudget = 128 * 1024 * 1024;
    inline constexpr std::size_t ForwardBufferLimit = 64 * 1024 * 1024;

    /// @brief 当前配置的三个程序包路径, 由 Runtime 定位; 首次准备场景时才读取包.
    struct ForwardPackagePaths
    {
        std::filesystem::path color;
        std::filesystem::path depth;
        std::filesystem::path display;
    };

    /// @brief 当前路径按实际下游需求准备附加表面. 正常主视口由 ShadeOpaque 同时写颜色和深度.
    struct ViewResourceNeeds
    {
        bool earlyDepth = false;
    };

    /// @brief 未交付候选时的 View 状态及具体说明. Pending 可重试, 硬限制须改变输入后再准备.
    struct ForwardFailure
    {
        ViewSettingReason reason;
        Core::BoundedText description;
    };

    /// @brief 本图的工作颜色、深度及显示颜色. 三者均为本图精确版本, 工作色为线性预览域.
    struct ForwardOutput
    {
        TextureId workingColor;
        TextureId depth;
        TextureId displayColor;

        /// @brief 是否有本候选的上传保留在同图计划中, 供准入失败时判定能否移除场景分支.
        [[nodiscard]] bool HasLiveUploads(const CompiledGraphPlan &plan) const noexcept;

    private:
        friend class ForwardFrame;
        std::vector<PassId> m_Uploads;
    };

    namespace Detail
    {
        struct ForwardFrameData;
        struct ForwardDevice;
    }

    /// @brief 槽拥有的 Draw 和上传快照. Draw 及其 RHI 引用持续保活至槽取得安全复用证明.
    class ForwardFrame final
    {
    public:
        ForwardFrame() noexcept;
        ~ForwardFrame();
        ForwardFrame(const ForwardFrame &) = delete;
        ForwardFrame &operator=(const ForwardFrame &) = delete;

        /// @brief 同槽安全取得时结束上次准备和引用, 保留可复用的 CPU 存储.
        void Reset() noexcept;
        /// @brief 借用本轮被跳过项的有界摘要, 下次 Reset/Prepare 后失效; 空值表示无局部降级.
        [[nodiscard]] const std::optional<Core::BoundedText> &Degradation() const noexcept;
        /// @brief 图回调对象所需的 arena 预留量, 不含帧拥有的 Draw 容器, 像素或 GPU 存储.
        [[nodiscard]] std::size_t GraphStorageBytes() const noexcept;

        /// @brief 声明实际上传及 Forward 输出, 在 UI 缓冲上传之前调用.
        /// @pre 此槽已有成功的准备值, 且保持至图执行结束. 纯声明失败后放弃候选图.
        /// UI 纹理上传须先声明, 以保持设备准入的纹理在前, 缓冲在后顺序.
        /// @note 返回的显示色由实际消费者读取并导出, 未消费分支可裁剪; 声明不确认驻留内容.
        [[nodiscard]] GraphResult<ForwardOutput> Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads) const;
        /// @brief 完整图实际执行并获得接受票据后, 确认本次上传可供后续录制使用.
        /// @pre output 来自此准备值, plan 是接受的同图计划. 安全 UI 或前缀票据不得调用.
        /// @note 本入口只核对图代和上传活性, 完整接受资格由 Runtime 保证; 此处不等待 GPU.
        void Confirm(const ForwardOutput &output, const CompiledGraphPlan &plan) noexcept;
        /// @brief 录制结束后结束 CPU 字节借用. GPU 引用仍由槽和 RHI 提交包保活.
        void ReleasePayload() noexcept;

    private:
        friend class ForwardRenderer;
        std::unique_ptr<Detail::ForwardFrameData> m_Data;
    };

    /// @brief 设备 owner 线程准备驻留、绑定及原生 Pipeline, 仅消费帧内固定的资产版本.
    /// @note Device 由宿主持有. Renderer 和各槽须在原生 context 关闭前销毁.
    class ForwardRenderer final
    {
    public:
        ForwardRenderer(RHI::Device &device, ForwardPackagePaths packages, std::uint32_t maxTextureSize,
                        std::uint32_t maxAttachmentSize) noexcept;
        ~ForwardRenderer();
        ForwardRenderer(const ForwardRenderer &) = delete;
        ForwardRenderer &operator=(const ForwardRenderer &) = delete;

        /// @brief 用已有的静态工作集准备当前模式. 空可见集允许背景输出.
        /// @pre view 已准备, frame 已由安全取得的槽 Reset. 失败值可用于 View 占位及重试.
        /// @note 成功后帧独立持有 Draw/上传, 不再借用 PreparedView; 部分缓存可在失败后留下未接受代.
        [[nodiscard]] std::optional<ForwardFailure> Prepare(ForwardFrame &frame, const PreparedView &view,
            PreviewMode mode, ViewResourceNeeds needs = {});

    private:
        RHI::Device &m_Device;
        ForwardPackagePaths m_Packages;
        std::uint32_t m_MaxTextureSize;
        std::uint32_t m_MaxAttachmentSize;
        std::unique_ptr<Detail::ForwardDevice> m_Resources;
    };
}
