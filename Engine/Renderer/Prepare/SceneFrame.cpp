#include "SceneFrame.hpp"

/// @file
/// @brief 只在安全帧槽的内容发生变化时读取活动 Scene, 返回独立于 ECS 的记录.
/// 一次遍历收集灯与网格, 固定模型按记录去重, 最后发布缓存键和全部只读区间.

#include <algorithm>
#include <cassert>
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
    static void IncludeBounds(std::optional<Scene::Bounds> &total, const Scene::Bounds &bounds) noexcept
    {
        if (!total)
        {
            total = bounds;
            return;
        }
        for (std::size_t axis = 0; axis < 3; ++axis)
        {
            total->minimum[axis] = std::min(total->minimum[axis], bounds.minimum[axis]);
            total->maximum[axis] = std::max(total->maximum[axis], bounds.maximum[axis]);
        }
    }

    void SceneFrameStorage::Clear() noexcept
    {
        m_Scene = {};
        m_Objects.clear();
        m_Primitives.clear();
        m_Lights.clear();
        m_MaterialOverrides.clear();
        m_Models.clear();
        m_DrawableBounds.reset();
    }

    std::variant<SceneFrame, Core::BoundedText> SceneFrameStorage::Prepare(const Scene::Scene &scene)
    {
        const auto sceneId = scene.Id();
        const auto content = scene.Versions().renderContent;
        // 相机矩阵由 ViewWork 每轮另取, 名称等文档字段也不进入这些静态记录.
        // 影响网格或灯求值的自身或祖先变换仍由 Scene 推进 renderContent, 不会被纯相机复用路径漏掉.
        if (m_Scene == sceneId && m_ContentRevision == content)
        {
            return SceneFrame{m_Scene, m_ContentRevision, m_Objects, m_Primitives, m_Lights, m_Models,
                              m_MaterialOverrides, m_DrawableBounds};
        }
        // 已回收的槽可以复用数组容量. 全部提取成功前保持缓存键无效.
        Clear();
        try
        {
            // 同一不可变记录唯一确定 AssetId 和 revision, 地址只用于本次内部去重.
            std::unordered_map<const Asset::ModelRecord *, std::uint32_t> modelIndices;
            scene.VisitEntities([&](Scene::EntityHandle handle, const Scene::EntityView &entity)
            {
                const auto &[record, world, model, node] = entity;
                const auto &props = record.properties;
                if (props.light)
                {
                    static_cast<void>(NextIndex(m_Lights.size()));
                    const auto kind = props.light->type;
                    LightRecord light{.entity = handle, .light = *props.light,
                        .intensityUnit = kind == Asset::ModelLightKind::Directional ? Asset::ModelLightUnit::Lux :
                            Asset::ModelLightUnit::Candela, .visible = props.visible};
                    if (kind != Asset::ModelLightKind::Directional)
                    {
                        light.position = world.pose.translation;
                    }
                    if (kind != Asset::ModelLightKind::Point)
                    {
                        light.direction = world.Forward();
                    }
                    m_Lights.push_back(light);
                }
                // optional 的缺席表示没有 MeshRenderer; 值为 false 的网格仍参与相机可见性.
                if (!props.castsShadow)
                {
                    return;
                }

                assert(model && node && node->mesh);
                const auto [pin, inserted] = modelIndices.try_emplace(model->get(), NextIndex(m_Models.size()));
                if (inserted)
                {
                    m_Models.push_back(*model);
                }
                const auto object = NextIndex(m_Objects.size());
                m_Objects.push_back({handle, world.worldFromLocal, world.orientation});
                const auto mesh = *node->mesh;
                const auto &primitives = (*model)->meshes[mesh].primitives;
                // Scene 已保证覆盖按源槽排序. 仅有修改的槽解析有效材质, 其余继续共享模型正文.
                auto override = record.overrides.begin();
                for (const auto &primitive : primitives)
                {
                    static_cast<void>(NextIndex(m_Primitives.size()));
                    const auto slot = primitive.sourceSlot;
                    PrimitiveRef ref{object, pin->second, mesh, slot, primitive.material, {},
                                     world.primitives[slot], props.visible, *props.castsShadow};
                    if (override != record.overrides.end() && override->primitiveSlot == slot)
                    {
                        ref.materialOverride = NextIndex(m_MaterialOverrides.size());
                        auto material = scene.Material(handle, slot);
                        assert(material);
                        m_MaterialOverrides.push_back(std::move(*material));
                        ++override;
                    }
                    if (ref.visible && ref.bounds)
                    {
                        IncludeBounds(m_DrawableBounds, *ref.bounds);
                    }
                    m_Primitives.push_back(ref);
                }
            });
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
        m_Scene = sceneId;
        m_ContentRevision = content;
        return SceneFrame{m_Scene, m_ContentRevision, m_Objects, m_Primitives, m_Lights, m_Models,
                          m_MaterialOverrides, m_DrawableBounds};
    }
}
