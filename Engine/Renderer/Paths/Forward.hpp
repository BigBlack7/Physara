#pragma once

/// @file
/// @brief 正式 Forward 准备和图声明入口. 设备缓存与完成驱动的帧槽准备分开保有资源.

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>

#include <Shader/Reload.hpp>

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
        std::filesystem::path shadowDepth;
        std::filesystem::path cascadePreview;
        std::filesystem::path spotShadowPreview;
        std::filesystem::path pointShadowPreview;
        std::filesystem::path colorShadows;
        std::filesystem::path physicalShadows;
        std::filesystem::path iblShadows;
        std::array<std::filesystem::path, 6> froxelColors; ///< Direct, DFG, IBL, 再按同序保存带阴影角色.
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

    enum class ShaderPreparationState : std::uint8_t { Active, Pending, Failed };

    /// @brief 有界源码请求和帧前原生准备统计. active 记录准备器选中的代, 已接受的 View 另有提交票据.
    /// @note demand 按最近登记的不同键计数, started 和 budget 是本轮准备量, nativeSeconds 是累计秒数.
    struct ShaderPreparationStatistics
    {
        std::uint64_t active{}, request{}, nativeCreated{}, nativeHits{};
        std::size_t demand{}, inCache{}, cacheLimit{}, budget = 1, started{};
        double nativeSeconds{};
        ShaderPreparationState state = ShaderPreparationState::Pending;
        Shader::ProductShader role = Shader::ProductShader::Count;
        std::optional<Core::BoundedText> reason;
    };

    /// @brief 实际在途槽为退役代提供可计数的持有. entryLimit 为零时按有限键和帧槽数导出.
    struct ShaderPreparationLimits
    {
        std::size_t frames = 3;
        std::size_t perStep = 1;
        std::size_t entryLimit = 0;
    };

    /// @brief 显示前工作色及深度共用的来源. layout 是渲染栅格, camera.exposure 是本帧的曝光快照.
    /// mode 保存该 View 请求的基础预览. 实际诊断另保存级数或局部灯身份.
    /// 常规 PhysicalLighting 输出预曝光 linear Rec.709/D65, DirectLighting 包含预曝光直接光与自发光.
    struct ForwardWorkingSource
    {
        ViewId view;
        Scene::SceneId scene;
        std::uint64_t contentRevision;
        PreviewMode mode;
        CameraSnapshot camera;
        ViewImageLayout layout;
        std::uint32_t directionalCascades{};
        std::optional<LocalShadowSelection> localShadow;
        OpaquePath opaquePath = OpaquePath::Forward;
    };

    /// @brief 本图的工作颜色, 深度及显示颜色. 读回和显示处理消费同一工作色生产者.
    /// 显示色已执行视口映射. 受光或诊断可另带实际消费的常规深度数组, 所有 TextureId 仅在本图有效.
    struct ForwardOutput
    {
        TextureId workingColor; ///< 显示映射的输入. 启用天空时指向合成后的颜色版本.
        TextureId depth; ///< 与工作色共用渲染矩形的主 Reverse-Z 深度, 天空合成保留此版本.
        TextureId displayColor;
        ForwardWorkingSource source;
        PassId depthPass; ///< 独立深度预通道. 未请求时此 ID 无效, 由颜色 Pass 写主深度.
        PassId colorPass; ///< 本 View 的 ShadeOpaque. 对此 Pass 单独计时只覆盖不透明着色阶段.
        TextureId shadowDepth; ///< 本图用于受光或诊断的分层常规深度.
        std::uint64_t shadowBytes{}; ///< 这路消费的紧凑数组字节, 用于图准入.
        PassId firstPass, lastPass; ///< 本 View 私有子图的声明范围, 不含共享上传和 UI.

        /// @brief 检查 DeclareView 连续声明的 Pass. 含先行独占上传的完整归属由 ForwardGraph::PassOwner 查询.
        [[nodiscard]] bool OwnsPass(PassId pass) const noexcept
        {
            return pass.IsValid() && pass.generation == firstPass.generation &&
                pass.index >= firstPass.index && pass.index <= lastPass.index;
        }
    };

    namespace Detail
    {
        struct Allocation;
        struct BufferPage;
        struct ForwardFrameStorage;
        struct ForwardDevice;
    }

    /// @brief 同图声明的各路输出及存储代的真实生产者. 借用 ForwardFrame 至当前候选结束.
    struct ForwardGraph
    {
        std::array<std::optional<ForwardOutput>, ForwardViewCount> views;

        /// @brief 查找本图中私有子图或独占写入的 View. 共享写入与外部 Pass 返回空身份.
        [[nodiscard]] ViewId PassOwner(PassId pass) const noexcept;

    private:
        friend class ForwardFrame;
        std::map<Detail::Allocation *, std::vector<PassId>> m_StorageWrites;
        std::vector<std::pair<Detail::BufferPage *, PassId>> m_TableWrites;
        std::array<std::vector<PassId>, ForwardViewCount> m_ViewUploadPasses;
        PassId m_LutWrite;
        std::size_t m_LutView{};
    };

    /// @brief 一个存活几何 Pass 发出的批次数与它们消费的间接参数记录数.
    struct ForwardPassWork
    {
        std::size_t batches{};
        std::size_t records{};
    };

    /// @brief View 工作集与各存活几何 Pass 的执行口径. 表面数不随 Pass 重复计.
    struct ForwardWorkCounts
    {
        std::size_t draws{};
        std::size_t lights{};
        ForwardPassWork depth;
        ForwardPassWork color;
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
        std::size_t largestGeometryGap{}, objectPageBytes{}, materialPageBytes{};
        std::size_t textureBytes{}, environmentBytes{}, samplerEntries{};
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

        /// @brief 同槽安全取得时释放上轮 Draw 和活动绑定. View 常量, 普通灯表以及各路批次数据可供下轮复用.
        void Reset() noexcept;
        /// @brief 本槽持有的程序表编号, 尚未取得表时为零. View 的接受票据另由正常图提交产生.
        [[nodiscard]] std::uint64_t ShaderGeneration() const noexcept;
        /// @brief 借用本轮被跳过项的有界摘要, 下次 Reset/Prepare 后失效; 空值表示无局部降级.
        [[nodiscard]] const std::optional<Core::BoundedText> &Degradation(std::size_t index = 0) const noexcept;
        /// @brief 本候选实际使用的环境角色, 与当前 PreparedView 一起交给 View.
        [[nodiscard]] const std::optional<RenderedEnvironment> &Environment(std::size_t index = 0) const noexcept;
        /// @brief 图回调对象所需的 arena 预留量, 不含帧拥有的 Draw 容器, 像素或 GPU 存储.
        [[nodiscard]] std::size_t GraphStorageBytes() const noexcept;
        /// @brief 返回本槽已准备的几何绘制项和活动灯表行数. 同一项供深度及颜色 Pass 消费时仍只计一次.
        [[nodiscard]] ForwardWorkCounts WorkCounts(std::size_t index = 0) const noexcept;
        /// @brief 表面和灯按该 View 计一次, 批次和参数记录分别计入存活的深度与颜色 Pass.
        /// @pre output 属于本槽 index 对应的 View, plan 来自完整执行且已接受的同一张图.
        [[nodiscard]] ForwardWorkCounts ExecutedWorkCounts(std::size_t index, const ForwardOutput &output,
                                                           const CompiledGraphPlan &plan) const noexcept;
        /// @brief 汇总自本槽 Reset 起两路实际执行的编码. 命中已编码的共享行时计数保持原值.
        [[nodiscard]] ForwardEncodingCounts EncodingCounts() const noexcept;
        /// @brief 封存前拥有的写入需求, 包含之后可能不进入存活计划的写入.
        [[nodiscard]] std::pair<std::size_t, std::size_t> UploadDemand() const noexcept;
        /// @brief 读取当前规划的 Froxel 消费和待选路径的额外需求, 供实际资源准入排序.
        [[nodiscard]] bool UsesFroxels(std::size_t index) const noexcept;
        [[nodiscard]] bool AddsFroxels(std::size_t index) const noexcept;

        /// @brief 声明共享上传及被选中的各路 Forward 输出. 先声明 UI 纹理, 最后声明 UI 缓冲.
        /// @pre selected 只包含有完整准备值的路, 本槽继续保有图借用直至执行结束.
        /// 封存前按槽撤回未选路的私有缓冲和显示预留. 选中路从本图的上传结果取得 Froxel 读版本.
        /// @note 调用方用 UI 采样或显式导出建立颜色消费者. 本路新生产的 shadowDepth 若要跨帧复用,
        /// 调用方为其声明完整层范围的 ShaderRead 出口, 在本路颜色获接受后确认. 只读导入沿用原回执.
        /// 调用方也可按实际范围为 shadowDepth 独立声明读回.
        [[nodiscard]] GraphResult<ForwardGraph> Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads,
            const std::array<bool, ForwardViewCount> &selected = {true, false});
        /// @brief 完整正常图获接受后, 分别确认全部所需写入均存活的存储代.
        /// @pre output 来自本次声明, plan 是接受的同图计划, 前缀提交不具备确认资格.
        void Confirm(const ForwardGraph &output, const CompiledGraphPlan &plan) noexcept;
        /// @brief 本 View 的受光或诊断颜色发布后, 用同一正常提交的出口确认本次生产的深度数组.
        void ConfirmShadowDepth(View &owner, std::size_t index, const GraphRetainedResource &retained,
                                     std::uint64_t ticket) const noexcept;
        /// @brief 借出该路实际消费的深度集合, 无消费时返回空指针. retained 有值表示本次使用已接受的数组.
        /// @pre 本路 FinishView 已成功. 返回值只借用到槽 Reset 或再次准备.
        [[nodiscard]] const ShadowDepthSet *PreparedShadowDepth(std::size_t index = 0) const noexcept;
        /// @brief 从已绑定的实际阶段和固定附件生成 HDR/深度生产身份, 随 View 的接受结果保存.
        /// @pre 本槽的 index 已成功完成绑定. 返回值拥有程序摘要, 可随输出保存到槽复用之后.
        [[nodiscard]] OpaqueProduction Production(std::size_t index = 0) const noexcept;
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

        /// @brief 在同一已选代中连续规划并完成一路 View. 空可见集允许背景输出.
        /// 产品多 View 帧分别调用 PlanView 和 FinishView, 在两者之间推进一次原生预算.
        /// @pre frame 已由安全槽 Reset, index 是本轮尚未准备的工作集位置. 各路在同一候选内连续准备.
        /// @note 局部失败清理该路的 Draw 和已标记的私有写入. 批次仍有其它候选时保留共享资源的生产记录.
        [[nodiscard]] std::optional<ForwardFailure> Prepare(ForwardFrame &frame, const PreparedView &view,
            PreviewMode mode, ViewResourceNeeds needs = {}, std::size_t index = 0, const GraphBufferSupport &buffers = {});
        /// @brief 准备一路的常量, 批次及驻留资源, 汇入原生键供本帧统一选代.
        /// @param alternate 同一 View 另行准备的请求路径. view.opaquePath 是本轮先规划的策略.
        /// @param holdFroxelWrites 为有效 Froxel 方案保留写入额度. 首次受光候选延至收尾才申请.
        /// @pre frame 已由安全槽 Reset, index 是本帧尚未规划的位置. 局部失败仍允许规划其它路.
        [[nodiscard]] std::optional<ForwardFailure> PlanView(ForwardFrame &frame, const PreparedView &view,
            PreviewMode mode, ViewResourceNeeds needs = {}, std::size_t index = 0,
            std::optional<OpaquePath> alternate = {}, bool holdFroxelWrites = true);
        /// @brief 绑定选定代, 补办失效的 caster 准备, 仅为实际 Froxel 颜色角色向帧批次登记拥有型载荷.
        /// @pre view 和 index 与成功的 PlanView 相同, 第一条成功绑定后本帧无 Shader 切换.
        /// @param buffers 同一设备的场景图缓冲区能力. 默认值只允许普通 Forward 与无 Froxel 消费的预览.
        [[nodiscard]] std::optional<ForwardFailure> FinishView(ForwardFrame &frame, const PreparedView &view,
            std::size_t index = 0, const GraphBufferSupport &buffers = {});
        /// @brief 独立准备可选路径及其专用资源. 局部拒绝保留当轮仍有效的计划及原因.
        /// @pre 当前 View 已 PlanView, 两路需求已进入本帧原生准备. 调用方为此阶段独立捕获 RHI 错误.
        /// @return 无有效策略可接续的错误才交给调用方撤回准备.
        [[nodiscard]] std::optional<ForwardFailure> SelectOpaquePath(ForwardFrame &frame, PreparedView &view,
            View &owner, std::size_t index, const GraphBufferSupport &buffers);
        /// @brief 结算选路边界上无法继续的计划, 其它路的有效需求继续持有.
        void CancelPlannedView(ForwardFrame &frame, std::size_t index) noexcept;
        /// @brief 在设备 owner 线程读取目录条目数, 物理存储占用和材质复用计数. 首次准备前各项为零.
        [[nodiscard]] ForwardResidencyCounts ResidencyCounts() const noexcept;
        /// @brief 初始化会话的原生设备身份和额度, 在首帧准备前调用.
        void SetShaderPreparationLimits(Core::BoundedText deviceIdentity, ShaderPreparationLimits limits) noexcept;
        /// @brief 在 context owner 的帧前轮次推进已登记的原生需求及当前源码候选, 不切换有效代.
        /// @param request 本次已观察的源码修订, 纯打包会话为零. result 为空时仍可推进未完成的原生准备.
        /// @note 首次设备初始化前持有最新回执. 初始化失败后由后续 View 规划或新回执重新触发装配.
        void AdvanceShaderPreparation(std::uint64_t request = 0,
            std::optional<Shader::ReloadResult> result = {}) noexcept;
        /// @brief 用本轮剩余额度补备当前代, 供归还上传额度后重试的 View 使用.
        /// @pre 本帧已调用 AdvanceShaderPreparation 并登记重试所需的键. 已开始的原生创建继续计入同一额度.
        /// 源码候选的准入和发布仍在帧边界处理.
        void ContinueShaderPreparation() noexcept;
        /// @brief 开始汇总本帧需求和显示资源结果. 槽忙时继续沿用最近一次规划的事实.
        void BeginShaderRequests() noexcept;
        /// @brief 本帧须已登记原生键, 候选覆盖全部键, 各路显示输入已通过规划, 且有空间记录旧表.
        [[nodiscard]] bool ShaderCandidateReady() const noexcept;
        /// @brief 在本帧所有需求已收集且尚未冻结帧槽时, 以刚观察的源修订核对并提交完整候选.
        void PublishShaderPreparation(std::uint64_t request = 0,
            std::optional<Shader::ReloadResult> result = {}) noexcept;
        [[nodiscard]] ShaderPreparationStatistics ShaderStatistics() const noexcept;
        /// @brief 将本次原生边界的首错附加到候选, 错误持有随候选身份保留.
        void RecordShaderDiagnostic(const Core::BoundedText &reason) noexcept;

    private:
        /// @brief 设备准备器首次接管前的当前请求与回执. 更新的修订撤销旧回执, 设备失败后仍可交接.
        struct InitialShaderPreparation
        {
            std::uint64_t request{};
            std::optional<Shader::ReloadResult> result;
            std::optional<ForwardFailure> deviceFailure;
        };

        /// @brief 由首份当前回执或首个有效 View 触发设备装配. 失败时保留请求与回执供恢复.
        [[nodiscard]] bool InitializeResources() noexcept;

        RHI::Device &m_Device;
        ForwardPackagePaths m_Packages;
        std::uint32_t m_MaxTextureSize;
        std::uint32_t m_MaxAttachmentSize;
        std::uint32_t m_UniformAlignment;
        Core::BoundedText m_ShaderDevice;
        ShaderPreparationLimits m_ShaderLimits;
        std::optional<InitialShaderPreparation> m_InitialShaders{std::in_place};
        std::unique_ptr<Detail::ForwardDevice> m_Resources;
    };
}
