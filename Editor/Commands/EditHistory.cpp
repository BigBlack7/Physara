#include "EditHistory.hpp"

/// @file
/// @brief 在 CPU 帧边界应用布局, 比例手势, Scene/View 事务及其历史重放.
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

    std::optional<Core::BoundedText> EditHistory::CancelGesture(Renderer::View &view) noexcept
    {
        ScaleGesture cancelled{};
        cancelled.cancelled = true;
        return ApplyGesture(&view, cancelled);
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

    // 手势按取消, 激活, 预览, 完成的顺序处理. 记录采用实际被 View 接受的值.
    std::optional<Core::BoundedText> EditHistory::ApplyGesture(Renderer::View *view,
                                                              const ScaleGesture &gesture) noexcept
    {
        if (m_Active && (gesture.cancelled || (!gesture.active && !gesture.finished)))
        {
            // 先结束交互再尝试恢复首值, 迟到的预览或释放不会提交刚结束的事务.
            const auto cancelled = *std::exchange(m_Active, std::nullopt);
            return ApplyScale(view, cancelled.view, cancelled.before);
        }
        if (gesture.cancelled)
        {
            return std::nullopt;
        }
        if (gesture.began)
        {
            if (m_Active || !view || view->Id() != gesture.view)
            {
                return Core::BoundedText("Render scale gesture requires the active View and an idle field");
            }
            // 捕获进入拖拽时的权威请求, 连续预览只更新末值.
            const auto before = view->Settings().requestedScale;
            m_Active = ScaleChange{gesture.view, before, before};
        }
        if (!m_Active)
        {
            return std::nullopt;
        }
        if (gesture.view != m_Active->view)
        {
            return Core::BoundedText("Render scale preview belongs to another View");
        }
        if (gesture.preview)
        {
            if (auto error = ApplyScale(view, m_Active->view, *gesture.preview))
            {
                return error;
            }
            m_Active->after = *gesture.preview;
        }
        if (gesture.finished)
        {
            const auto completed = *std::exchange(m_Active, std::nullopt);
            // 往返到首值时没有需要重放的变化, 保留已有 Redo 分支.
            if (completed.before != completed.after)
            {
                Commit(completed);
            }
        }
        return std::nullopt;
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
        if (const auto *scale = std::get_if<ScaleChange>(&entry))
        {
            if (auto error = ApplyScale(view, scale->view, undo ? scale->before : scale->after))
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
                                                       const ScaleGesture &gesture,
                                                       std::span<const EditAction> actions,
                                                       Scene::Scene *scene, bool frozen)
    {
        if (!m_Id.IsValid() || session != m_Id)
        {
            return Core::BoundedText("Editor edit belongs to an expired session");
        }
        if (!frozen)
        {
            if (auto error = ApplyGesture(view, gesture))
            {
                return error;
            }
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
                    Commit(ScaleChange{scale->view, before, scale->scale});
                }
            }
            else if (auto error = Replay(view, scene, std::get<HistoryAction>(action), frozen))
            {
                return error;
            }
        }
        return std::nullopt;
    }

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
