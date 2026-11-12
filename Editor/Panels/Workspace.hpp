#pragma once

/// @file
/// @brief 提交固定面板和停靠布局, 由只读快照构建字段编辑及窗口意图.
/// Workspace 持有面板展示状态和标题草稿, 会话在 UI 封存后消费编辑意图.

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <DeviceHost.hpp>

#include "Commands/EditHistory.hpp"
#include "Viewport/Viewport.hpp"

namespace Physara::Editor::Detail
{
    struct InputClaims;
    struct SelectionIdentityTag;
    using SelectionId = Core::Identity<SelectionIdentityTag>;

    /// @brief 固定面板的显隐动作. 前两项的序号同时索引本地展示状态.
    enum class ActionId : std::uint8_t
    {
        ToggleHierarchy,
        ToggleInspector,
        ToggleViewport
    };

    /// @brief 当前 UI 批次的布局及值意图. 会话封存绘制数据后消费.
    /// @note 视口图像借用当轮 ImGui 列表及字体. 编辑和窗口请求由本值持有.
    struct WorkspaceFrame
    {
        ViewportLayout viewport;
        ScaleGesture scale;
        std::vector<EditAction> edits;
        std::optional<Platform::SetWindowTitle> title;
        std::optional<Platform::SetWindowVSync> vsync;
    };

    /// @brief 管理主窗口停靠和展示状态. 主视口显隐及比例由领域快照提供.
    class Workspace final
    {
    public:
        /// @brief 在当前 ImGui 帧构建菜单, 控件及可见面板.
        /// @param window 当前宿主设置的短期借用. 空值时省略窗口设置菜单.
        /// @param allowActions 输入可用时才收集动作. 输入失效轮次仍可构建布局.
        [[nodiscard]] WorkspaceFrame Draw(const std::optional<SelectionId> &selection, const Viewport &viewport,
                                            EditState edits, const Renderer::ViewSettings &view,
                                            const Platform::WindowSettings *window,
                                            const std::optional<Core::BoundedText> &editError, bool allowActions);

        /// @brief 依到达顺序从当轮有资格的 Press 生成历史或布局动作.
        /// @note 每条历史动作在会话重放时按届时的游标判断可用性.
        void ApplyShortcuts(std::span<const Platform::InputEvent> events, const InputClaims &claims,
                            WorkspaceFrame &frame);

    private:
        [[nodiscard]] bool Visible(ActionId action, EditState edits) const noexcept;
        void SetVisible(ActionId action, bool visible, WorkspaceFrame &frame);
        void DrawWindowMenu(const Platform::WindowSettings *window, WorkspaceFrame &frame, bool allowActions);

        std::array<bool, 2> m_Visible{true, true}; ///< Hierarchy 与 Inspector 的展示状态.
        /// @brief 控件激活期间保留 UTF-8 草稿. 退出编辑后重新从宿主快照取值.
        std::array<char, Platform::MaxWindowTitleBytes + 1> m_TitleDraft{};
        bool m_TitleEditing = false;
    };
}
