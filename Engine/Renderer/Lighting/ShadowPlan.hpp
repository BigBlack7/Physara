#pragma once

/// @file
/// @brief 单个 View 的 CPU 阴影请求、有限视图、逻辑分配和缓存版本.
/// 帧内索引由匹配的 SceneFrame 解释. 区域代表示逻辑分配, 所需内容代由后续深度生产消费.

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include <RHI.hpp>
#include <Scene/Scene.hpp>

namespace Physara::Renderer
{
    struct CameraSnapshot;
    struct SceneFrame;
    namespace Detail
    {
        struct CullingPlane;
    }

    inline constexpr auto ShadowDepthFormat = RHI::Format::D32;
    inline constexpr std::uint32_t MaxShadowEdge = 1024;
    inline constexpr std::uint32_t MaxDirectionalCascades = 4;
    inline constexpr std::uint32_t MaxShadowSubviews = 6;
    inline constexpr std::uint32_t MaxShadowLayers = 16;
    inline constexpr std::uint64_t MaxShadowBytes = 32ull * 1024 * 1024;
    inline constexpr std::uint32_t ShadowGuardTexels = 4; ///< 每侧为最大比较核和投影位移保留的物理 texel.

    /// @brief Hard 比较中心 texel, PCF 平均同层九个比较结果. 两档共用深度内容.
    enum class ShadowFilter : std::uint8_t { Hard, Pcf };

    /// @brief 名义接收域到实际画幅的比例. edge 取已准入的物理边长.
    [[nodiscard]] constexpr double ShadowInnerScale(std::uint32_t edge) noexcept
    {
        return 1.0 - 2.0 * ShadowGuardTexels / edge;
    }

    /// @brief 每 View 的会话请求. edge 以 texel 计, 可取 1024, 512 或 256.
    /// shadowDistanceMeters 限制方向光接收段和局部灯的阴影半径. 方向光按近到远申请 1 到 4 层.
    /// splitLambda 只影响透视分段. 直接光继续使用源灯的 range.
    struct ShadowSettings
    {
        bool enabled = false;
        std::uint32_t edge = MaxShadowEdge;
        double shadowDistanceMeters = 50;
        std::uint32_t directionalCascades = MaxDirectionalCascades;
        double splitLambda = 0.5; ///< 透视分段的对数权重, 从线性 0 到对数 1.
        double transitionFraction = 0.05; ///< 过渡半宽占相邻较短段的比例.
        ShadowFilter filter = ShadowFilter::Hard;
        std::uint64_t byteBudget = MaxShadowBytes; ///< 请求的每 View D32 逻辑字节上限.
        bool operator==(const ShadowSettings &) const = default;
    };

    /// @brief 同一设备的分层深度规格上限及本轮逻辑字节预算. 零边长或零层表示所需用途不可用.
    /// 规划还受 MaxShadowLayers 和 MaxShadowBytes 约束. 每个 View 分别核算占用.
    struct ShadowPlanningLimits
    {
        std::uint32_t maxEdge{};
        std::uint32_t maxLayers{};
        std::uint64_t byteBudget{MaxShadowBytes};
        bool operator==(const ShadowPlanningLimits &) const = default;
    };

    /// @brief 计划或单灯的结果. 成功、无请求、局部拒绝及整份计划不可用各有原因.
    enum class ShadowPlanReason : std::uint8_t
    {
        Planned,
        Disabled,
        ViewUnavailable,
        DepthArrayUnavailable,
        DeviceSizeLimit,
        NoLights,
        NoReceiverCoverage,
        InvalidVolume,
        DirectionalLimit,
        Budget,
        StorageUnavailable,
        RevisionExhausted,
        LightUnavailable,
        Count
    };

    /// @brief 请求本次规划选出的主方向光级联, 无需持久保存灯的实体身份.
    struct CascadeDiagnostic
    {
        bool operator==(const CascadeDiagnostic &) const = default;
    };

    /// @brief 灯的 Scene 与 generation 位于 handle, kind 用于拒绝同一实体的灯型变更.
    struct LocalShadowSelection
    {
        Scene::EntityHandle light;
        Asset::ModelLightKind kind;
        bool operator==(const LocalShadowSelection &) const = default;
    };

    /// @brief 单个 View 的显示选择. monostate 恢复基础预览, 局部诊断持有带代的实体身份.
    using ShadowDiagnostic = std::variant<std::monostate, CascadeDiagnostic, LocalShadowSelection>;

    /// @brief 阴影使用独立的 [0,1] 深度, 近平面为 0.
    struct ShadowDepthConvention
    {
        static constexpr float Clear = 1.0f;
        static constexpr auto Compare = RHI::ComparisonFunc::Less;
        static constexpr double CasterBiasTexels = 0.5; ///< 沿发射方向的偏移, 按该投影的世界 texel 计.
    };

    /// @brief 方向光的正交投影. 原点位于吸附后的近面中心, axes 依次指向 clip 的 +X/+Y/+Z.
    /// XY 以 halfExtent 归一化, Z 随光线在 depthRange 内由 0 增至 1. 长度均为米.
    struct DirectionalShadowProjection
    {
        std::array<double, 3> origin;
        std::array<std::array<double, 3>, 3> axes;
        double halfExtent;
        double depthRange;
        bool operator==(const DirectionalShadowProjection &) const = default;
    };

    /// @brief 级联的投影及对应 SceneFrame 的 caster 下标. farDepth 和过渡端点为线性视深, 单位为米.
    /// transition 给出与下一级共用的覆盖, 两端相等表示直接切换或已到末级.
    struct DirectionalShadowCascade
    {
        DirectionalShadowProjection projection;
        float farDepth;
        std::array<float, 2> transition;
        std::vector<std::uint32_t> casters;
    };

    /// @brief 按近到远交付的主方向光组. cascades 与对应 ShadowLightPlan::Regions 顺序相同.
    struct DirectionalShadowPlan
    {
        float nearDepth{};
        std::vector<DirectionalShadowCascade> cascades;
    };

    /// @brief 有限局部阴影体. Spot 使用发射轴和外锥半角, Point 的各面使用共同的米制近远面.
    /// farPlane 同时限定灯心到接收点的半径. 投影裁剪按所选面的轴深计算, 斜向样点须满足两项限制.
    struct LocalShadowVolume
    {
        std::array<double, 3> position;
        std::array<double, 3> direction{};
        double nearPlane;
        double farPlane;
        double outerCone{};
        bool operator==(const LocalShadowVolume &) const = default;
    };

    /// @brief 局部灯单面的列主序齐次投影, 输入为相对灯位的世界向量. w 为米制面轴深度.
    struct LocalShadowProjection
    {
        std::array<std::array<float, 4>, 4> clipFromLight;
        float casterClipZ; ///< 含 caster 偏移的齐次 Z 常量, 写入时替换矩阵的 [3][2].
        bool operator==(const LocalShadowProjection &) const = default;
    };

    /// @brief 局部灯的有限体积、与区域同序的投影以及冻结 Primitive 下标. 点光六面共享 caster 集.
    struct LocalShadowPlan
    {
        LocalShadowVolume volume;
        float receiverOffsetRatio{}; ///< 接收偏移长度占面轴深的上限, 使投影位移不超过半个 texel.
        std::array<LocalShadowProjection, MaxShadowSubviews> projections{};
        std::vector<std::uint32_t> casters;
    };

    /// @brief 稳定的 CPU 子视图身份. Point 按轴 X/Y/Z、每轴正面在前的顺序交付.
    enum class ShadowSubview : std::uint8_t
    {
        Cascade0, Cascade1, Cascade2, Cascade3, Spot, PositiveX, NegativeX, PositiveY, NegativeY, PositiveZ, NegativeZ
    };

    /// @brief 区域占整个逻辑层. 像素范围由计划的 edge 决定, 分配代区分回收后重新使用的层.
    struct ShadowRegion
    {
        ShadowSubview subview;
        std::uint32_t layer;
        std::uint64_t allocationGeneration;
        std::uint64_t requiredContentRevision; ///< 对应此层需要产生的内容, 与 GPU 完成无关.
        bool operator==(const ShadowRegion &) const = default;
    };

    /// @brief 一盏准入灯的有限投影和投射物. casters 中的下标索引同帧 SceneFrame::primitives.
    /// Regions() 只交付有效项. 点光六面共用投射物集合, 面顺序固定, 层号可以离散.
    struct ShadowLightPlan
    {
        Scene::EntityHandle light;
        std::variant<DirectionalShadowPlan, LocalShadowPlan> views;
        std::array<ShadowRegion, MaxShadowSubviews> regions{};
        std::uint32_t regionCount{};

        [[nodiscard]] std::span<const ShadowRegion> Regions() const noexcept
        {
            return std::span(regions).first(regionCount);
        }

        /// @brief 以组内次序取得投射物, 按同次 SceneFrame 解析返回的下标.
        /// @pre region 小于 Regions().size(). 分配器交付的 layer 是另一套地址.
        [[nodiscard]] std::span<const std::uint32_t> Casters(std::size_t region) const noexcept;
    };

    /// @brief 按 PreparedView::lights 的顺序记录的灯. sceneLightIndex 索引同帧 SceneFrame::lights.
    /// plan 有值时索引本计划的 admitted, 空值由 reason 解释. Forward 另外按灯种类打包 GPU 行.
    struct ShadowLightLink
    {
        Scene::EntityHandle light;
        std::uint32_t sceneLightIndex;
        std::optional<std::uint32_t> plan;
        ShadowPlanReason reason;
    };

    /// @brief 可跨安全槽共享的不可变 CPU 计划. 只用匹配的 SceneFrame 解析其中的灯和 Primitive 索引.
    /// View 身份由外层 PreparedView 交付. 层跨度涵盖最高已用层前的空层.
    struct ShadowPlan
    {
        ShadowDepthConvention depth;
        std::uint32_t edge{};
        std::uint32_t layerSpan{};
        std::vector<ShadowLightLink> lights;
        std::vector<ShadowLightPlan> admitted;

        /// @brief 主方向光组排在所有局部灯前. 未准入时返回空.
        [[nodiscard]] const ShadowLightPlan *Directional() const noexcept;
    };

    /// @brief 单次 CPU 准备的有界摘要. planRevision 标识计划轮次, allocationRevision 标识最近一次映射变化.
    /// regions 数有效区域, layerSpan 包括最高已用层之前的空层. logicalBytes 按整个跨度计费.
    /// dirtyRegions 和 releasedRegions 以同一 View 上一次交付的分配为基准. 深度生产者自行核对已接受的版本.
    struct ShadowPlanStatistics
    {
        Scene::SceneId scene;
        std::uint64_t structureRevision{}, contentRevision{}, requestRevision{};
        std::uint64_t planRevision{}, allocationRevision{};
        std::uint64_t logicalBytes{}, byteLimit{};
        std::uint32_t requestedEdge{}, effectiveEdge{}, layerLimit{}, layerSpan{};
        std::uint32_t regions{}, dirtyRegions{}, releasedRegions{};
        std::uint32_t directionalCascades{};
        std::size_t plannedLights{};
        std::array<std::size_t, static_cast<std::size_t>(ShadowPlanReason::Count)> unallocated{};
        ShadowPlanReason reason = ShadowPlanReason::Disabled;
        ShadowPlanReason qualityReason = ShadowPlanReason::Planned;
        ShadowPlanReason directionalReason = ShadowPlanReason::Disabled;
        /// 本 View 选中灯的解析结果. 即使其它灯取得区域, 当前诊断仍可有自己的拒绝原因.
        ShadowPlanReason diagnosticReason = ShadowPlanReason::Disabled;
        ShadowFilter filter = ShadowFilter::Hard; ///< 本次请求的过滤方式, 不属于 CPU 深度内容版本.
        bool enabled{}, reused{};
    };

    /// @brief 更新后交付的 CPU 结果. 可用计划与摘要同次产生, 无计划时摘要仍标识当轮输入.
    struct PreparedShadows
    {
        std::shared_ptr<const ShadowPlan> plan;
        ShadowPlanStatistics statistics;
        const ShadowLightPlan *diagnostic{}; ///< 借用本结果的 plan, 空值由 diagnosticReason 说明.
    };

    /// @brief View owner 线程持有的缓存. 缓存按 Scene/稳定相机/有效配置匹配, 可以跨安全槽轮转.
    /// 读取旧计划的帧保有其不可变值; 缓存从不跨次借用 SceneFrame.
    class ShadowPlanCache final
    {
    public:
        ShadowPlanCache() noexcept;
        ShadowPlanCache(ShadowPlanCache &&) noexcept;
        ~ShadowPlanCache();

        /// @brief 从已验证 View 输入准备完整计划. 失败或禁用时交付空分配及原因.
        /// @pre 可见索引、相机和裁剪面来自同次稳定 View 准备; settings 已由所属 View 校验.
        /// @return 可消费的计划通过只读共享引用保活. 单灯拒绝保存在该灯的关联记录中.
        /// @param diagnostic 此帧可消费的会话诊断. 缓存不会因只切换诊断选择而重建 CPU 计划.
        /// @param requireDiagnostic 布局只容纳线性诊断时, 在选中灯完整准入后才发布分配.
        /// @param hasVisibleReceivers 在仅容纳诊断的布局中, 没有可见几何时只解析拒绝原因并结算旧分配.
        [[nodiscard]] PreparedShadows Prepare(const SceneFrame &scene, const CameraSnapshot &camera,
            std::span<const Detail::CullingPlane, 6> planes, std::array<double, 2> sideLimit,
            std::span<const std::uint32_t> lights, const ShadowSettings &settings, std::uint64_t requestRevision,
            const ShadowPlanningLimits &limits, const ShadowDiagnostic &diagnostic = {},
            bool requireDiagnostic = false, bool hasVisibleReceivers = true) noexcept;

        /// @brief 当前 View 无可用相机或布局时结束活动分配, 保留已交付值的只读寿命.
        [[nodiscard]] PreparedShadows Unavailable(Scene::SceneId scene, const Scene::Revisions &revisions,
            const ShadowSettings &settings, std::uint64_t requestRevision, const ShadowPlanningLimits &limits) noexcept;

    private:
        struct State;
        /// @brief 释放缓存持有的活动映射, 将释放数和最新修订写入本次摘要.
        void Clear(ShadowPlanStatistics &statistics) noexcept;

        std::unique_ptr<State> m_State;
        std::uint64_t m_PlanRevision{};
        std::uint64_t m_AllocationRevision{};
    };
}
