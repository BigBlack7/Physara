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
#include "Session/SceneDocumentTypes.hpp"
#include "Session/UiFrame.hpp"
#include "Viewport/Viewport.hpp"

namespace Physara::Editor::Detail
{
    struct InputClaims;

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
        std::optional<Renderer::PreviewMode> preview; ///< 会话内显示意图, 不进入持久化文档或撤销历史.
        std::vector<EditAction> edits;
        std::optional<Platform::SetWindowTitle> title;
        std::optional<Platform::SetWindowVSync> vsync;
        std::optional<std::uint64_t> cancelDocument; ///< 本轮看到的文件操作编号, 会话接受时再次核对资格.
    };

    /// @brief 管理主窗口停靠和展示状态. 主视口显隐及比例由领域快照提供.
    class Workspace final
    {
    public:
        /// @brief 在当前 ImGui 帧构建菜单, 控件及可见面板.
        /// @param acceptedPreview 最近接受的显示色所关联的场景预览, 仅在本次布局中借用.
        /// @param window 当前宿主设置的短期借用. 空值时省略窗口设置菜单.
        /// @param frameDiagnostic 会话拥有的最近失败及恢复状态, 本轮只读, 与 Inspector 显隐独立.
        /// @param allowActions 输入可用时才收集动作. 输入失效轮次仍可构建布局.
        /// @param document 单文档进度快照, 本面板只展示和取消, 不读取文件或接管 Scene.
        /// @param frozen 禁止持久化比例编辑, 窗口与布局动作仍按各自合同收集.
        [[nodiscard]] WorkspaceFrame Draw(const std::optional<Scene::EntityHandle> &selection, const Viewport &viewport,
                                            EditState edits, const Renderer::ViewSettings &view,
                                            const Renderer::RenderedScene *acceptedPreview,
                                            const Platform::WindowSettings *window,
                                            const std::optional<Core::BoundedText> &editError,
                                            const std::optional<FrameDiagnostic> &frameDiagnostic, bool allowActions,
                                            const std::optional<SceneDocumentStatus> &document, bool frozen);

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
