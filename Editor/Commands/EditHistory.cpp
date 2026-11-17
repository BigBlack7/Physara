#include "EditHistory.hpp"

/// @file
/// @brief 在 CPU 帧边界应用布局, TRS/比例/导航手势, Scene/View 事务及其历史重放.
/// 活动手势保存首值和最近接受值. Scene 记录按稳定 ID 重放, 相机回退与实体事务一起提交.

#include <algorithm>
#include <utility>

namespace Physara::Editor::Detail
{
    // 活动 EditorSession 由进程唯一宿主在 owner 线程串行装配.
    static std::uint64_t LastEditSession = 0;

    EditHistory::EditHistory() noexcept
    {
        if (Core::AdvanceGeneration(LastEditSession))
        {
            m_Id = {0, LastEditSession};
        }
    }

    // Open 只冻结文档条目; 显隐仍可重放. 任何活动字段手势都占用整条历史的重放边界.
    EditState EditHistory::Snapshot(bool frozen) const noexcept
    {
        const auto mayReplay = [this, frozen](std::size_t index)
        {
            return !m_Active && (!frozen || std::holds_alternative<VisibilityChange>(m_Entries[index]));
        };
        return {m_ViewportVisible, m_Applied != 0 && mayReplay(m_Applied - 1),
                m_Applied < m_Count && mayReplay(m_Applied)};
    }

    void EditHistory::Clear() noexcept
    {
        for (std::size_t i = 0; i < m_Count; ++i)
        {
            m_Entries[i] = VisibilityChange{};
        }
        m_Count = m_Applied = 0;
        m_Active.reset();
    }

    std::optional<Core::BoundedText> EditHistory::CancelGesture(Scene::Scene *scene, Renderer::View *view)
    {
        auto cancelled = std::exchange(m_Active, std::nullopt);
        if (!cancelled || cancelled->before == cancelled->after)
        {
            return std::nullopt;
        }
        return WriteField(scene, view, *cancelled, cancelled->before, false);
    }

    // 预览, 恢复和重放都借用同一个比例入口, 由 View 确认目标和取值.
    static std::optional<Core::BoundedText> ApplyScale(Renderer::View *view, Renderer::ViewId id,
                                                       float scale) noexcept
    {
        if (!view)
        {
            return Core::BoundedText("Render scale edit has no active View");
        }
        return view->RequestRenderScale(id, scale);
    }

    // 在当前应用位置追加记录. 撤销与重做共享同一固定容量.
    void EditHistory::Commit(Entry entry) noexcept
    {
        // 只在新编辑完成时截断 Redo. 满时移除最早的一项, 同时释放它保有的固定模型和图像引用.
        if (m_Applied == Capacity)
        {
            std::move(m_Entries.begin() + 1, m_Entries.end(), m_Entries.begin());
            --m_Applied;
        }
        m_Entries[m_Applied++] = std::move(entry);
        for (std::size_t i = m_Applied; i < m_Count; ++i)
        {
            m_Entries[i] = VisibilityChange{};
        }
        m_Count = m_Applied;
    }

    // 从权威数据收集首值, 将本地字段的 live 身份和稳定重放身份一起记录.
    std::optional<EditHistory::FieldChange> EditHistory::BeginGesture(Scene::Scene *scene, Renderer::View *view,
                                                                      const FieldTarget &target)
    {
        if (const auto *id = std::get_if<Renderer::ViewId>(&target))
        {
            if (view && view->Id() == *id)
            {
                const auto before = view->Settings().requestedScale;
                return FieldChange{target, {}, before, before};
            }
        }
        else if (const auto *local = std::get_if<LocalFieldTarget>(&target))
        {
            if (const auto entity = scene ? scene->Get(local->entity) : std::nullopt)
            {
                const auto &transform = entity->record.properties.local;
                FieldValue before = local->field == TransformField::Rotation ? FieldValue{transform.rotation} :
                    FieldValue{local->field == TransformField::Scale ? transform.scale : transform.translation};
                return FieldChange{target, entity->record.id, before, before};
            }
        }
        else if (const auto *navigation = std::get_if<NavigationTarget>(&target))
        {
            if (scene && scene->Id() == navigation->scene && view && view->Id() == navigation->view &&
                !view->DocumentRequests().cameraEntityId)
            {
                const auto &before = view->DocumentRequests().cameraPose;
                return FieldChange{target, {}, before, before};
            }
        }
        return std::nullopt;
    }

    // 每个字段以当前权威值为基底. 预览和取消只接受原实体代, 历史重放可解析新一代的同一稳定实体.
    std::optional<Core::BoundedText> EditHistory::WriteField(Scene::Scene *scene, Renderer::View *view,
        const FieldChange &field, const FieldValue &value, bool replay)
    {
        if (const auto *id = std::get_if<Renderer::ViewId>(&field.target))
        {
            return ApplyScale(view, *id, std::get<float>(value));
        }
        if (const auto *local = std::get_if<LocalFieldTarget>(&field.target))
        {
            const auto handle = scene && scene->Id() == local->entity.scene ?
                (replay ? scene->Find(field.stableEntity) : std::optional(local->entity)) : std::nullopt;
            const auto entity = handle ? scene->Get(*handle) : std::nullopt;
            if (!entity)
            {
                return Core::BoundedText("Transform edit belongs to an expired entity");
            }
            auto properties = entity->record.properties;
            if (local->field == TransformField::Rotation)
            {
                properties.local.rotation = std::get<std::array<double, 4>>(value);
            }
            else
            {
                auto &axis = local->field == TransformField::Scale ? properties.local.scale :
                    properties.local.translation;
                axis = std::get<std::array<double, 3>>(value);
            }
            const auto result = scene->Apply(Scene::EditProperties{*handle, std::move(properties)});
            if (const auto *error = std::get_if<Core::BoundedText>(&result))
            {
                return *error;
            }
            return std::nullopt;
        }
        const auto &navigation = std::get<NavigationTarget>(field.target);
        if (!scene || scene->Id() != navigation.scene || !view || view->Id() != navigation.view ||
            view->DocumentRequests().cameraEntityId)
        {
            return Core::BoundedText("Navigation edit requires the active navigation camera");
        }
        auto requests = view->DocumentRequests();
        requests.cameraPose = std::get<Scene::Pose>(value);
        auto update = view->PrepareDocumentUpdate(navigation.view, std::move(requests));
        if (const auto *error = std::get_if<Core::BoundedText>(&update))
        {
            return *error;
        }
        view->PublishDocumentUpdate(std::get<Renderer::ViewDocumentUpdate>(std::move(update)));
        return std::nullopt;
    }

    // 手势按旧项退役, 激活, 预览, 完成的顺序处理. 拒绝末值时仍可收束此前已接受的预览.
    std::optional<Core::BoundedText> EditHistory::ApplyGesture(Scene::Scene *scene, Renderer::View *view,
                                                              const FieldGesture &gesture)
    {
        if (m_Active && (gesture.began || m_Active->target != gesture.target))
        {
            if (auto error = CancelGesture(scene, view))
            {
                return error;
            }
        }
        if (gesture.began)
        {
            m_Active = BeginGesture(scene, view, gesture.target);
            if (!m_Active)
            {
                return Core::BoundedText("Edit gesture has no live target");
            }
        }
        if (!m_Active)
        {
            return std::nullopt;
        }
        std::optional<Core::BoundedText> rejected;
        if (gesture.preview && *gesture.preview != m_Active->after)
        {
            rejected = WriteField(scene, view, *m_Active, *gesture.preview, false);
            if (!rejected)
            {
                m_Active->after = *gesture.preview;
            }
        }
        if (gesture.finished)
        {
            auto completed = std::move(*std::exchange(m_Active, std::nullopt));
            if (completed.before != completed.after)
            {
                Commit(std::move(completed));
            }
        }
        return rejected;
    }

    // 两项可指向同一领域字段的不同轴. 先提交旧项, 让新项从实际结果捕获首值.
    std::optional<Core::BoundedText> EditHistory::ApplyGestures(Scene::Scene *scene, Renderer::View *view,
                                                               const FieldGestures &gestures)
    {
        const auto completed = gestures.completed ? ApplyGesture(scene, view, *gestures.completed) : std::nullopt;
        const auto current = ApplyGesture(scene, view, gestures.current);
        return completed ? completed : current;
    }

    // 先应用栈顶记录对应的字段值, 接受后才推进历史游标.
    std::optional<Core::BoundedText> EditHistory::Replay(Renderer::View *view, Scene::Scene *scene,
                                                        HistoryAction action, bool frozen)
    {
        const auto state = Snapshot(frozen);
        const bool undo = action == HistoryAction::Undo;
        if (undo ? !state.canUndo : !state.canRedo)
        {
            return std::nullopt;
        }
        const auto &entry = m_Entries[undo ? m_Applied - 1 : m_Applied];
        if (const auto *field = std::get_if<FieldChange>(&entry))
        {
            if (auto error = WriteField(scene, view, *field, undo ? field->before : field->after, true))
            {
                return error;
            }
        }
        else if (const auto *visibility = std::get_if<VisibilityChange>(&entry))
        {
            m_ViewportVisible = undo ? visibility->before : visibility->after;
        }
        else
        {
            const auto *sceneChange = std::get_if<SceneChange>(&entry);
            if (sceneChange && !scene)
            {
                return Core::BoundedText("Scene edit belongs to an expired document");
            }
            const auto *viewChange = sceneChange ?
                (sceneChange->cameraFallback ? &*sceneChange->cameraFallback : nullptr) : &std::get<ViewChange>(entry);
            std::optional<Renderer::ViewDocumentUpdate> viewUpdate;
            if (viewChange)
            {
                // 先准备可能失败的 View 更新. Scene 接受后才能无分配地发布, 避免撤销只恢复一个 owner.
                const auto &expected = undo ? viewChange->after : viewChange->before;
                if (!view || view->DocumentRequests() != expected)
                {
                    return Core::BoundedText("View history no longer matches its target");
                }
                auto update = view->PrepareDocumentUpdate(viewChange->view, undo ? viewChange->before :
                                                                                  viewChange->after);
                if (auto *error = std::get_if<Core::BoundedText>(&update))
                {
                    return *error;
                }
                viewUpdate = std::get<Renderer::ViewDocumentUpdate>(std::move(update));
            }
            if (sceneChange)
            {
                if (auto error = scene->Replay(sceneChange->change, undo))
                {
                    return error;
                }
            }
            if (viewUpdate)
            {
                view->PublishDocumentUpdate(std::move(*viewUpdate));
            }
        }
        if (undo)
        {
            --m_Applied;
        }
        else
        {
            ++m_Applied;
        }
        return std::nullopt;
    }

    std::optional<Core::BoundedText> EditHistory::Apply(EditSessionId session, Renderer::View *view,
                                                       std::span<const EditAction> actions,
                                                       Scene::Scene *scene, bool frozen)
    {
        if (!m_Id.IsValid() || session != m_Id)
        {
            return Core::BoundedText("Editor edit belongs to an expired session");
        }
        // 每项按届时的 owner 状态应用. 返回失败时, 前面已接受的字段和历史仍有效.
        for (const auto &action : actions)
        {
            if (const auto *visibility = std::get_if<SetViewportVisibility>(&action))
            {
                if (m_ViewportVisible != visibility->visible)
                {
                    Commit(VisibilityChange{m_ViewportVisible, visibility->visible});
                    m_ViewportVisible = visibility->visible;
                }
            }
            else if (const auto *scale = std::get_if<SetViewScale>(&action))
            {
                if (frozen)
                {
                    return Core::BoundedText("Document edits are frozen while opening");
                }
                if (m_Active)
                {
                    return Core::BoundedText("Render scale already has an active edit");
                }
                const auto before = view ? view->Settings().requestedScale : Renderer::MaxRenderScale;
                if (auto error = ApplyScale(view, scale->view, scale->scale))
                {
                    return error;
                }
                if (before != scale->scale)
                {
                    Commit(FieldChange{scale->view, {}, before, scale->scale});
                }
            }
            else if (const auto *erase = std::get_if<Scene::EraseSubtree>(&action))
            {
                if (frozen || !scene || !view)
                {
                    return Core::BoundedText("Delete requires the active, editable document");
                }
                if (auto error = ApplyScene(*scene, *view, *erase))
                {
                    return error;
                }
            }
            else if (auto error = Replay(view, scene, std::get<HistoryAction>(action), frozen))
            {
                return error;
            }
        }
        return std::nullopt;
    }

    // 先计划 Scene 记录与可能失效的相机引用, 再准备 View 回退, 最后提交两个 owner 并写历史.
    std::optional<Core::BoundedText> EditHistory::ApplyScene(Scene::Scene &scene, Renderer::View &view,
                                                            const Scene::Command &command)
    {
        if (m_Active)
        {
            return Core::BoundedText("Finish the active edit before changing the scene");
        }
        auto planned = scene.Plan(command);
        if (const auto *failure = std::get_if<Core::BoundedText>(&planned))
        {
            return *failure;
        }
        SceneChange entry{std::get<Scene::Change>(std::move(planned)), {}};
        if (entry.change.Empty())
        {
            return std::nullopt;
        }
        const auto &current = view.DocumentRequests();
        std::optional<Renderer::ViewDocumentUpdate> viewUpdate;
        if (current.cameraEntityId)
        {
            const auto &id = *current.cameraEntityId;
            const auto hasCamera = [&id](const Scene::EntityRecord &record)
            {
                return record.id == id && record.properties.camera.has_value();
            };
            if (std::ranges::any_of(entry.change.before, hasCamera) &&
                std::ranges::none_of(entry.change.after, hasCamera))
            {
                auto next = current;
                next.cameraEntityId.reset();
                auto update = view.PrepareDocumentUpdate(view.Id(), next);
                if (auto *error = std::get_if<Core::BoundedText>(&update))
                {
                    return *error;
                }
                viewUpdate = std::get<Renderer::ViewDocumentUpdate>(std::move(update));
                entry.cameraFallback = ViewChange{view.Id(), current, std::move(next)};
            }
        }
        if (auto failure = scene.Replay(entry.change, false))
        {
            return failure;
        }
        // Scene 已接受实际前后记录, 此后的 View 发布和历史移动都不再分配.
        if (viewUpdate)
        {
            view.PublishDocumentUpdate(std::move(*viewUpdate));
        }
        Commit(std::move(entry));
        return std::nullopt;
    }

    // 相机稳定 ID 必须属于当前 Scene. 保存完整请求前后值, 重放时核对未被其它入口改写.
    std::optional<Core::BoundedText> EditHistory::ApplyView(const Scene::Scene &scene, Renderer::View &view,
                                                           Renderer::ViewDocumentRequests value)
    {
        if (m_Active)
        {
            return Core::BoundedText("Finish the active edit before changing the main View");
        }
        if (value.cameraEntityId)
        {
            const auto entity = scene.Find(*value.cameraEntityId);
            if (!entity || !scene.Get(*entity)->record.properties.camera)
            {
                return Core::BoundedText("Main View requires a camera in the active scene");
            }
        }
        if (value == view.DocumentRequests())
        {
            return std::nullopt;
        }
        auto update = view.PrepareDocumentUpdate(view.Id(), value);
        if (const auto *error = std::get_if<Core::BoundedText>(&update))
        {
            return *error;
        }
        auto previous = view.DocumentRequests();
        view.PublishDocumentUpdate(std::get<Renderer::ViewDocumentUpdate>(std::move(update)));
        Commit(ViewChange{view.Id(), std::move(previous), std::move(value)});
        return std::nullopt;
    }
}
