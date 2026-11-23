#include "ShadowPlan.hpp"

/// @file
/// @brief 解析阴影预算, 成组分配并保留兼容区域, 核对各区域实际需要的深度内容.
/// 完整候选建成后替换 View 缓存, 已借出的计划继续以只读方式匹配原帧.

#include <algorithm>
#include <bit>
#include <bitset>
#include <cassert>
#include <new>
#include <numeric>
#include <tuple>
#include <utility>

#include "../Prepare/CameraSnapshot.hpp"
#include "../Prepare/SceneFrame.hpp"
#include "../Prepare/ViewFrustum.hpp"
#include "ShadowVolumes.hpp"

namespace Physara::Renderer
{
    using Detail::ShadowReceiver;
    using Asset::ModelLightKind;

    /// @brief 单档布局. 能力和项目上限已收紧到固定最大层数与有限质量档, 字节乘积在 uint64_t 内.
    struct ShadowLayout
    {
        std::uint32_t edge{};
        std::uint32_t layers{};
        std::uint64_t layerBytes{};
        ShadowPlanReason reason = ShadowPlanReason::Budget;
    };

    /// @brief 选择能容纳最高优先组的最大合法请求档. 同一布局中后续组复用该档位.
    static ShadowLayout ChooseLayout(std::uint32_t requestedEdge, std::uint32_t regions,
                                     const ShadowPlanningLimits &limits, std::uint64_t byteLimit) noexcept
    {
        // 先定位设备允许的最高请求档. 层上限在各档相同, 继续降低边长只能缓解单层字节开销.
        const auto deviceEdge = std::bit_floor(std::min(requestedEdge, limits.maxEdge));
        if (deviceEdge < 256)
        {
            return {.reason = ShadowPlanReason::DeviceSizeLimit};
        }
        const auto maxLayers = std::min(limits.maxLayers, MaxShadowLayers);
        for (auto edge = deviceEdge; edge >= 256; edge /= 2)
        {
            const auto layerBytes = std::uint64_t(edge) * edge * RHI::GetFormatInfo(ShadowDepthFormat).bytesPerBlock;
            const auto layers = static_cast<std::uint32_t>(std::min(std::uint64_t(maxLayers), byteLimit / layerBytes));
            if (layers >= regions)
            {
                const auto reason = edge < deviceEdge ? ShadowPlanReason::Budget :
                    (deviceEdge < requestedEdge ? ShadowPlanReason::DeviceSizeLimit : ShadowPlanReason::Planned);
                return {edge, layers, layerBytes, reason};
            }
        }
        return {};
    }

    /// @brief 同类灯依赖运行期槽号和分配代给出固定顺序, 与紧凑灯数组的顺序无关.
    static auto Priority(const LightRecord &light) noexcept
    {
        const int kind = light.light.type == ModelLightKind::Directional ? 0 :
            (light.light.type == ModelLightKind::Spot ? 1 : 2);
        return std::tuple(kind, light.entity.index, light.entity.generation);
    }

    /// @brief 初始化子视图身份. Point 的六面按照显式顺序, 层地址稍后作为整组保留或分配.
    static void SetSubviews(ShadowLightPlan &group, ModelLightKind kind) noexcept
    {
        if (kind == ModelLightKind::Point)
        {
            constexpr std::array faces{ShadowSubview::PositiveX, ShadowSubview::NegativeX, ShadowSubview::PositiveY,
                                       ShadowSubview::NegativeY, ShadowSubview::PositiveZ, ShadowSubview::NegativeZ};
            group.regionCount = static_cast<std::uint32_t>(faces.size());
            for (std::size_t face = 0; face < faces.size(); ++face)
            {
                group.regions[face].subview = faces[face];
            }
            return;
        }
        group.regionCount = 1;
        group.regions[0].subview = kind == ModelLightKind::Directional ? ShadowSubview::Directional : ShadowSubview::Spot;
    }

    /// @brief 保留完整可见灯索引, 按优先级准入有覆盖的投影. 局部拒绝仅改变该条阴影关联.
    static ShadowLayout SelectLights(ShadowPlan &plan, const SceneFrame &scene, const ShadowReceiver &receiver,
        std::span<const Detail::CullingPlane, 6> planes, std::span<const std::uint32_t> visible,
        const ShadowSettings &settings, const ShadowPlanningLimits &limits, ShadowPlanStatistics &statistics)
    {
        plan.lights.reserve(visible.size());
        for (const auto index : visible)
        {
            plan.lights.push_back({scene.lights[index].entity, index, {}, ShadowPlanReason::NoReceiverCoverage});
        }
        // 只排序行号. 预算优先级可以改变准入顺序, 可见灯仍能沿原顺序查到自己的结果.
        std::vector<std::uint32_t> ordered(visible.size());
        std::iota(ordered.begin(), ordered.end(), std::uint32_t{});
        std::ranges::sort(ordered, [&](auto left, auto right)
        {
            return Priority(scene.lights[visible[left]]) < Priority(scene.lights[visible[right]]);
        });

        ShadowLayout layout;
        bool primaryDirectional = false;
        for (const auto index : ordered)
        {
            auto &link = plan.lights[index];
            const auto &light = scene.lights[link.sceneLightIndex];
            const auto kind = light.light.type;
            if (kind == ModelLightKind::Directional && primaryDirectional)
            {
                link.reason = ShadowPlanReason::DirectionalLimit;
                continue;
            }

            auto volume = Detail::MakeShadowVolume(receiver, planes, light, settings.shadowDistanceMeters);
            if (const auto *reason = std::get_if<ShadowPlanReason>(&volume))
            {
                link.reason = *reason;
                continue;
            }
            const auto regions = kind == ModelLightKind::Point ? 6u : 1u;
            const auto offer = layout.edge == 0 ? ChooseLayout(settings.edge, regions, limits, statistics.byteLimit) : layout;
            if (regions > offer.layers - statistics.regions)
            {
                link.reason = offer.edge == 0 ? offer.reason : ShadowPlanReason::Budget;
                continue;
            }

            ShadowLightPlan group{.light = link.light, .volume = std::get<ShadowVolume>(std::move(volume))};
            if (!Detail::CollectShadowCasters(scene, group.volume, group.casters))
            {
                link.reason = ShadowPlanReason::InvalidVolume;
                continue;
            }
            SetSubviews(group, kind);
            layout = offer;
            primaryDirectional |= kind == ModelLightKind::Directional;
            link.plan = static_cast<std::uint32_t>(plan.admitted.size());
            link.reason = ShadowPlanReason::Planned;
            plan.admitted.push_back(std::move(group));
            statistics.regions += regions;
        }

        statistics.plannedLights = plan.admitted.size();
        for (const auto &link : plan.lights)
        {
            if (!link.plan)
            {
                ++statistics.unallocated[static_cast<std::size_t>(link.reason)];
            }
        }
        return layout;
    }

    /// @brief 与 alpha 覆盖相关的版本及采样值. 模型内图像索引由 caster 的固定模型版本限定.
    struct ShadowMaskTexture
    {
        std::variant<std::uint32_t, std::pair<Asset::AssetId, Asset::ContentRevision>> image;
        std::uint32_t texCoord;
        std::array<float, 2> offset;
        std::array<float, 2> scale;
        float rotation;
        std::optional<std::uint32_t> minFilter, magFilter;
        std::uint32_t wrapS, wrapT;
        bool operator==(const ShadowMaskTexture &) const = default;
    };

    /// @brief 决定深度覆盖的有效材质值. 双面性影响剔面, MASK 还取基础色 alpha 和纹理采样参数.
    struct ShadowCoverage
    {
        Asset::ModelAlpha alphaMode;
        bool doubleSided;
        float alpha{}, cutoff{};
        std::optional<ShadowMaskTexture> texture;
        bool operator==(const ShadowCoverage &) const = default;
    };

    /// @brief 描述影响同一几何覆盖的字段. OPAQUE 直接使用绕序合同, MASK 另记录有效 alpha 输入.
    static ShadowCoverage Coverage(const Scene::EffectiveMaterial &material) noexcept
    {
        const auto &values = material.values;
        ShadowCoverage coverage{.alphaMode = values.alphaMode, .doubleSided = values.doubleSided};
        if (values.alphaMode == Asset::ModelAlpha::Mask)
        {
            coverage.alpha = values.baseColor[3];
            coverage.cutoff = values.alphaCutoff;
            if (const auto &texture = material.textures[static_cast<std::size_t>(Asset::MaterialSlot::BaseColor)])
            {
                const auto &slot = texture->slot;
                const auto &sampler = texture->sampler;
                ShadowMaskTexture key{.texCoord = slot.texCoord, .offset = slot.offset, .scale = slot.scale,
                                      .rotation = slot.rotationRadians, .minFilter = sampler.minFilter,
                                      .magFilter = sampler.magFilter, .wrapS = sampler.wrapS, .wrapT = sampler.wrapT};
                if (const auto *source = std::get_if<const Asset::ModelImage *>(&texture->image))
                {
                    key.image = (*source)->sourceIndex;
                }
                else
                {
                    const auto &external = *std::get<Asset::ImageVersion>(texture->image);
                    key.image = std::pair(external.asset, external.revision);
                }
                coverage.texture = key;
            }
        }
        return coverage;
    }

    /// @brief 一条投射记录跨 SceneFrame 重排的身份和覆盖依赖. 比较值无需借用源模型或帧内数组.
    struct ShadowCaster
    {
        Scene::EntityHandle entity;
        Asset::AssetId asset;
        Asset::ContentRevision modelRevision;
        std::uint32_t mesh, sourceSlot;
        std::uint64_t transformRevision;
        ShadowCoverage coverage;
        bool operator==(const ShadowCaster &) const = default;
    };

    /// @brief 每个被引用的 Primitive 仅解析一次覆盖, 匹配来源索引只交给同次计划的比较.
    static std::vector<std::optional<ShadowCaster>> CasterInputs(const SceneFrame &scene, const ShadowPlan &plan)
    {
        if (plan.admitted.empty())
        {
            return {};
        }
        std::vector<std::optional<ShadowCaster>> inputs(scene.primitives.size());
        for (const auto &group : plan.admitted)
        {
            for (const auto index : group.casters)
            {
                if (inputs[index])
                {
                    continue;
                }
                const auto &primitive = scene.primitives[index];
                const auto &object = scene.objects[primitive.object];
                const auto &model = *scene.models[primitive.model];
                const auto coverage = primitive.materialOverride ? Coverage(scene.materialOverrides[*primitive.materialOverride]) :
                    Coverage(Scene::ResolveMaterial(model, model.meshes[primitive.mesh].primitives[primitive.sourceSlot],
                                                    nullptr));
                inputs[index] = ShadowCaster{object.entity, model.asset, model.revision, primitive.mesh,
                                             primitive.sourceSlot, object.transformRevision, coverage};
            }
        }
        return inputs;
    }

    /// @brief 找出同边长和子视图布局的旧灯组. 地址容量只影响单个旧层的可复用性.
    static const ShadowLightPlan *MatchingGroup(const ShadowPlan &old, const ShadowLightPlan &group,
                                                const ShadowLayout &layout) noexcept
    {
        if (old.edge != layout.edge)
        {
            return nullptr;
        }
        const auto found = std::ranges::find(old.admitted, group.light, &ShadowLightPlan::light);
        if (found == old.admitted.end() || found->regionCount != group.regionCount ||
            found->regions[0].subview != group.regions[0].subview)
        {
            return nullptr;
        }
        return &*found;
    }

    /// @brief 先占住相容的单面地址, 再为迁址或新建面定位空层. 灯组仍以完整结果发布.
    static void AssignRegions(ShadowPlan &plan, const ShadowPlan *previous, const ShadowLayout &layout,
        std::span<const std::optional<ShadowCaster>> inputs, std::span<const std::optional<ShadowCaster>> oldInputs,
        std::uint64_t revision, ShadowPlanStatistics &statistics)
    {
        std::bitset<MaxShadowLayers> occupied;
        std::array<const ShadowLightPlan *, MaxShadowLayers> oldGroups{};
        for (std::size_t group = 0; previous && group < plan.admitted.size(); ++group)
        {
            auto &current = plan.admitted[group];
            if (const auto *match = MatchingGroup(*previous, current, layout))
            {
                oldGroups[group] = match;
                for (std::size_t face = 0; face < current.regionCount; ++face)
                {
                    const auto &region = match->regions[face];
                    if (region.layer < layout.layers)
                    {
                        current.regions[face] = region;
                        occupied.set(region.layer);
                        --statistics.releasedRegions;
                    }
                }
            }
        }

        // 组准入已保证全部层可用. 先保留旧地址后, 离散的空层也可以组成一个 Point 分配.
        std::uint32_t freeLayer = 0;
        for (std::size_t group = 0; group < plan.admitted.size(); ++group)
        {
            auto &current = plan.admitted[group];
            const auto *old = oldGroups[group];
            // 结构编辑可以重排 Primitive 下标. 两边各自索引所属快照, 比较固定来源及实际覆盖.
            const bool sameContent = old && old->volume == current.volume &&
                std::ranges::equal(current.casters, old->casters,
                                   [&](auto now, auto before) { return inputs[now] == oldInputs[before]; });
            for (auto &region : std::span(current.regions).first(current.regionCount))
            {
                // 旧层可沿用时已经复制了两个版本. 分配代为零的面需要新地址, 即使灯和投射物均未改变.
                const bool newAddress = region.allocationGeneration == 0;
                if (newAddress)
                {
                    while (occupied[freeLayer])
                    {
                        ++freeLayer;
                    }
                    assert(freeLayer < layout.layers);
                    region.layer = freeLayer++;
                    region.allocationGeneration = revision;
                }
                if (newAddress || !sameContent)
                {
                    region.requiredContentRevision = revision;
                    ++statistics.dirtyRegions;
                }
                plan.layerSpan = std::max(plan.layerSpan, region.layer + 1);
            }
        }
    }

    /// @brief 冻结命中所需的稳定输入. jitter 和曝光只影响其他准备合同.
    struct ShadowPlanInputs
    {
        Scene::SceneId scene;
        std::uint64_t structureRevision, contentRevision;
        ShadowReceiver receiver;
        ShadowSettings settings;
        ShadowPlanningLimits limits;
        bool operator==(const ShadowPlanInputs &) const = default;
    };

    /// @brief 同一次准备发布的输入, 只读计划及覆盖依赖. casters 按该计划的帧内下标排列.
    struct ShadowPlanCache::State
    {
        ShadowPlanInputs inputs;
        std::shared_ptr<const ShadowPlan> plan;
        std::vector<std::optional<ShadowCaster>> casters;
        ShadowPlanStatistics statistics;
    };

    ShadowPlanCache::ShadowPlanCache() noexcept = default;
    ShadowPlanCache::ShadowPlanCache(ShadowPlanCache &&) noexcept = default;
    ShadowPlanCache::~ShadowPlanCache() = default;

    /// @brief 为本次输入建立统一统计身份, 无分配时仍交付请求及容量上界.
    static ShadowPlanStatistics InitialStatistics(Scene::SceneId scene, std::uint64_t structure, std::uint64_t content,
        const ShadowSettings &settings, std::uint64_t requestRevision, const ShadowPlanningLimits &limits) noexcept
    {
        return {.scene = scene, .structureRevision = structure, .contentRevision = content,
                .requestRevision = requestRevision, .byteLimit = std::min(limits.byteBudget, MaxShadowBytes),
                .requestedEdge = settings.edge, .layerLimit = std::min(limits.maxLayers, MaxShadowLayers),
                .reason = ShadowPlanReason::Disabled, .enabled = settings.enabled};
    }

    void ShadowPlanCache::Clear(ShadowPlanStatistics &statistics) noexcept
    {
        if (m_State && m_State->statistics.regions > 0)
        {
            statistics.releasedRegions = m_State->statistics.regions;
            static_cast<void>(Core::AdvanceGeneration(m_PlanRevision));
            m_AllocationRevision = m_PlanRevision;
        }
        m_State.reset();
        statistics.planRevision = m_PlanRevision;
        statistics.allocationRevision = m_AllocationRevision;
    }

    PreparedShadows ShadowPlanCache::Unavailable(Scene::SceneId scene, const Scene::Revisions &revisions,
        const ShadowSettings &settings, std::uint64_t requestRevision, const ShadowPlanningLimits &limits) noexcept
    {
        auto statistics = InitialStatistics(scene, revisions.renderStructure, revisions.renderContent, settings,
                                            requestRevision, limits);
        statistics.reason = settings.enabled ? ShadowPlanReason::ViewUnavailable : ShadowPlanReason::Disabled;
        Clear(statistics);
        return {{}, statistics};
    }

    /// @brief 无成功分配时选取局部结果中具有可操作性的原因. 单灯细节仍由对应表交付.
    static ShadowPlanReason UnallocatedReason(const ShadowPlan &plan, const ShadowPlanStatistics &statistics) noexcept
    {
        if (plan.lights.empty())
        {
            return ShadowPlanReason::NoLights;
        }
        for (const auto reason :
             {ShadowPlanReason::DeviceSizeLimit, ShadowPlanReason::Budget, ShadowPlanReason::InvalidVolume})
        {
            if (statistics.unallocated[static_cast<std::size_t>(reason)] > 0)
            {
                return reason;
            }
        }
        return ShadowPlanReason::NoReceiverCoverage;
    }

    PreparedShadows ShadowPlanCache::Prepare(const SceneFrame &scene, const CameraSnapshot &camera,
        std::span<const Detail::CullingPlane, 6> planes, std::array<double, 2> sideLimit,
        std::span<const std::uint32_t> lights, const ShadowSettings &settings, std::uint64_t requestRevision,
        const ShadowPlanningLimits &limits) noexcept
    {
        const auto initial = InitialStatistics(scene.scene, scene.structureRevision, scene.contentRevision, settings,
                                               requestRevision, limits);
        const auto unavailable = [&](ShadowPlanReason reason) -> PreparedShadows
        {
            // 主可见灯已经确定. 全局拒绝时把该集合计入实际原因, 禁用请求则没有待准入的灯.
            auto summary = initial;
            summary.reason = reason;
            if (settings.enabled)
            {
                summary.unallocated[static_cast<std::size_t>(reason)] = lights.size();
            }
            Clear(summary);
            return {{}, summary};
        };
        if (!settings.enabled || limits.maxLayers == 0 || limits.maxEdge == 0)
        {
            return unavailable(settings.enabled ? ShadowPlanReason::DepthArrayUnavailable : ShadowPlanReason::Disabled);
        }
        const ShadowPlanInputs inputs{scene.scene, scene.structureRevision, scene.contentRevision,
            {camera.worldFromView, {camera.stableClipFromView[0], camera.stableClipFromView[5]}, sideLimit,
             camera.nearPlane, camera.cullingFar, camera.orthographic}, settings, limits};
        if (m_State && m_State->inputs == inputs)
        {
            // 旧槽继续持有原计划. 这次返回自己的请求代和变化计数, 不改写上次发布的摘要.
            auto reused = m_State->statistics;
            reused.requestRevision = requestRevision;
            reused.dirtyRegions = 0;
            reused.releasedRegions = 0;
            reused.reused = true;
            return {m_State->plan, reused};
        }

        if (!Core::AdvanceGeneration(m_PlanRevision))
        {
            return unavailable(ShadowPlanReason::RevisionExhausted);
        }
        try
        {
            auto statistics = initial;
            auto plan = std::make_shared<ShadowPlan>();
            const auto layout = SelectLights(*plan, scene, inputs.receiver, planes, lights, settings, limits, statistics);
            plan->edge = layout.edge;
            auto casters = CasterInputs(scene, *plan);
            // 即使两个 Scene 碰巧使用相同层号, 也只有同一 Scene 的实体和覆盖记录可以配对.
            const bool sameScene = m_State && m_State->inputs.scene == scene.scene;
            statistics.releasedRegions = m_State ? m_State->statistics.regions : 0;
            const auto *previous = sameScene ? m_State->plan.get() : nullptr;
            const auto oldCasters = sameScene ? std::span<const std::optional<ShadowCaster>>(m_State->casters) :
                std::span<const std::optional<ShadowCaster>>{};
            const auto oldRegions = statistics.releasedRegions;
            AssignRegions(*plan, previous, layout, casters, oldCasters, m_PlanRevision, statistics);
            statistics.planRevision = m_PlanRevision;
            statistics.allocationRevision = statistics.releasedRegions > 0 || statistics.regions != oldRegions ?
                m_PlanRevision : m_AllocationRevision;
            statistics.effectiveEdge = plan->edge;
            statistics.layerSpan = plan->layerSpan;
            statistics.logicalBytes = plan->layerSpan * layout.layerBytes;
            statistics.reason = plan->admitted.empty() ? UnallocatedReason(*plan, statistics) : ShadowPlanReason::Planned;
            if (layout.edge != 0)
            {
                statistics.layerLimit = layout.layers;
                statistics.qualityReason = layout.reason;
            }
            // 地址, 所需内容代和摘要准备齐全后一起发布. 先前帧槽的共享引用继续指向原值.
            m_State = std::make_unique<State>(State{inputs, plan, std::move(casters), statistics});
            m_AllocationRevision = statistics.allocationRevision;
            return {std::move(plan), statistics};
        }
        catch (const std::bad_alloc &)
        {
            return unavailable(ShadowPlanReason::StorageUnavailable);
        }
    }
}
