#pragma once

/// @file
/// @brief 每个 View 持有自己的相机, 比例, AA, 会话预览和阴影请求, 以及最近接受的显示色.
/// 主视口按编辑器布局请求像素, 附加离屏 View 由调用方提供布局. Runtime 按各自的图出口确认颜色.
/// Prepare 取得当前镜头和曝光, 匹配场景颜色获接受时才同时确认渲染相机与曝光.

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <variant>

#include <Core/Error.hpp>
#include <Core/Identity.hpp>
#include <RHI.hpp>
#include <Scene/Scene.hpp>
#include "FrameGraph.hpp"
#include "Lighting/ShadowPlan.hpp"
#include "Prepare/CameraSnapshot.hpp"

namespace Physara::Core
{
    class FrameArena;
}

namespace Physara::Renderer
{
    struct PreparedView;
    class ViewWork;
    struct ViewIdentityTag;
    /// @brief 跨布局和纹理换代保持稳定的逻辑 View 身份.
    using ViewId = Core::Identity<ViewIdentityTag>;

    /// @brief 无单位渲染比例的闭区间. 初始请求使用上界.
    inline constexpr float MinRenderScale = 0.5f;
    inline constexpr float MaxRenderScale = 1.0f;
    /// @brief 导航透视缺省: 垂直 FOV 为 pi/3 弧度, 近面为 0.10 m, 远面无限.
    /// 有效视场由 View 镜头给出, 宽高比来自有效画布.
    inline constexpr Asset::ModelPerspective NavigationProjection{std::numbers::pi / 3, 0.10, {}, {}};

    /// @brief View 拥有的物理相机请求, 供新 View 和省略该对象的 v1 场景共用缺省.
    /// 除补偿为有限档数外, 各字段须有限且为正; View 另检查导航 FOV 和派生 FP32 倍率.
    /// 三项长度均为米. 对焦距离目前只保存输入, 尚不据此计算景深或限制薄透镜关系.
    struct PhysicalCameraRequest
    {
        static constexpr double DefaultSensorHeightMeters = 0.024;

        double fNumber = 16.0; ///< 无单位的光圈 f 数, 不按孔径直径解释.
        double shutterSeconds = 1.0 / 125.0;
        double iso = 100.0;
        double exposureCompensationEv = 0.0; ///< 正一档令倍率加倍.
        double sensorHeightMeters = DefaultSensorHeightMeters;
        // 24 mm 传感器在 60 度垂直视场下的焦距, 与缺省导航透视一致.
        double navigationFocalLengthMeters = DefaultSensorHeightMeters * std::numbers::sqrt3 / 2.0;
        double focusDistanceMeters = 10.0;

        bool operator==(const PhysicalCameraRequest &) const = default;
    };

    /// @brief 主 View 持久化的单样本抗锯齿请求.
    enum class AntiAliasing : std::uint8_t
    {
        None,
        Fxaa
    };

    /// @brief 持久化主 View 请求. 相机引用由会话根据候选 Scene 解释.
    struct ViewDocumentRequests
    {
        Scene::Pose cameraPose;
        std::optional<Scene::EntityId> cameraEntityId;
        float renderScale = MaxRenderScale;
        PhysicalCameraRequest physicalCamera;
        AntiAliasing antiAliasing = AntiAliasing::None;

        bool operator==(const ViewDocumentRequests &) const = default;
    };

    /// @brief 从同一合法请求求出的导航视场和预曝光, 跟随该请求发布.
    /// 这是当前请求的派生值, 不表示 GPU 已渲染该曝光; 渲染记录由 RenderedScene 提供.
    struct PhysicalCameraEvaluation
    {
        double navigationYfov;
        CameraExposure exposure;
    };

    /// @brief 已校验的 View 文档更新. 只可由创建它的 View 在同一请求代发布.
    class ViewDocumentUpdate final
    {
    public:
        ViewDocumentUpdate(ViewDocumentUpdate &&) noexcept = default;
        ViewDocumentUpdate &operator=(ViewDocumentUpdate &&) noexcept = default;

    private:
        friend class View;
        ViewDocumentUpdate(ViewId view, std::uint64_t revision, ViewDocumentRequests value,
                           PhysicalCameraEvaluation physical, bool cameraChanged) noexcept;

        ViewId m_View;
        std::uint64_t m_Revision;
        ViewDocumentRequests m_Value;
        PhysicalCameraEvaluation m_Physical;
        bool m_CameraChanged;
    };

    /// @brief 离屏目标的物理像素宽高. Editor 从本帧布局请求生成有效范围.
    struct ViewExtent
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        bool operator==(const ViewExtent &) const = default;
    };

    /// @brief 在左上原点的像素画布内定位有效图像, 像素区间为半开.
    struct ViewImageRect
    {
        std::uint32_t x = 0;
        std::uint32_t y = 0;
        ViewExtent extent;

        bool operator==(const ViewImageRect &) const = default;
    };

    /// @brief 一张栅格的完整画布与有效矩形. 两种范围均以像素为单位.
    struct ViewImageLayout
    {
        ViewExtent outputExtent;
        ViewImageRect rect;

        /// @brief 由连续画布像素坐标得到左上原点的图像 UV. 留边与无效范围返回空.
        [[nodiscard]] std::optional<std::array<double, 2>> ImageUv(double x, double y) const noexcept;

        bool operator==(const ViewImageLayout &) const = default;
    };

    /// @brief 同一投影在渲染及显示栅格中的布局. 诊断测试图直接使用渲染布局.
    struct ViewLayouts
    {
        ViewImageLayout render;
        ViewImageLayout display;

        bool operator==(const ViewLayouts &) const = default;
    };

    /// @brief 指定一个 View 本轮的渲染及显示画布. 主视口由封存的面板布局换算, 附加消费者提供自己的范围.
    /// @note View 用请求代确认换算依据, 颜色纹理另有自己的资源代.
    struct ViewportRequest
    {
        ViewId view;
        ViewExtent renderExtent; ///< 乘请求比例后取整. 无法换算时为零.
        ViewExtent outputExtent; ///< 实际面板像素向下取整, 供正常场景的显示输出使用.
        std::uint64_t requestRevision = 0; ///< 用于计算像素范围的比例请求代.
    };

    /// @brief 编辑器可用渲染栅格显示诊断图, 显式离屏请求始终需要完整显示目标.
    enum class ViewportUse : std::uint8_t { DisplayOutput, EditorViewport };

    /// @brief 最后一次被接受的视口比例和实际像素范围.
    struct EffectiveViewport
    {
        float scale;
        ViewImageLayout layout;
    };

    /// @brief 准备或设置确认的原因. PreparedView 中 None 表示可消费, ViewSettings 中表示已接受.
    enum class ViewSettingReason : std::uint8_t
    {
        None,
        LayoutPending,
        SlotPending,
        SubmissionPending,
        UploadPending,
        InvalidExtent,
        TextureSizeLimit,
        DiagnosticBudget,
        CameraUnavailable,
        InvalidProjection,
        InvalidImageRect,
        InvalidSampling,
        ScenePreparationFailed,
        ScenePipelineUnavailable,
        ShaderPending,
        SceneResidencyPending,
        SceneResidencyLimit,
        SceneAttachmentLimit,
        SceneUnsupported,
        SceneUploadLimit,
        SceneLightTableRejected, ///< 可见灯集合超容量或行字段无法编码.
        EnvironmentPending,
        EnvironmentRejected,
        DisplayUnavailable,
        InvalidViewRequest,
        ViewCapacityLimit,
        SceneOutputUnavailable,
        ViewportSamplingPending ///< 有效请求等待主视口的可见采样, 上次接受的颜色及尺寸仍有效.
    };

    /// @brief 会话内 Forward 模式. 顺序同时用于编辑器菜单和 ForwardView.mode 的 Shader ABI.
    enum class PreviewMode : std::uint32_t
    {
        BaseColor,
        WorldNormal,
        MaterialChannels,
        Emissive,
        DirectLighting, ///< 直接光诊断, 使用当帧预曝光.
        PhysicalLighting
    };

    /// @brief 场景输出实际采用的显示变换.
    enum class DisplayProfile : std::uint8_t
    {
        LinearClamp,
        AcesSdr
    };

    /// @brief 已解析的显示配置, 与正常帧实际生成的颜色配对.
    struct DisplaySettings
    {
        DisplayProfile profile = DisplayProfile::LinearClamp;
        AntiAliasing antiAliasing = AntiAliasing::None;
    };

    /// @brief 物理预览消费 AA 请求; 诊断预览使用可比较的线性限幅.
    [[nodiscard]] constexpr DisplaySettings ResolveDisplaySettings(PreviewMode mode, AntiAliasing requested) noexcept
    {
        return mode == PreviewMode::PhysicalLighting ? DisplaySettings{DisplayProfile::AcesSdr, requested}
                                                     : DisplaySettings{};
    }

    /// @brief 同一 View 的请求和确认快照, 可独立用于面板显示.
    /// effective 在首个匹配的测试图或场景预览获接受前为空. 等待或拒绝时保留上次确认值.
    struct ViewSettings
    {
        ViewId view;
        float requestedScale = MaxRenderScale;
        std::optional<EffectiveViewport> effective;
        ViewSettingReason reason = ViewSettingReason::LayoutPending;
        std::uint64_t requestRevision = 0; ///< 比例请求的布局代, 不是 AA 的版本或 GPU 输出代.
        PreviewMode preview = PreviewMode::BaseColor; ///< 当前请求模式; 已接受像素的模式由 ViewOutput::scene 表达.
        AntiAliasing requestedAa = AntiAliasing::None; ///< 当前文档意图, 与最近场景帧实际采用的 AA 可以不同.
    };

    /// @brief Runtime 对诊断纹理像素记账的总字节上限, 同时约束单张诊断图的尺寸.
    inline constexpr std::size_t DiagnosticUploadBudget = 32 * 1024 * 1024;

    /// @brief 视口 token 持有的显示色角色. 测试图和场景预览共享该角色.
    enum class ViewOutputRole : std::uint8_t
    {
        DisplayColor
    };

    /// @brief 正常帧实际消费的源与角色. settings 交付查询参数及活跃角色, 可独立于活动 Scene 保留.
    using RenderedEnvironment = Scene::SceneEnvironment;

    /// @brief 一代场景预览的身份, 相机与模式. 候选携带准备值, View 接受后才成为已渲染记录.
    struct RenderedScene
    {
        Scene::SceneId scene;
        std::uint64_t contentRevision;
        CameraSnapshot camera;
        PreviewMode mode; ///< 该次提交的基础预览请求. 阴影诊断使用单独的实际光源标识.
        std::optional<Core::BoundedText> degradation;
        std::uint64_t documentRevision; ///< 准备时的 View 文档代, 不等于 Scene 内容代或时域历史 key.
        std::optional<RenderedEnvironment> environment; ///< 本颜色代实际使用的源和角色, 无环境消费时为空.
        DisplaySettings display; ///< 本场景候选选用的配置, 随正常图确认, 不是此刻界面上的请求.
        ViewImageLayout renderLayout; ///< 配对 HDR、深度和相机实际采样的栅格.
        ViewExtent outputExtent; ///< 此次场景输出的显示像素尺寸, 随场景历史保留.
        std::uint64_t shaderGeneration{}; ///< 正常图实际使用的冻结代, 随颜色提交确认.
        std::uint32_t directionalCascades{}; ///< 级联诊断的实际级数.
        std::uint64_t shadowRequestRevision{}; ///< 场景色所对应的会话阴影请求代, 含诊断退回常规预览的情形.
        std::optional<LocalShadowSelection> localShadow; ///< 局部诊断实际查询的灯与灯型.
        std::uint64_t submission{}; ///< 接受该颜色的正常提交票据.
    };

    /// @brief 最近一次成功诊断的身份. 普通画面替换颜色后仍可显示这次诊断的记录.
    struct RenderedShadowDiagnostic
    {
        Scene::SceneId scene;
        ShadowDiagnostic selection;
        std::uint64_t submission;
    };

    /// @brief 某 View 的诊断深度组. 像素只在对应图提交接受后由 retained 持有.
    /// 逻辑区域版本与物理层顺序成对. 深度程序摘要和实际变体用于核对换代后的生产需求.
    struct ShadowDepthGroup
    {
        ViewId view;
        Scene::SceneId scene;
        Scene::EntityHandle light;
        std::uint32_t edge{};
        std::uint32_t count{};
        std::array<ShadowRegion, MaxShadowSubviews> regions{};
        const RHI::Device *device{};
        Core::BoundedText program;
        std::bitset<32> variants; ///< 深度角色的 8 种顶点布局及 4 种绕序/剔面组合, 空组只有清除.
        std::optional<Core::BoundedText> degradation; ///< 随该组生产确认的 caster 跳过摘要, 与主表面独立.
        GraphRetainedResource retained;

        /// @brief 对照同 View 的当前 CPU 计划, 核查身份、规格及所需内容代.
        [[nodiscard]] bool Matches(ViewId owner, Scene::SceneId current, const ShadowPlan &plan,
                                   const ShadowLightPlan &group) const noexcept;
    };

    /// @brief 视口颜色代及其身份. generation 随新候选分配, ViewId 保持稳定.
    /// @note 新颜色尚无物理对象. 池输出获接受后由 retained 持有, 独立执行的颜色由 texture 持有.
    struct ViewOutput
    {
        ViewId view;
        ViewOutputRole role = ViewOutputRole::DisplayColor;
        std::uint64_t generation = 0;
        ViewImageLayout layout;
        RHI::TextureHandle texture;
        std::optional<RenderedScene> scene; ///< 缺席表示空文档诊断图.
        GraphRetainedResource retained; ///< 设备池交接持有, 普通外部颜色使用 texture.
        [[nodiscard]] bool HasReadableColor() const noexcept { return retained ? retained.Readable() : bool(texture); }
    };

    /// @brief 本轮视口候选或安全 UI 的只读旧输出.
    /// @note 普通候选保存准备时的比例及请求代. 复用或旧输出拥有颜色句柄, 新像素借用本槽 arena.
    /// 不含跨帧 GraphId. IsRetained 区分旧输出快照, 不能仅凭纹理非空判断本轮发布资格.
    class PreparedViewOutput final
    {
    public:
        /// @brief 只在本次准备中借用的颜色代, 返回空时由 UI 决定占位.
        [[nodiscard]] const ViewOutput *Color() const noexcept { return m_Output ? &*m_Output : nullptr; }
        /// @brief 只读旧输出快照不代表本次请求候选, 只能供安全 UI 读取, 不能再次 Publish.
        [[nodiscard]] bool IsRetained() const noexcept { return m_Retained; }
        /// @brief 普通候选颜色匹配的比例请求代. 仅在 Color 非空且非 IsRetained 时使用.
        [[nodiscard]] std::uint64_t RequestRevision() const noexcept { return m_RequestRevision; }
        /// @brief 本次测试图需要的紧密像素字节数. 场景上传另由 ForwardFrame 持有, 不计入此值.
        [[nodiscard]] std::size_t UploadBytes() const noexcept { return m_UploadBytes; }
        /// @brief 当前候选的等待或拒绝原因, 可随占位或只读旧图一起显示.
        [[nodiscard]] ViewSettingReason UnavailableReason() const noexcept { return m_UnavailableReason; }

        /// @brief 在已预留的本槽空间中生成测试图像素. 复用已有代或无测试图上传时直接成功.
        /// @return 可供本轮录制时为 true. 空间不足时返回 false, 由调用方结束本次准备.
        [[nodiscard]] bool PrepareUpload(Core::FrameArena &arena) noexcept;
        /// @brief 将候选上传或已接受颜色的导入加入当前图, 不创建原生纹理.
        /// @pre Color 非空且为测试图或旧输出, PrepareUpload 已成功, 像素区段借用至图执行结束.
        /// @note 返回本图版本, 由实际消费者决定活性与持久导出. 声明本身不发布有效设置.
        [[nodiscard]] GraphResult<TextureId> Declare(RenderGraph &graph, std::vector<GraphUpload> &uploads) const;

    private:
        friend class View;
        std::optional<ViewOutput> m_Output;
        std::size_t m_UploadBytes = 0;
        std::span<const std::byte> m_Pixels;
        bool m_Retained = false;
        ViewSettingReason m_UnavailableReason = ViewSettingReason::None;
        float m_Scale = MaxRenderScale;
        std::uint64_t m_RequestRevision = 0;
        std::uint64_t m_CameraRevision = 0;
    };

    /// @brief 计算紧密 RGBA8 像素字节数. 零尺寸或超出 32 MiB 诊断预算时返回 0.
    [[nodiscard]] std::size_t DiagnosticPixelBytes(ViewExtent extent) noexcept;

    /// @brief 在 owner 线程管理持久化请求及会话模式, 准备并发布测试图或场景预览.
    /// @note 测试图在布局相同时可复用, 场景预览逐次分配新颜色代. GPU 在途访问由原提交保活.
    class View final
    {
    public:
        /// @brief 按宿主能力建立逻辑 View, 纹理创建由本轮图执行负责.
        /// @return 身份代耗尽时返回空. 视图输出随首次有效请求准备.
        [[nodiscard]] static std::optional<View> Create(std::uint32_t maxTextureSize) noexcept;
        View(const View &) = delete;
        View &operator=(const View &) = delete;
        View(View &&) noexcept = default;
        View &operator=(View &&) = delete;

        [[nodiscard]] ViewId Id() const noexcept { return m_Id; }
        /// @brief 按值读取比例请求, 最后确认的有效设置及当前原因.
        [[nodiscard]] ViewSettings Settings() const noexcept;
        /// @brief 当前持久请求的借用, 不得跨请求更新保留其内部字符串引用.
        [[nodiscard]] const ViewDocumentRequests &DocumentRequests() const noexcept { return m_Document; }
        /// @brief 任一持久请求净变化都推进此代, 用于保存及拒绝陈旧场景候选.
        [[nodiscard]] std::uint64_t DocumentRevision() const noexcept { return m_DocumentRevision; }
        /// @brief 位姿, 相机引用或有效导航 FOV 的变化代. 纯曝光变化不使几何输入映射过期.
        [[nodiscard]] std::uint64_t CameraRevision() const noexcept { return m_CameraRevision; }
        /// @brief 读取当前合法请求的曝光, 可早于该请求的首次场景提交.
        [[nodiscard]] CameraExposure Exposure() const noexcept { return m_Physical.exposure; }
        /// @brief 按当前合法镜头构造导航投影, 沿用导航的裁剪和 aspect 合同.
        [[nodiscard]] Asset::ModelPerspective NavigationCameraProjection() const noexcept;
        /// @brief 解析当前请求的显示镜头. 空指针选择导航, 实体投影来自当前 Scene.
        /// @pre 非空投影已经过 Scene 校验, 仅在调用期间借用. 本入口不改写源投影.
        /// @return 正交没有 yfov 和焦距; 场景透视的等效焦距无法表示时仅该字段为空.
        [[nodiscard]] CameraLens ResolveLens(const Scene::Camera *sceneProjection) const noexcept;
        [[nodiscard]] PreviewMode Preview() const noexcept { return m_Preview; }
        /// @brief 借用最近一次已接受的真实场景记录, 测试图和安全 UI 不推进此值; 后续 Publish 可替换它.
        /// @note 记录可能属于上一文档. 使用前须核对 Scene 身份及相关版本; 当前显示说明用 AcceptedScenePreview.
        [[nodiscard]] const std::optional<RenderedScene> &LastRenderedScene() const noexcept { return m_RenderedScene; }
        /// @brief 借用最后一次获接受的阴影诊断及票据. 展示历史时须核对 Scene, 当前颜色另见 AcceptedScenePreview.
        [[nodiscard]] const std::optional<RenderedShadowDiagnostic> &LastShadowDiagnostic() const noexcept
        {
            return m_LastShadowDiagnostic;
        }

        /// @brief 在完成 UI 布局后选择本轮预览模式. 该请求不参与场景 JSON.
        /// @pre mode 为已声明的枚举值. 修改只进入待确认状态, 上一接受画面的模式随其颜色代保留.
        void RequestPreview(PreviewMode mode) noexcept;

        /// @brief 按值读取本 View 的会话请求. 实际准入的质量与层数见当轮 PreparedView::shadowStatistics.
        [[nodiscard]] ShadowSettings RequestedShadows() const noexcept { return m_ShadowSettings; }
        /// @brief 在帧边界接收会话阴影请求. 距离为有限正米数, 边长为支持档位, 级数为 1 到 4.
        /// splitLambda 和 transitionFraction 分别接受闭区间 [0,1] 和 [0,0.25].
        /// @return 成功返回空, 相同设置沿用原请求代. 身份过期, 参数无效或版号耗尽时返回原因并保留原请求.
        /// @note 新请求在下次 CPU 准备生效. 文档编辑代和已接受颜色按各自合同发布.
        [[nodiscard]] std::optional<Core::BoundedText> RequestShadows(ViewId view, ShadowSettings settings) noexcept;
        [[nodiscard]] const ShadowDiagnostic &RequestedShadowDiagnostic() const noexcept { return m_ShadowDiagnostic; }
        [[nodiscard]] std::uint64_t ShadowRequestRevision() const noexcept { return m_ShadowRequestRevision; }
        [[nodiscard]] bool CascadePreviewRequested() const noexcept
        {
            return std::holds_alternative<CascadeDiagnostic>(m_ShadowDiagnostic);
        }
        /// @brief 为匹配的 View 更新诊断请求, 与规划开关共用请求代. 拒绝时保留原值, 成功返回空.
        /// 关闭时释放本 View 持有的深度组, 原提交继续保活. 画面切换由下一次颜色接受确认.
        [[nodiscard]] std::optional<Core::BoundedText> RequestCascadePreview(ViewId view, bool enabled) noexcept;
        /// @brief 提交当前 Scene 的诊断选择. 局部灯身份和类型必须仍可解析, 拒绝保留原请求.
        [[nodiscard]] std::optional<Core::BoundedText> RequestShadowDiagnostic(
            ViewId view, const Scene::Scene &scene, ShadowDiagnostic diagnostic) noexcept;
        /// @brief 在同一 View 的正常诊断颜色发布后接收匹配的已接受深度出口.
        void AcceptShadowDepth(ShadowDepthGroup depth, std::uint64_t ticket) noexcept;

        /// @brief 无副作用地校验字段及派生 FOV/曝光, 失败返回可定位原因.
        /// 具体相机实体归 Scene, 由组合调用者校验; 成功不代表布局或 GPU 输出已就绪.
        [[nodiscard]] static std::optional<Core::BoundedText>
            ValidateDocumentRequests(const ViewDocumentRequests &requests) noexcept;
        /// @brief 预检全部请求和版号, 允许调用者与 Scene 事务一起发布.
        /// @note 返回值拥有候选请求及已求值曝光, 失败保留原请求和渲染记录.
        /// 准备本身不改写 owner; 有效输出仍由图形提交确认.
        [[nodiscard]] std::variant<ViewDocumentUpdate, Core::BoundedText>
            PrepareDocumentUpdate(ViewId view, ViewDocumentRequests requests) const noexcept;
        /// @pre 来自此 View, 且准备后本 View 的持久请求未变更. 不影响已采纳的显示资源.
        void PublishDocumentUpdate(ViewDocumentUpdate &&update) noexcept;
        /// @brief 取得最后接受颜色的拥有型只读快照, 缺失时仍明确表示安全 UI 输入.
        /// @note 不产生新代或上传, 不改变请求及有效设置. 旧图的 GraphId 不进入快照.
        /// reason 描述当前候选的等待或拒绝, 同旧输出的身份独立交付.
        /// 快照可跨后续 View 换代持有; 测试图和 Forward 预览均不原地覆写已接受颜色.
        [[nodiscard]] PreparedViewOutput LastAcceptedOutput(
            ViewSettingReason reason = ViewSettingReason::None) const noexcept;

        /// @brief 借用最近接受的显示色所关联的场景预览, 供会话同步布局.
        /// @return 若尚无显示色或最近接受的是测试图, 返回空.
        /// 与保留场景历史的 LastRenderedScene 分开, 防止测试图继续显示先前场景的降级摘要.
        /// @note 借用止于下一次成功 Publish 或 View 销毁.
        [[nodiscard]] const RenderedScene *AcceptedScenePreview() const noexcept;

        /// @brief 在帧边界接收此 View 的有限比例. 同值请求直接成功.
        /// @return 身份或比例无效, 或请求代耗尽时返回原因, 保留已有状态.
        [[nodiscard]] std::optional<Core::BoundedText> RequestRenderScale(ViewId view, float scale) noexcept;

        /// @brief 本轮没有提交候选颜色时记录真实等待原因, 已确认的当前请求保持有效.
        void Defer(ViewSettingReason reason) noexcept;

        /// @brief 在场景提取前核对请求、渲染栅格和显式离屏显示范围.
        /// 编辑器视口只要求渲染栅格在纹理上限内, 以便空文档仍能准备诊断图.
        [[nodiscard]] ViewSettingReason CheckViewport(const std::optional<ViewportRequest> &request,
            ViewportUse use = ViewportUse::DisplayOutput) const noexcept;
        /// @brief 复用布局相同的已接受测试图, 或按当前有效矩形建立测试图候选.
        /// @pre prepared 来自本轮已封存的布局和已应用的 Scene/View 请求.
        /// @param remainingUploadBytes 本轮可用的诊断像素额度, 由 Runtime 按槽记账.
        /// @return 无有效范围或上传额度不足时返回无颜色的结果. 身份代耗尽时返回原因.
        [[nodiscard]] std::variant<PreparedViewOutput, Core::BoundedText>
            PrepareDiagnostic(const PreparedView &prepared, std::size_t remainingUploadBytes);
        /// @brief 通过已准备的场景和相机建立本轮显示色候选, 由 Forward 图生产实际内容.
        /// @pre 场景 Draw 已使用此 View 准备值及其 preview 模式. degradation 只描述局部不支持项.
        /// @param environment 同一 Forward 候选选中的角色和源, 可与场景声明的用途不同.
        /// @param shaderGeneration 产品传入该帧槽持有的程序表编号, 随输出一起留给提交阶段核对.
        /// @param shadow 仅在本路实际准备了诊断时借用. 无匹配深度时释放 View 持有的旧组.
        [[nodiscard]] std::variant<PreparedViewOutput, Core::BoundedText>
            PreparePreview(const PreparedView &prepared, std::optional<Core::BoundedText> degradation,
                           std::optional<RenderedEnvironment> environment = std::nullopt,
                           std::uint64_t shaderGeneration = 0, const ShadowDepthGroup *shadow = nullptr);
        /// @brief 为本轮缺失的可绘输出记录同一原因, 供设置快照与 UI 占位读取.
        /// @note 已接受的显示色和有效设置仍归 View 持有.
        [[nodiscard]] PreparedViewOutput Unavailable(ViewSettingReason reason) noexcept;
        /// @brief 接受本轮匹配的颜色, 发布比例、布局与实际场景记录. 复用测试图也可确认新比例.
        /// @pre outputs 属于本轮准备, 其显示色在已接受的完整图中被 UI 采样或由该 View 显式导出.
        /// @param retained 池路径传入该颜色的 ShaderRead 出口和匹配的 ticket, 可在 GPU 完成前发布.
        /// @param acceptedTexture 独立执行路径传入同一版本的已接受句柄, 由调用者核对提交.
        /// @note 比例、相机或场景候选携带的持久请求代过期时保留原状态. 旧代由原 RHI 包保活.
        /// @return true 表示输出和有效设置已发布, false 表示候选无资格且原状态保留.
        [[nodiscard]] bool Publish(PreparedViewOutput &&outputs, const RHI::TextureHandle &acceptedTexture,
                                   const GraphRetainedResource &retained = {}, std::uint64_t ticket = 0) noexcept;

    private:
        friend class ViewWork;
        View(ViewId id, std::uint32_t maxTextureSize) noexcept;
        void ApplyRenderScale(float scale) noexcept;
        [[nodiscard]] std::optional<Core::BoundedText> SetShadowDiagnostic(ViewId view,
            ShadowDiagnostic diagnostic) noexcept;

        ViewId m_Id;
        std::uint32_t m_MaxTextureSize;
        std::uint64_t m_LastGeneration = 0;
        std::uint64_t m_RequestRevision = 0;
        ViewDocumentRequests m_Document;
        PhysicalCameraEvaluation m_Physical;
        std::uint64_t m_DocumentRevision = 0;
        std::uint64_t m_CameraRevision = 0;
        float m_EffectiveScale = MaxRenderScale;
        ViewSettingReason m_Reason = ViewSettingReason::LayoutPending;
        PreviewMode m_Preview = PreviewMode::BaseColor;
        ShadowSettings m_ShadowSettings;
        std::uint64_t m_ShadowRequestRevision{};
        ShadowDiagnostic m_ShadowDiagnostic;
        mutable ShadowPlanCache m_ShadowPlan; ///< 只由 owner 线程的 ViewWork 更新派生结果.
        mutable std::optional<ShadowDepthGroup> m_ShadowDepth; ///< 当前 View 已接受的只读深度组.
        std::optional<ViewOutput> m_Accepted;
        std::optional<RenderedScene> m_RenderedScene;
        std::optional<RenderedShadowDiagnostic> m_LastShadowDiagnostic;
    };
}
