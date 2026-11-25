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

    const ShadowLightPlan *ShadowPlan::Directional() const noexcept
    {
        return !admitted.empty() && std::holds_alternative<DirectionalShadowPlan>(admitted.front().views) ?
            &admitted.front() : nullptr;
    }

    std::span<const std::uint32_t> ShadowLightPlan::Casters(std::size_t region) const noexcept
    {
        assert(region < regionCount);
        if (const auto *directional = std::get_if<DirectionalShadowPlan>(&views))
        {
            return directional->cascades[region].casters;
        }
        return std::get<LocalShadowPlan>(views).casters;
    }

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

    /// @brief 交付灯组的固定子视图序列. 层地址由分配阶段填充.
    static std::span<const ShadowSubview> Subviews(ModelLightKind kind, std::uint32_t cascades) noexcept
    {
        static constexpr std::array directional{ShadowSubview::Cascade0, ShadowSubview::Cascade1,
                                                ShadowSubview::Cascade2, ShadowSubview::Cascade3};
        static constexpr std::array spot{ShadowSubview::Spot};
        static constexpr std::array point{ShadowSubview::PositiveX, ShadowSubview::NegativeX, ShadowSubview::PositiveY,
                                          ShadowSubview::NegativeY, ShadowSubview::PositiveZ, ShadowSubview::NegativeZ};
        if (kind == ModelLightKind::Directional)
        {
            return std::span(directional).first(cascades);
        }
        if (kind == ModelLightKind::Point)
        {
            return point;
        }
        return spot;
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

            const bool directional = kind == ModelLightKind::Directional;
            std::optional<LocalShadowVolume> local;
            if (directional)
            {
                if (std::min(receiver.farPlane, settings.shadowDistanceMeters) <= receiver.nearPlane)
                {
                    link.reason = ShadowPlanReason::NoReceiverCoverage;
                    continue;
                }
            }
            else
            {
                auto volume = Detail::MakeLocalShadowVolume(receiver, planes, light, settings.shadowDistanceMeters);
                if (const auto *reason = std::get_if<ShadowPlanReason>(&volume))
                {
                    link.reason = *reason;
                    continue;
                }
                local = std::get<LocalShadowVolume>(volume);
            }
            const auto subviews = Subviews(kind, settings.directionalCascades);
            const auto regions = static_cast<std::uint32_t>(subviews.size());
            const auto offer = layout.edge == 0 ? ChooseLayout(settings.edge, regions, limits, statistics.byteLimit) : layout;
            if (regions > offer.layers - statistics.regions)
            {
                link.reason = offer.edge == 0 ? offer.reason : ShadowPlanReason::Budget;
                continue;
            }

            ShadowLightPlan group{.light = link.light, .regionCount = regions};
            if (directional)
            {
                auto result = Detail::MakeDirectionalShadowPlan(scene, receiver, light, settings, offer.edge);
                if (const auto *reason = std::get_if<ShadowPlanReason>(&result))
                {
                    link.reason = *reason;
                    continue;
                }
                group.views = std::get<DirectionalShadowPlan>(std::move(result));
            }
            else
            {
                auto result = Detail::MakeLocalShadowPlan(scene, *local, offer.edge);
                if (!result)
                {
                    link.reason = ShadowPlanReason::InvalidVolume;
                    continue;
                }
                group.views = std::move(*result);
            }
            for (std::size_t i = 0; i < subviews.size(); ++i)
            {
                group.regions[i].subview = subviews[i];
            }
            layout = offer;
            primaryDirectional |= directional;
            link.plan = static_cast<std::uint32_t>(plan.admitted.size());
            link.reason = ShadowPlanReason::Planned;
            plan.admitted.push_back(std::move(group));
            statistics.regions += regions;
        }

        statistics.plannedLights = plan.admitted.size();
        statistics.directionalReason = ShadowPlanReason::NoLights;
        if (const auto *directional = plan.Directional())
        {
            statistics.directionalCascades = directional->regionCount;
            statistics.directionalReason = ShadowPlanReason::Planned;
        }
        else
        {
            const auto first = std::ranges::find_if(ordered, [&](auto index)
            {
                return scene.lights[visible[index]].light.type == ModelLightKind::Directional;
            });
            if (first != ordered.end())
            {
                statistics.directionalReason = plan.lights[*first].reason;
            }
        }
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
            const auto sets = std::holds_alternative<DirectionalShadowPlan>(group.views) ? group.regionCount : 1;
            for (std::size_t region = 0; region < sets; ++region)
            {
                for (const auto index : group.Casters(region))
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
        }
        return inputs;
    }

    /// @brief 按有效边长, 灯身份和首个子视图种类找旧组. 改变级数时仍可保留同序号的前几级.
    static const ShadowLightPlan *MatchingGroup(const ShadowPlan &old, const ShadowLightPlan &group,
                                                const ShadowLayout &layout) noexcept
    {
        if (old.edge != layout.edge)
        {
            return nullptr;
        }
        const auto found = std::ranges::find(old.admitted, group.light, &ShadowLightPlan::light);
        if (found == old.admitted.end() || found->regions[0].subview != group.regions[0].subview)
        {
            return nullptr;
        }
        return &*found;
    }

    /// @brief 比对同一子视图的投影和投射物. 两份 caster 下标分别解析各自的冻结场景.
    /// 名义切分距离供查询选级. 过渡区实际改变了投影或投射物时才影响这里的比较.
    static bool SameShadowContent(const ShadowLightPlan &current, const ShadowLightPlan &previous, std::size_t region,
        std::span<const std::optional<ShadowCaster>> inputs, std::span<const std::optional<ShadowCaster>> oldInputs)
    {
        if (const auto *directional = std::get_if<DirectionalShadowPlan>(&current.views))
        {
            if (directional->cascades[region].projection !=
                std::get<DirectionalShadowPlan>(previous.views).cascades[region].projection)
            {
                return false;
            }
        }
        else
        {
            const auto &now = std::get<LocalShadowPlan>(current.views);
            const auto &before = std::get<LocalShadowPlan>(previous.views);
            if (now.volume != before.volume || now.projections[region] != before.projections[region])
            {
                return false;
            }
        }
        return std::ranges::equal(current.Casters(region), previous.Casters(region),
                                  [&](auto now, auto before) { return inputs[now] == oldInputs[before]; });
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
                for (std::size_t face = 0; face < std::min(current.regionCount, match->regionCount); ++face)
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
            for (std::size_t face = 0; face < current.regionCount; ++face)
            {
                auto &region = current.regions[face];
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
                if (newAddress || !SameShadowContent(current, *old, face, inputs, oldInputs))
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
                .requestRevision = requestRevision,
                .byteLimit = std::min({limits.byteBudget, settings.byteBudget, MaxShadowBytes}),
                .requestedEdge = settings.edge, .layerLimit = std::min(limits.maxLayers, MaxShadowLayers),
                .reason = ShadowPlanReason::Disabled, .filter = settings.filter, .enabled = settings.enabled};
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
        statistics.directionalReason = statistics.reason;
        statistics.diagnosticReason = statistics.reason;
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

    /// @brief 在同一冻结场景下把选中身份解析到完整准入组. 保留单灯拒绝原因供 UI 与附件结算使用.
    static const ShadowLightPlan *ResolveDiagnostic(const ShadowPlan &plan, const SceneFrame &scene,
        const ShadowDiagnostic &diagnostic, ShadowPlanStatistics &statistics) noexcept
    {
        if (std::holds_alternative<CascadeDiagnostic>(diagnostic))
        {
            statistics.diagnosticReason = statistics.directionalReason;
            return plan.Directional();
        }
        if (const auto *local = std::get_if<LocalShadowSelection>(&diagnostic))
        {
            const auto found = std::ranges::find(plan.lights, local->light, &ShadowLightLink::light);
            if (found != plan.lights.end() && scene.lights[found->sceneLightIndex].light.type == local->kind)
            {
                statistics.diagnosticReason = found->reason;
                return found->plan ? &plan.admitted[*found->plan] : nullptr;
            }
            const auto light = std::ranges::find(scene.lights, local->light, &LightRecord::entity);
            statistics.diagnosticReason = light != scene.lights.end() && light->light.type == local->kind ?
                ShadowPlanReason::NoReceiverCoverage : ShadowPlanReason::LightUnavailable;
        }
        else
        {
            statistics.diagnosticReason = ShadowPlanReason::Disabled;
        }
        return nullptr;
    }

    PreparedShadows ShadowPlanCache::Prepare(const SceneFrame &scene, const CameraSnapshot &camera,
        std::span<const Detail::CullingPlane, 6> planes, std::array<double, 2> sideLimit,
        std::span<const std::uint32_t> lights, const ShadowSettings &settings, std::uint64_t requestRevision,
        const ShadowPlanningLimits &limits, const ShadowDiagnostic &diagnostic, bool requireDiagnostic,
        bool hasVisibleReceivers) noexcept
    {
        const auto initial = InitialStatistics(scene.scene, scene.structureRevision, scene.contentRevision, settings,
                                               requestRevision, limits);
        const auto reject = [&](ShadowPlanReason reason, const ShadowPlanStatistics &failed)
        {
            // 在旧缓存上结算发布过的区域. 本次试算只交付拒绝原因, 其中灯光诊断和总体摘要各有来源.
            auto summary = initial;
            summary.reason = requireDiagnostic && settings.enabled ? ShadowPlanReason::ViewUnavailable : reason;
            summary.directionalReason = failed.directionalReason;
            summary.diagnosticReason = failed.diagnosticReason;
            summary.unallocated = failed.unallocated;
            Clear(summary);
            return PreparedShadows{{}, summary};
        };
        const auto unavailable = [&](ShadowPlanReason reason)
        {
            // 全局规划失败时所有可见灯共用该原因. 普通和仅容纳诊断的布局使用同一计数口径.
            auto failed = initial;
            failed.directionalReason = failed.diagnosticReason = reason;
            if (settings.enabled)
            {
                failed.unallocated[static_cast<std::size_t>(reason)] = lights.size();
            }
            return reject(reason, failed);
        };
        if (!settings.enabled || limits.maxLayers == 0 || limits.maxEdge == 0)
        {
            return unavailable(settings.enabled ? ShadowPlanReason::DepthArrayUnavailable : ShadowPlanReason::Disabled);
        }
        if (requireDiagnostic && !hasVisibleReceivers)
        {
            // 有同轮冻结场景时仍可区分灯身份失配和空覆盖. 结算旧地址即可, 无需构造候选分配.
            auto failed = initial;
            failed.directionalReason = ShadowPlanReason::NoReceiverCoverage;
            static_cast<void>(ResolveDiagnostic(ShadowPlan{}, scene, diagnostic, failed));
            return reject(failed.diagnosticReason, failed);
        }
        auto productionSettings = settings;
        // 深度始终覆盖最大核. 仅切换比较方式时, 计划输入和所需内容保持一致.
        productionSettings.filter = ShadowFilter::Hard;
        const ShadowPlanInputs inputs{scene.scene, scene.structureRevision, scene.contentRevision,
            {camera.worldFromView, {camera.stableClipFromView[0], camera.stableClipFromView[5]}, sideLimit,
             camera.nearPlane, camera.cullingFar, camera.orthographic}, productionSettings, limits};
        if (m_State && m_State->inputs == inputs)
        {
            auto reused = m_State->statistics;
            const auto *selected = ResolveDiagnostic(*m_State->plan, scene, diagnostic, reused);
            if (requireDiagnostic && !selected)
            {
                return reject(reused.diagnosticReason, reused);
            }
            // 旧槽继续持有原计划. 这次返回自己的请求代和变化计数, 不改写上次发布的摘要.
            reused.requestRevision = requestRevision;
            reused.filter = settings.filter;
            reused.dirtyRegions = 0;
            reused.releasedRegions = 0;
            reused.reused = true;
            return {m_State->plan, reused, selected};
        }

        // 给候选预留下一版号, 到发布 State 时才提交. 布局拒绝只按旧缓存中的实际区域结算一次.
        auto revision = m_PlanRevision;
        if (!Core::AdvanceGeneration(revision))
        {
            return unavailable(ShadowPlanReason::RevisionExhausted);
        }
        try
        {
            auto statistics = initial;
            auto plan = std::make_shared<ShadowPlan>();
            const auto layout = SelectLights(*plan, scene, inputs.receiver, planes, lights, settings, limits, statistics);
            const auto *selected = ResolveDiagnostic(*plan, scene, diagnostic, statistics);
            if (requireDiagnostic && !selected)
            {
                // 布局只容纳诊断输出. 用仍活动的旧计划结算, 本次试算不分配区域或推进版号.
                return reject(statistics.diagnosticReason, statistics);
            }
            plan->edge = layout.edge;
            auto casters = CasterInputs(scene, *plan);
            // 即使两个 Scene 碰巧使用相同层号, 也只有同一 Scene 的实体和覆盖记录可以配对.
            const bool sameScene = m_State && m_State->inputs.scene == scene.scene;
            statistics.releasedRegions = m_State ? m_State->statistics.regions : 0;
            const auto *previous = sameScene ? m_State->plan.get() : nullptr;
            const auto oldCasters = sameScene ? std::span<const std::optional<ShadowCaster>>(m_State->casters) :
                std::span<const std::optional<ShadowCaster>>{};
            const auto oldRegions = statistics.releasedRegions;
            AssignRegions(*plan, previous, layout, casters, oldCasters, revision, statistics);
            statistics.planRevision = revision;
            statistics.allocationRevision = statistics.releasedRegions > 0 || statistics.regions != oldRegions ?
                revision : m_AllocationRevision;
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
            m_PlanRevision = revision;
            m_AllocationRevision = statistics.allocationRevision;
            return {std::move(plan), statistics, selected};
        }
        catch (const std::bad_alloc &)
        {
            return unavailable(ShadowPlanReason::StorageUnavailable);
        }
    }
}
