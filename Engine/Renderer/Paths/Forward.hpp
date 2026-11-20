#pragma once

/// @file
/// @brief 正式 Forward 准备和图声明入口. 设备缓存与完成驱动的帧槽准备分开保有资源.

#include <array>
#include <filesystem>
#include <map>
#include <memory>

#include "../Prepare/ViewPreparation.hpp"

namespace Physara::Renderer
{
    inline constexpr std::size_t ForwardAttachmentBudget = 128 * 1024 * 1024;
    inline constexpr std::size_t ForwardBufferLimit = 64 * 1024 * 1024;
    /// @brief 本配置的工作集位置 0 供主视口, 位置 1 供显式离屏出口. View 身份随各自的请求传入.
    inline constexpr std::size_t ForwardViewCount = 2;

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
    };

    namespace Detail
    {
        struct Allocation;
        struct ForwardFrameStorage;
        struct ForwardDevice;
    }

    /// @brief 同图声明的各路输出及存储代的真实生产者. 借用 ForwardFrame 至当前候选结束.
    struct ForwardGraph
    {
        std::array<std::optional<ForwardOutput>, ForwardViewCount> views;

    private:
        friend class ForwardFrame;
        std::map<Detail::Allocation *, std::vector<PassId>> m_StorageWrites;
        PassId m_LutWrite;
        std::size_t m_LutView{};
    };

    /// @brief 准备值的实际绘制项和提交给本 View 的灯表行.
    struct ForwardWorkCounts
    {
        std::size_t draws{};
        std::size_t lights{};
    };

    /// @brief 真正执行的对象、材质和世界灯行编码, 包括未产出可绘项的准备工作.
    struct ForwardEncodingCounts
    {
        std::size_t objects{};
        std::size_t materials{};
        std::size_t lights{};
    };

    /// @brief 设备级 Forward 缓存快照. 几何已用字节包含标为待退休的区间, 最大空洞取单页的连续空闲长度.
    /// @note 材质请求和命中按设备累计, materialGeneration 在发布新材质行时推进.
    struct ForwardResidencyCounts
    {
        std::size_t geometry{};
        std::size_t textures{};
        std::size_t materials{};
        std::size_t environment{};
        std::size_t geometryPages{}, geometryPageBytes{}, geometryUsedBytes{}, geometryRetiringBytes{};
        std::size_t largestGeometryGap{}, materialPageBytes{}, textureBytes{}, environmentBytes{}, samplerEntries{};
        std::uint64_t materialRequests{}, materialHits{}, materialGeneration{};
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
        [[nodiscard]] const std::optional<Core::BoundedText> &Degradation(std::size_t index = 0) const noexcept;
        /// @brief 本候选实际使用的环境角色, 与当前 PreparedView 一起交给 View.
        [[nodiscard]] const std::optional<RenderedEnvironment> &Environment(std::size_t index = 0) const noexcept;
        /// @brief 图回调对象所需的 arena 预留量, 不含帧拥有的 Draw 容器, 像素或 GPU 存储.
        [[nodiscard]] std::size_t GraphStorageBytes() const noexcept;
        /// @brief 返回本槽已准备的几何绘制项和活动灯表行数. 同一项供深度及颜色 Pass 消费时仍只计一次.
        [[nodiscard]] ForwardWorkCounts WorkCounts(std::size_t index = 0) const noexcept;
        /// @brief 汇总自本槽 Reset 起两路实际执行的编码. 命中已编码的共享行时计数保持原值.
        [[nodiscard]] ForwardEncodingCounts EncodingCounts() const noexcept;
        /// @brief 封存前拥有的写入需求, 包含之后可能不进入存活计划的写入.
        [[nodiscard]] std::pair<std::size_t, std::size_t> UploadDemand() const noexcept;

        /// @brief 声明共享上传及被选中的各路 Forward 输出. 先声明 UI 纹理, 最后声明 UI 缓冲.
        /// @pre selected 只包含有完整准备值的路, 本槽继续保有图借用直至执行结束.
        /// @note 返回值带各路独立的显示色版本. 调用方按 UI 采样或显式导出建立各自的消费者.
        [[nodiscard]] GraphResult<ForwardGraph> Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads,
            const std::array<bool, ForwardViewCount> &selected = {true, false});
        /// @brief 完整正常图获接受后, 分别确认全部所需写入均存活的存储代.
        /// @pre output 来自本次声明, plan 是接受的同图计划, 前缀提交不具备确认资格.
        void Confirm(const ForwardGraph &output, const CompiledGraphPlan &plan) noexcept;
        /// @brief 结束本帧场景和显示上传的 CPU 借用. 此后不能再用本批次声明或确认上传.
        /// @pre 图已结束录制或已放弃. 正常发布的候选先 Confirm; 放弃的候选跳过确认. GPU 引用仍由槽和提交包保活.
        void ReleasePayload() noexcept;

    private:
        friend class ForwardRenderer;
        std::unique_ptr<Detail::ForwardFrameStorage> m_Data;
    };

    /// @brief 设备 owner 线程准备驻留、绑定及原生 Pipeline, 仅消费帧内固定的资产版本.
    /// @note Device 由宿主持有. Renderer 和各槽须在原生 context 关闭前销毁.
    class ForwardRenderer final
    {
    public:
        ForwardRenderer(RHI::Device &device, ForwardPackagePaths packages, std::uint32_t maxTextureSize,
                        std::uint32_t maxAttachmentSize,
                        std::uint32_t uniformAlignment = RHI::ConstantBufferOffsetSizeAlignment) noexcept;
        ~ForwardRenderer();
        ForwardRenderer(const ForwardRenderer &) = delete;
        ForwardRenderer &operator=(const ForwardRenderer &) = delete;

        /// @brief 以当前 Scene 的有效源开启需求收集, 各路 Prepare 合并自己的实际环境需求.
        void BeginEnvironmentRequests(const Asset::EnvironmentVersion &source) noexcept;
        /// @brief 所有可消费的 View 已准备, 撤销已失去消费者的环境作业.
        void EndEnvironmentRequests() noexcept;

        /// @brief 用完整模型表更新已有几何目录, 包括本帧只构建诊断图的情况.
        /// @pre scene 来自本轮成功提取. 本轮未提取时调用方沿用上次观察.
        /// @return 模型表准备内存不足时报告错误并保留上次观察.
        /// @note 此入口不创建驻留缓存. 目录撤销后的区间由持有相应 Draw 的槽保活.
        [[nodiscard]] std::optional<ForwardFailure> ObserveModelTable(const SceneFrame &scene) noexcept;

        /// @brief 在创建资源前核对渲染和显示画布的单边上限及本 View 的固定附件预算.
        /// @param display 本路预览模式和 AA 请求解析出的实际显示配置.
        /// @param resample 显示阶段需要从渲染栅格重采样时为 true, 对应一个额外的线性中间目标.
        /// @return 空值表示附件可容纳. 失败携带具体的 View 原因, 显式离屏请求可在共享场景提取前返回该原因.
        [[nodiscard]] std::optional<ForwardFailure> CheckAttachments(ViewExtent render, ViewExtent output,
            DisplaySettings display, bool resample) const noexcept;

        /// @brief 用已有的静态工作集准备当前模式. 空可见集允许背景输出.
        /// @pre frame 已由安全槽 Reset, index 是本轮尚未准备的工作集位置. 各路在同一候选内连续准备.
        /// @note 局部失败清理该路的 Draw, 已完整登记的生产仍由共同批次保有, 图将核对活性.
        [[nodiscard]] std::optional<ForwardFailure> Prepare(ForwardFrame &frame, const PreparedView &view,
            PreviewMode mode, ViewResourceNeeds needs = {}, std::size_t index = 0);
        /// @brief 在设备 owner 线程读取目录条目数, 物理存储占用和材质复用计数. 首次准备前各项为零.
        [[nodiscard]] ForwardResidencyCounts ResidencyCounts() const noexcept;

    private:
        RHI::Device &m_Device;
        ForwardPackagePaths m_Packages;
        std::uint32_t m_MaxTextureSize;
        std::uint32_t m_MaxAttachmentSize;
        std::uint32_t m_UniformAlignment;
        std::unique_ptr<Detail::ForwardDevice> m_Resources;
    };
}
