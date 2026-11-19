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

    /// @brief 当前配置的程序包路径, 由 Runtime 按实际 exe 定位. 物理变体按需加载.
    struct ForwardPackagePaths
    {
        std::filesystem::path color;
        std::filesystem::path depth;
        std::filesystem::path display;
        std::filesystem::path physical;
        std::filesystem::path ibl;
        std::filesystem::path sky;
        std::filesystem::path grading;
        std::filesystem::path fxaa;
        std::filesystem::path scale;
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

    /// @brief 显示前工作色及深度共用的来源. layout 是渲染栅格, camera.exposure 是本帧的曝光快照.
    /// mode 区分实际生产模式. PhysicalLighting 的受光值和天空为预曝光 linear Rec.709/D65.
    /// DirectLighting 仅包含预曝光直接光与自发光. 诊断模式输出对应通道, Unlit 仍是标准基色.
    struct ForwardWorkingSource
    {
        ViewId view;
        Scene::SceneId scene;
        std::uint64_t contentRevision;
        PreviewMode mode;
        CameraSnapshot camera;
        ViewImageLayout layout;
    };

    /// @brief 本图的工作颜色, 深度及显示颜色. 读回和显示处理消费同一工作色生产者.
    /// 显示色已执行视口映射, 三个图版本均只在本图有效.
    struct ForwardOutput
    {
        TextureId workingColor;
        TextureId depth;
        TextureId displayColor;
        ForwardWorkingSource source;

        /// @brief 是否有本候选的上传保留在同图计划中, 供准入失败时判定能否移除场景分支.
        [[nodiscard]] bool HasLiveUploads(const CompiledGraphPlan &plan) const noexcept;
        /// @brief 核对显示版本的图代以及所有候选上传 Pass 的活性. 执行成功与提交票据由 Runtime 核对.
        [[nodiscard]] bool HasCompleteUploads(const CompiledGraphPlan &plan) const noexcept;

    private:
        friend class ForwardFrame;
        std::vector<PassId> m_Uploads;
    };

    namespace Detail
    {
        struct ForwardFrameData;
        struct ForwardDevice;
    }

    /// @brief 准备值的实际绘制项和提交给本 View 的灯表行.
    struct ForwardWorkCounts
    {
        std::size_t draws{};
        std::size_t lights{};
    };

    /// @brief 设备级 Forward 驻留缓存的当前索引数量, 含等待淘汰的缓存代.
    struct ForwardResidencyCounts
    {
        std::size_t geometry{};
        std::size_t textures{};
        std::size_t materials{};
        std::size_t environment{};
    };

    /// @brief 槽拥有的 Draw 和上传快照. Draw 及其 RHI 引用持续保活至槽取得安全复用证明.
    class ForwardFrame final
    {
    public:
        ForwardFrame() noexcept;
        ~ForwardFrame();
        ForwardFrame(const ForwardFrame &) = delete;
        ForwardFrame &operator=(const ForwardFrame &) = delete;

        /// @brief 同槽安全取得时释放上轮 Draw 和活动绑定, 保留可重用的 View 行及该槽的普通灯缓冲.
        void Reset() noexcept;
        /// @brief 借用本轮被跳过项的有界摘要, 下次 Reset/Prepare 后失效; 空值表示无局部降级.
        [[nodiscard]] const std::optional<Core::BoundedText> &Degradation() const noexcept;
        /// @brief 本候选实际使用的环境角色, 与当前 PreparedView 一起交给 View.
        [[nodiscard]] const std::optional<RenderedEnvironment> &Environment() const noexcept;
        /// @brief 图回调对象所需的 arena 预留量, 不含帧拥有的 Draw 容器, 像素或 GPU 存储.
        [[nodiscard]] std::size_t GraphStorageBytes() const noexcept;
        /// @brief 返回本槽已准备的几何绘制项和活动灯表行数. 同一项供深度及颜色 Pass 消费时仍只计一次.
        [[nodiscard]] ForwardWorkCounts WorkCounts() const noexcept;

        /// @brief 声明实际上传及 Forward 输出, 在 UI 缓冲上传之前调用.
        /// @pre 此槽已有成功的准备值, 且保持至图执行结束. 纯声明失败后放弃候选图.
        /// UI 纹理上传须先声明, 以保持设备准入的纹理在前, 缓冲在后顺序.
        /// @note 返回的显示色由实际消费者读取并导出, 未消费分支可裁剪; 声明不确认驻留内容.
        [[nodiscard]] GraphResult<ForwardOutput> Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads) const;
        /// @brief 完整图实际执行并获得接受票据后, 确认本次上传可供后续录制使用.
        /// @pre output 来自此准备值, plan 是接受的同图计划. 安全 UI 或前缀票据不得调用.
        /// @note 本入口只核对图代和上传活性, 完整接受资格由 Runtime 保证; 此处不等待 GPU.
        void Confirm(const ForwardOutput &output, const CompiledGraphPlan &plan) noexcept;
        /// @brief 结束本帧场景和显示上传的 CPU 借用. 此后不能再用本批次声明或确认上传.
        /// @pre 图已结束录制或已放弃. 正常发布的候选先 Confirm; 放弃的候选跳过确认. GPU 引用仍由槽和提交包保活.
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
        /// @brief 在设备 owner 线程读取四张缓存索引当时的条目数. 首次准备前各项为零.
        [[nodiscard]] ForwardResidencyCounts ResidencyCounts() const noexcept;

    private:
        RHI::Device &m_Device;
        ForwardPackagePaths m_Packages;
        std::uint32_t m_MaxTextureSize;
        std::uint32_t m_MaxAttachmentSize;
        std::unique_ptr<Detail::ForwardDevice> m_Resources;
    };
}
