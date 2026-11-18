#pragma once

/// @file
/// @brief CPU 资产请求、拥有型版本及图片采样语义. 主线程掌管请求与发布.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <variant>

#include <Core/ContentHash.hpp>
#include <Core/Error.hpp>
#include <Core/Identity.hpp>

namespace Physara::Asset
{
    struct AssetTag;
    using AssetId = Core::Identity<AssetTag>;

    /// @brief 单个 manager 内成功准入的请求序号, 0 表示没有请求; 不等于已发布内容版本.
    struct RequestId
    {
        std::uint64_t value = 0;
        bool operator==(const RequestId &) const = default;
    };

    /// @brief 进程内已发布的内容版本, 0 表示还没有有效内容.
    struct ContentRevision
    {
        std::uint64_t value = 0;
        bool operator==(const ContentRevision &) const = default;
    };

    /// @brief 最近一次请求的状态. CpuReady 只确认 CPU 内容, 失败或取消不清空已发布版本.
    enum class AssetState
    {
        Unloaded,
        Loading,
        CpuReady,
        Failed,
        Cancelled
    };

    enum class AssetError
    {
        InvalidConfiguration,
        InvalidPath,
        CannotOpen,
        IncompleteRead,
        SourceByteLimit,
        HashFailed,
        UnsupportedEncoding,
        InvalidImage,
        ImageByteLimit,
        PixelCapacity,
        AssetCapacity,
        QueueFull,
        Stopped,
        InvalidAsset,
        IdentityExhausted,
        OutOfMemory,
        WorkerUnavailable,
        UnexpectedFailure,
        InvalidModel,
        UnsupportedModel,
        ModelByteLimit
    };

    /// @brief 跨 worker/owner 传递的自持失败值, 文本截断不影响可判断的错误类别.
    struct AssetFailure
    {
        AssetError reason;
        Core::BoundedText description;
    };

    enum class ImageEncoding
    {
        Png,
        Jpeg
    };

    /// @brief 单层 RGBA8, 从顶部起按行无填充存储. 像素不可修改, 所有引用计入同一保有额度.
    struct Rgba8Image
    {
        ImageEncoding encoding;
        std::uint32_t width;
        std::uint32_t height;
        bool hasSourceAlpha;
        std::size_t byteCount;
        std::shared_ptr<const unsigned char[]> storage;

        static constexpr std::uint32_t MipLevels = 1;
        [[nodiscard]] std::size_t RowBytes() const noexcept { return static_cast<std::size_t>(width) * 4; }
        [[nodiscard]] std::span<const unsigned char> Pixels() const noexcept { return {storage.get(), byteCount}; }
    };

    /// @brief 成功发布的完整记录. source 是项目资源根相对路径, 指纹覆盖实际解码的源字节.
    /// @note manager 仅交付 const 共享引用; 后续替换或 manager 析构不使已持有的版本失效.
    struct ImageRecord
    {
        AssetId asset;
        ContentRevision revision;
        std::filesystem::path source;
        Core::Sha256Digest sourceFingerprint;
        Rgba8Image image;
        std::size_t sourceByteCount{}; ///< 实际源文件字节数, 供文档核对读取上界, 与解码后的 image.byteCount 分开.
    };

    using ImageVersion = std::shared_ptr<const ImageRecord>;

    enum class ImageUse
    {
        BaseColor,
        EditorColor,
        Emissive,
        Normal,
        OcclusionRoughnessMetallic,
        LinearData
    };

    enum class ImageDomain
    {
        Srgb,
        LinearData,
        LinearRadiance
    };

    enum class AlphaMeaning
    {
        StraightCoverage,
        Opaque,
        Ignored,
        Data
    };

    /// @brief 采样解释独立于像素. image 的拥有引用固定了 AssetId 和 ContentRevision.
    /// use 是用途身份的一部分; DescribeImage 只附加解释, 不转换字节或生成 GPU 资源.
    struct ImageSample
    {
        ImageVersion image;
        ImageUse use;
        ImageDomain domain;
        AlphaMeaning alpha;
    };

    /// @brief 描述现有版本的用途, 共享存储并保留原通道. 无有效图像或用途时返回空.
    [[nodiscard]] std::optional<ImageSample> DescribeImage(ImageVersion image, ImageUse use) noexcept;

    enum class EnvironmentEncoding
    {
        RadianceHdr,
        OpenExr
    };

    enum class RadianceCalibration
    {
        Relative,
        Absolute
    };

    /// @brief 描述环境输入的编码和标定方式. 解码的 EXR 以文件单位白是否存在确定标定方式.
    struct EnvironmentSource
    {
        EnvironmentEncoding encoding;
        RadianceCalibration calibration = RadianceCalibration::Relative;
        static constexpr ImageDomain Domain = ImageDomain::LinearRadiance;
        static constexpr AlphaMeaning Alpha = AlphaMeaning::Ignored;
    };

    /// @brief 等距柱状环境, 顶部起行的线性 RGB32F, alpha 不参与辐亮度解释.
    struct EnvironmentImage
    {
        std::uint32_t width;
        std::uint32_t height;
        std::shared_ptr<const float[]> storage;
        EnvironmentSource source;
        std::optional<float> whiteLuminance; ///< 文件声明的 cd/m² 单位白. 空值表示相对亮度.

        [[nodiscard]] std::span<const float> Pixels() const noexcept
        {
            return {storage.get(), std::size_t(width) * height * 3};
        }
    };

    /// @brief 不可变环境源版本, 源身份和标定来自同次受控读快照.
    struct EnvironmentRecord
    {
        AssetId asset;
        ContentRevision revision;
        std::filesystem::path source;
        Core::Sha256Digest sourceFingerprint;
        EnvironmentImage image;
        std::size_t sourceByteCount;
    };

    using EnvironmentVersion = std::shared_ptr<const EnvironmentRecord>;

    /// @brief 指向一次准入的目标和请求身份. 合并的调用者共享此值和取消效果.
    struct AssetRequest
    {
        AssetId asset;
        RequestId request;
        bool operator==(const AssetRequest &) const = default;
    };

    /// @brief 最近请求的状态和最后发布版本独立. 刷新失败时两者可同时存在.
    /// @note Status 返回值快照, 后续状态变化需重新查询; published 持有相应版本和像素额度.
    struct AssetStatus
    {
        AssetState state = AssetState::Unloaded;
        RequestId request;
        ImageVersion published;
        std::optional<AssetFailure> failure;
    };

    struct ModelRecord;
    using ModelVersion = std::shared_ptr<const ModelRecord>;

    /// @brief 有效源 scene 下标. 空值表示使用文件默认, 未声明默认时仅自动选择唯一 scene.
    struct ModelImportOptions
    {
        std::optional<std::uint32_t> scene;
        bool operator==(const ModelImportOptions &) const = default;
    };

    /// @brief 模型请求的当前状态与最后发布版本. ModelAsset.hpp 定义记录内容.
    /// 状态是调用时的快照, 失败/取消可与旧 published 共存; 持有 published 同时保有其几何和像素额度.
    struct ModelStatus
    {
        AssetState state = AssetState::Unloaded;
        RequestId request;
        ModelVersion published;
        std::optional<AssetFailure> failure;
    };

    /// @brief 最新环境请求及最后成功的 CPU 版本. 失败或取消时已有版本仍可固定.
    struct EnvironmentStatus
    {
        AssetState state = AssetState::Unloaded;
        RequestId request;
        EnvironmentVersion published;
        std::optional<AssetFailure> failure;
    };

    /// @brief 每个服务的工作及存储额度. 模型依赖同样受单文件上限约束, 内嵌图像同样使用像素额度.
    struct ImportLimits
    {
        std::uint32_t assetCount = 256; ///< 服务寿命内登记的不同来源数; 当前不提供卸载表项入口.
        std::uint32_t pendingJobs = 8; ///< 排队、运行与完成待取共同占用此额度.
        std::uint32_t workerCount = 2;
        std::size_t sourceBytes = 32 * 1024 * 1024;
        std::size_t imageBytes = 64 * 1024 * 1024;
        std::size_t retainedPixels = 256 * 1024 * 1024; ///< 包含候选和仍被旧版本读者持有的像素.
        std::size_t modelInputBytes = 128 * 1024 * 1024; ///< 每个模型及其实际依赖的累计输入.
        std::size_t modelParseBytes = 48 * 1024 * 1024; ///< JSON 树和 C 元数据 arena 各自上限.
        std::size_t modelGeometryBytes = 128 * 1024 * 1024; ///< 单模型输出与几何准备的上界.
        std::size_t retainedGeometry = 256 * 1024 * 1024; ///< 候选及所有保有模型的几何.
    };

    /// @brief 主线程独占请求表和完成发布. worker 只拥有 CPU 输入和结果.
    /// @note 所有管理接口及析构均由同一主线程串行调用, 只读版本可独立跨线程持有.
    /// 创建后首个实际需求启动 worker. 析构停止接单和发布并收拢所有作业.
    class AssetManager final
    {
    public:
        using Creation = std::variant<std::unique_ptr<AssetManager>, AssetFailure>;
        using Submission = std::variant<AssetRequest, AssetFailure>;

        /// @brief 创建 CPU 资产 owner. root 是绝对的本地盘符目录, 文件系统在实际读取时访问.
        /// @return 配置拒绝或内部预留失败时返回 AssetFailure, 成功时尚未启动线程或读取资产.
        [[nodiscard]] static Creation Create(std::filesystem::path root, ImportLimits limits = {});

        AssetManager(const AssetManager &) = delete;
        AssetManager &operator=(const AssetManager &) = delete;
        ~AssetManager();

        /// @brief 请求 Assets/Textures 或 Assets/Editor 下的图片. 同源同代合并进行中或已就绪的请求.
        /// @return 共享结果的请求身份, 或未准入的有界错误. 失败或取消后可显式再次请求.
        /// @note 队列拒绝不建立 Loading 状态. 外部文件修改须先 InvalidateSource, 不隐式轮询磁盘.
        /// 环境由独立的 RequestEnvironment 解释为线性辐亮度.
        [[nodiscard]] Submission RequestImage(const std::filesystem::path &source);

        /// @brief 在同一队列中请求 Assets/Models 下的静态模型. 来源与 scene 选项共同标识请求.
        /// @return 成功准入或合并后的请求身份; 拒绝不建立 Loading 状态, 不清除旧版本.
        /// @note 与图片共用 pendingJobs. 默认选择和显式 scene 是不同逻辑 key, 即使最终选择相同 scene.
        /// 外部依赖变化后也须对该 AssetId 调用 InvalidateSource 再请求, 不隐式轮询文件.
        [[nodiscard]] Submission RequestModel(const std::filesystem::path &source, ModelImportOptions options = {});

        /// @brief 从 Assets/Environments 请求线性 EXR. 相同源代复用请求, 刷新须先调用 InvalidateSource.
        /// @note StatusEnvironment 的 CpuReady 表示可固定 RGB 和标定. 视口另外按所需用途准备环境.
        [[nodiscard]] Submission RequestEnvironment(const std::filesystem::path &source);

        /// @brief 显式推进已知源代, 撤销旧请求的发布资格. 调用者可另行请求新代.
        /// @return 空值表示失效已生效; 后继请求暂满不会恢复旧资格, 最后有效版本仍可取得.
        /// 当前 Loading 变为 Cancelled, 其它状态变为 Unloaded; 不改变 ContentRevision.
        [[nodiscard]] std::optional<AssetFailure> InvalidateSource(AssetId asset) noexcept;

        /// @brief 取消 identity 仍匹配的请求. 合并请求的观察者共享这一次取消.
        /// 已发布、过期或未知请求不变; 未发布载荷由作业交接或关闭释放, 此调用不等待 worker.
        void Cancel(AssetRequest request) noexcept;

        /// @brief 返回自持状态. 未登记身份返回 Unloaded, 已持有的旧版本独立有效.
        [[nodiscard]] AssetStatus Status(AssetId asset) const noexcept;

        /// @brief 查询模型状态. 未登记或其它种类身份返回 Unloaded.
        [[nodiscard]] ModelStatus StatusModel(AssetId asset) const noexcept;
        /// @brief 查询环境请求及版本. 未登记或其它种类身份返回 Unloaded.
        [[nodiscard]] EnvironmentStatus StatusEnvironment(AssetId asset) const noexcept;

        /// @brief 主线程在继续的 CPU 输入轮次调用. 仅处理已经交接的结果.
        /// @return 本次实际发布的新内容版本数, 不计失败、取消、过期与相同内容.
        /// @note 单批最多处理 pendingJobs 个完成槽, 不等待仍在执行的任务.
        std::size_t PublishCompleted() noexcept;

        /// @brief 停止新请求和发布, 取消并 join 作业, 释放尚未发布的载荷. 可重复调用.
        /// @note 等待已进入的 I/O/解码调用返回; Status 和既有只读版本仍可查询或持有.
        void Shutdown() noexcept;

    private:
        struct Impl;
        explicit AssetManager(std::unique_ptr<Impl> state) noexcept;
        std::unique_ptr<Impl> m_State;
    };
}
