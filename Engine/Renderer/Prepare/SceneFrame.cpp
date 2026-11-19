#include "SceneFrame.hpp"

/// @file
/// @brief 安全槽按结构代重建数组, 或按输入轮次合并的索引追赶局部变更.
/// 观察变更与写入快照分开, 其它槽可以继续消费自己的记录和固定资产.

#include <algorithm>
#include <cassert>
#include <chrono>
#include <limits>
#include <new>
#include <unordered_map>
#include <utility>

namespace Physara::Renderer
{
    /// @brief 帧内索引与消费列表共用 32 位范围, 保证数组长度也在该范围内.
    static std::uint32_t NextIndex(std::size_t size)
    {
        if (size >= std::numeric_limits<std::uint32_t>::max())
        {
            throw Core::BoundedText("Scene frame index range exhausted");
        }
        return static_cast<std::uint32_t>(size);
    }

    /// @brief 汇集可绘世界范围, 无显式远面的相机用它求稳定裁剪深度.
    static std::optional<Scene::Bounds> JoinBounds(std::optional<Scene::Bounds> a,
                                                  const std::optional<Scene::Bounds> &b) noexcept
    {
        if (!a)
        {
            return b;
        }
        if (b)
        {
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                a->minimum[axis] = std::min(a->minimum[axis], b->minimum[axis]);
                a->maximum[axis] = std::max(a->maximum[axis], b->maximum[axis]);
            }
        }
        return a;
    }

    /// @brief 提取灯实际使用的分量, 使活动资产和 Scene 可在之后独立推进.
    static LightRecord ReadLight(Scene::EntityHandle handle, const Scene::EntityView &entity) noexcept
    {
        const auto &props = entity.record.properties;
        const auto kind = props.light->type;
        LightRecord result{.entity = handle, .light = *props.light,
            .intensityUnit = kind == Asset::ModelLightKind::Directional ? Asset::ModelLightUnit::Lux :
                Asset::ModelLightUnit::Candela, .visible = props.visible, .lightRevision = entity.lightRevision};
        if (kind != Asset::ModelLightKind::Directional)
        {
            result.position = entity.world.pose.translation;
        }
        if (kind != Asset::ModelLightKind::Point)
        {
            result.direction = entity.world.Forward();
        }
        return result;
    }

    static constexpr std::uint8_t RecordBit(Scene::RenderRecord record) noexcept
    {
        return static_cast<std::uint8_t>(1u << static_cast<unsigned>(record));
    }

    void SceneFrameStorage::Observe(const Scene::RenderChanges &changes) noexcept
    {
        // 新场景, 新结构或漏收的版本区间都需要重建. 在途槽只登记这个决定, 旧快照仍可读取.
        if (changes.scene != m_Scene || changes.structureRevision != m_StructureRevision ||
            changes.baseRevision > m_ObservedRevision)
        {
            m_Rebuild = true;
        }
        if (m_Rebuild || changes.contentRevision <= m_ObservedRevision)
        {
            return;
        }
        // Rebuild 已按实体数和源槽数预留列表. 每个实体或源槽只在首次置脏时入列.
        for (const auto &change : changes.records)
        {
            const auto found = m_Indices.find(change.entity.index);
            assert(found != m_Indices.end() && found->second.entity == change.entity);
            auto &indices = found->second;
            if (change.record == Scene::RenderRecord::Material)
            {
                assert(change.sourceSlot < indices.primitiveCount);
                const auto index = indices.firstPrimitive + change.sourceSlot;
                if (!m_MaterialPending[index])
                {
                    m_MaterialPending[index] = 1;
                    m_PendingMaterials.push_back(index);
                }
            }
            else
            {
                if (!indices.pending)
                {
                    m_PendingEntities.push_back(&indices);
                }
                indices.pending |= RecordBit(change.record);
            }
        }
        m_ObservedRevision = changes.contentRevision;
    }

    void SceneFrameStorage::Clear() noexcept
    {
        m_Scene = {};
        m_Objects.clear();
        m_Primitives.clear();
        m_Lights.clear();
        m_MaterialOverrides.clear();
        m_Models.clear();
        m_Bounds.clear();
        m_PendingEntities.clear();
        m_PendingMaterials.clear();
        m_Indices.clear();
        m_MaterialPending.clear();
        m_FreeOverrides.clear();
        m_HasGeometry = false;
        m_Rebuild = true;
    }

    /// @brief 冷槽或结构变化时重建紧凑索引, 同时为该结构的待消费标记预留空间.
    void SceneFrameStorage::Rebuild(const Scene::Scene &scene)
    {
        Clear();
        std::unordered_map<const Asset::ModelRecord *, std::uint32_t> modelIndices;
        scene.VisitEntities([&](Scene::EntityHandle handle, const Scene::EntityView &entity)
        {
            const auto &props = entity.record.properties;
            if (!props.light && !props.castsShadow)
            {
                return;
            }
            auto &indices = m_Indices.try_emplace(handle.index, EntityIndices{.entity = handle}).first->second;
            if (props.light)
            {
                indices.light = NextIndex(m_Lights.size());
                m_Lights.push_back(ReadLight(handle, entity));
            }
            if (!props.castsShadow)
            {
                return;
            }

            assert(entity.model && entity.sourceNode && entity.sourceNode->mesh);
            const auto &model = *entity.model;
            const auto [pin, inserted] = modelIndices.try_emplace(model.get(), NextIndex(m_Models.size()));
            if (inserted)
            {
                m_Models.push_back(model);
            }
            const auto object = NextIndex(m_Objects.size());
            indices.object = object;
            const auto &world = entity.world;
            m_Objects.push_back({handle, world.worldFromLocal, world.orientation, entity.transformRevision});
            m_Bounds.push_back(props.visible ? world.bounds : std::nullopt);
            m_HasGeometry |= world.bounds.has_value();
            const auto mesh = *entity.sourceNode->mesh;
            const auto &primitives = model->meshes[mesh].primitives;
            indices.firstPrimitive = NextIndex(m_Primitives.size());
            indices.primitiveCount = NextIndex(primitives.size());
            std::size_t override = 0;
            for (const auto &primitive : primitives)
            {
                static_cast<void>(NextIndex(m_Primitives.size()));
                const auto slot = primitive.sourceSlot;
                PrimitiveRef ref{object, pin->second, mesh, slot, primitive.material, {},
                                 world.primitives[slot], props.visible, *props.castsShadow};
                const auto &overrides = entity.record.overrides;
                if (override < overrides.size() && overrides[override].primitiveSlot == slot)
                {
                    ref.materialOverride = NextIndex(m_MaterialOverrides.size());
                    ref.materialRevision = entity.overrideRevisions[override];
                    m_MaterialOverrides.push_back(Scene::ResolveMaterial(*model, primitive, &overrides[override]));
                    ++override;
                }
                m_Primitives.push_back(ref);
            }
        });

        // 紧凑归并树的叶在后半段. 之后收缩极值只需更新对应叶到根的链.
        if (const auto leaves = m_Objects.size())
        {
            m_Bounds.resize(2 * leaves);
            std::copy_n(m_Bounds.begin(), leaves, m_Bounds.begin() + leaves);
            for (auto node = leaves - 1; node != 0; --node)
            {
                m_Bounds[node] = JoinBounds(m_Bounds[2 * node], m_Bounds[2 * node + 1]);
            }
            m_Bounds[0].reset();
        }
        m_PendingEntities.reserve(m_Indices.size());
        m_PendingMaterials.reserve(m_Primitives.size());
        m_MaterialPending.resize(m_Primitives.size());
    }

    /// @brief 用当前有效覆盖替换稀疏行. 清除覆盖时归还位置并释放该行拥有的图像引用.
    void SceneFrameStorage::UpdateMaterial(std::uint32_t index, const Scene::EntityView &entity)
    {
        auto &ref = m_Primitives[index];
        const auto &overrides = entity.record.overrides;
        const auto value = std::ranges::lower_bound(overrides, ref.sourceSlot, {}, &Scene::PrimitiveOverride::primitiveSlot);
        if (value != overrides.end() && value->primitiveSlot == ref.sourceSlot)
        {
            ref.materialRevision = entity.overrideRevisions[value - overrides.begin()];
            const auto &model = *m_Models[ref.model];
            auto material = Scene::ResolveMaterial(model, model.meshes[ref.mesh].primitives[ref.sourceSlot], &*value);
            if (!ref.materialOverride && !m_FreeOverrides.empty())
            {
                ref.materialOverride = m_FreeOverrides.back();
                m_FreeOverrides.pop_back();
            }
            if (ref.materialOverride)
            {
                m_MaterialOverrides[*ref.materialOverride] = std::move(material);
            }
            else
            {
                ref.materialOverride = NextIndex(m_MaterialOverrides.size());
                m_MaterialOverrides.push_back(std::move(material));
            }
        }
        else if (ref.materialOverride)
        {
            m_FreeOverrides.push_back(*ref.materialOverride);
            m_MaterialOverrides[*ref.materialOverride] = {};
            ref.materialOverride.reset();
            ref.materialRevision = 0;
        }
        m_MaterialPending[index] = 0;
    }

    /// @brief 将受影响对象的可见范围推到根, 允许旧的极值收缩.
    void SceneFrameStorage::UpdateBounds(std::uint32_t object, std::optional<Scene::Bounds> bounds) noexcept
    {
        auto node = m_Objects.size() + object;
        m_Bounds[node] = bounds;
        while ((node /= 2) != 0)
        {
            m_Bounds[node] = JoinBounds(m_Bounds[2 * node], m_Bounds[2 * node + 1]);
        }
    }

    /// @brief 按槽内索引追到事务终态. 覆盖与实体范围重叠时合并对同一 Primitive 的写入.
    void SceneFrameStorage::Update(const Scene::Scene &scene, SceneExtractionStatistics &statistics)
    {
        for (auto *indices : m_PendingEntities)
        {
            const auto entity = scene.Get(indices->entity);
            assert(entity);
            const auto &props = entity->record.properties;
            const bool transform = (indices->pending & RecordBit(Scene::RenderRecord::Transform)) != 0;
            const bool flags = (indices->pending & RecordBit(Scene::RenderRecord::PrimitiveFlags)) != 0;
            if (transform)
            {
                const auto &world = entity->world;
                m_Objects[*indices->object] = {indices->entity, world.worldFromLocal, world.orientation,
                                              entity->transformRevision};
                ++statistics.objects;
            }
            if (transform || flags)
            {
                UpdateBounds(*indices->object, props.visible ? entity->world.bounds : std::nullopt);
                for (std::uint32_t source = 0; source < indices->primitiveCount; ++source)
                {
                    const auto index = indices->firstPrimitive + source;
                    auto &ref = m_Primitives[index];
                    if (transform)
                    {
                        ref.bounds = entity->world.primitives[source];
                    }
                    if (flags)
                    {
                        ref.visible = props.visible;
                        ref.castsShadow = *props.castsShadow;
                    }
                    if (m_MaterialPending[index])
                    {
                        UpdateMaterial(index, *entity);
                    }
                    ++statistics.primitives;
                }
            }
            if (indices->pending & RecordBit(Scene::RenderRecord::Light))
            {
                m_Lights[*indices->light] = ReadLight(indices->entity, *entity);
                ++statistics.lights;
            }
            indices->pending = 0;
        }
        for (const auto index : m_PendingMaterials)
        {
            if (m_MaterialPending[index])
            {
                const auto entity = scene.Get(m_Objects[m_Primitives[index].object].entity);
                assert(entity);
                UpdateMaterial(index, *entity);
                ++statistics.primitives;
            }
        }
        m_PendingEntities.clear();
        m_PendingMaterials.clear();
    }

    std::variant<SceneFrame, Core::BoundedText> SceneFrameStorage::Prepare(const Scene::Scene &scene)
    {
        const auto start = std::chrono::steady_clock::now();
        const auto versions = scene.Versions();
        Observe(scene.PendingRenderChanges());
        SceneExtractionStatistics statistics{.mode = SceneExtraction::Reused};
        try
        {
            if (m_Rebuild)
            {
                Rebuild(scene);
                statistics = {SceneExtraction::Rebuilt, 0.0, m_Objects.size(), m_Primitives.size(), m_Lights.size()};
            }
            else if (m_ContentRevision != versions.renderContent)
            {
                statistics.mode = SceneExtraction::Updated;
                Update(scene, statistics);
            }
        }
        catch (const Core::BoundedText &reason)
        {
            Clear();
            return reason;
        }
        catch (const std::bad_alloc &)
        {
            Clear();
            return Core::BoundedText("Scene frame storage allocation failed");
        }
        // 所有派生范围和覆盖都已写完, 才能将这份槽记录标为可供各 View 借用.
        m_Scene = scene.Id();
        m_ContentRevision = m_ObservedRevision = versions.renderContent;
        m_StructureRevision = versions.renderStructure;
        m_Rebuild = false;
        statistics.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        return SceneFrame{m_Scene, m_ContentRevision, m_Objects, m_Primitives, m_Lights, m_Models,
                          m_MaterialOverrides, m_Bounds.empty() ? std::nullopt : m_Bounds[1], scene.Environment(),
                          m_StructureRevision, m_HasGeometry, statistics};
    }
}
