#include "EditHistory.hpp"

/// @file
/// @brief 在 CPU 帧边界应用比例手势, 离散编辑和历史重放.
/// 活动手势保存首值和最近接受值, 完成的实际修改才进入有界历史.

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

    EditState EditHistory::Snapshot() const noexcept
    {
        return {m_ViewportVisible, !m_Active && m_Applied != 0, !m_Active && m_Applied < m_Count};
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
        // 只在新编辑完成时截断 Redo. 满时移除最早的一项, 值不拥有外部资源.
        if (m_Applied == Capacity)
        {
            std::move(m_Entries.begin() + 1, m_Entries.end(), m_Entries.begin());
            --m_Applied;
        }
        m_Entries[m_Applied++] = std::move(entry);
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
    std::optional<Core::BoundedText> EditHistory::Replay(Renderer::View *view, HistoryAction action) noexcept
    {
        const auto state = Snapshot();
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
        else
        {
            const auto &visibility = std::get<VisibilityChange>(entry);
            m_ViewportVisible = undo ? visibility.before : visibility.after;
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
                                                       std::span<const EditAction> actions) noexcept
    {
        if (!m_Id.IsValid() || session != m_Id)
        {
            return Core::BoundedText("Editor edit belongs to an expired session");
        }
        if (auto error = ApplyGesture(view, gesture))
        {
            return error;
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
            else if (auto error = Replay(view, std::get<HistoryAction>(action)))
            {
                return error;
            }
        }
        return std::nullopt;
    }
}
