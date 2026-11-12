#pragma once

/// @file
/// @brief 会话的字段编辑与历史. 布局显隐由会话持有, 比例经 View 的字段入口应用.
/// 历史保存值和目标身份, 生命周期与 EditorSession 一致.

#include <array>
#include <optional>
#include <span>
#include <variant>

#include <Renderer/View.hpp>

namespace Physara::Editor::Detail
{
    struct EditSessionTag;
    /// @brief 会话的创建代. 同一宿主重新接入编辑器时获得新的编辑身份.
    using EditSessionId = Core::Identity<EditSessionTag>;

    /// @brief 主 View 的离散比例意图. 比例无单位, 由 View 校验身份和值域.
    struct SetViewScale
    {
        Renderer::ViewId view;
        float scale;
    };

    /// @brief 当前会话主视口的显隐意图, 目标由 Apply 的会话身份确定.
    struct SetViewportVisibility
    {
        bool visible;
    };

    enum class HistoryAction : std::uint8_t
    {
        Undo,
        Redo
    };

    using EditAction = std::variant<SetViewportVisibility, SetViewScale, HistoryAction>;

    /// @brief 当前 UI 帧提取的比例手势. began 标记激活边沿, preview 保存控件返回的候选值.
    /// @note finished 标记正常结束, cancelled 优先结束事务. 活动项消失也可触发取消.
    struct ScaleGesture
    {
        Renderer::ViewId view;
        std::optional<float> preview;
        bool began = false;
        bool active = false;
        bool finished = false;
        bool cancelled = false;
    };

    /// @brief 菜单与面板使用的会话布局及历史启用快照.
    struct EditState
    {
        bool viewportVisible;
        bool canUndo;
        bool canRedo;
    };

    /// @brief 管理主视口显隐, 单活动比例事务和有界历史. 活动事务期间暂停历史重放.
    /// @note 在会话 owner 线程使用. Apply 只在本次调用期间借用 View 及动作序列.
    class EditHistory final
    {
    public:
        static constexpr std::size_t Capacity = 64;

        EditHistory() noexcept;
        EditHistory(const EditHistory &) = delete;
        EditHistory &operator=(const EditHistory &) = delete;

        [[nodiscard]] EditSessionId Id() const noexcept { return m_Id; }
        [[nodiscard]] EditState Snapshot() const noexcept;

        /// @brief 封存 UI 后消费本会话的一批意图. 先处理连续手势, 再按序处理离散及重放动作.
        /// @param view 本会话当轮借用的主 View. 空指针时比例修改返回拒绝原因.
        /// @return 首个拒绝原因, 已应用的有效前缀保留. 会话身份失配时整批拒绝.
        [[nodiscard]] std::optional<Core::BoundedText> Apply(EditSessionId session, Renderer::View *view,
                                                             const ScaleGesture &gesture,
                                                             std::span<const EditAction> actions) noexcept;

    private:
        struct ScaleChange
        {
            Renderer::ViewId view;
            float before;
            float after;
        };

        struct VisibilityChange
        {
            bool before;
            bool after;
        };

        using Entry = std::variant<VisibilityChange, ScaleChange>;

        void Commit(Entry entry) noexcept;
        [[nodiscard]] std::optional<Core::BoundedText> ApplyGesture(Renderer::View *view,
                                                                    const ScaleGesture &gesture) noexcept;
        [[nodiscard]] std::optional<Core::BoundedText> Replay(Renderer::View *view, HistoryAction action) noexcept;

        EditSessionId m_Id;
        /// @brief 已应用记录位于 [0, m_Applied), 待重做记录位于 [m_Applied, m_Count).
        std::array<Entry, Capacity> m_Entries{};
        std::size_t m_Count = 0;
        std::size_t m_Applied = 0;
        std::optional<ScaleChange> m_Active;
        bool m_ViewportVisible = true;
    };
}
