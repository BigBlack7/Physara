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
#include "DocumentPanel.hpp"
#include "ScenePanels.hpp"
#include "Session/SceneDocumentTypes.hpp"
#include "Session/UiFrame.hpp"
#include "Viewport/Viewport.hpp"

namespace Physara::Editor::Detail
{
    struct InputClaims;

    /// @brief 菜单及快捷键的共享动作. 前三项的序号同时索引本地面板展示状态.
    enum class ActionId : std::uint8_t
    {
        ToggleHierarchy,
        ToggleInspector,
        ToggleViewSettings,
        ToggleViewport,
        NewScene,
        OpenScene,
        SaveScene,
        SaveSceneAs,
        ImportModel,
        DeleteSelection,
        Undo,
        Redo
    };

    /// @brief 相机来源意图只修改 View 的实体引用, 保留该 View 已保存的导航位姿.
    struct CameraSource
    {
        std::optional<Scene::EntityId> entity; ///< 空引用选择导航相机.
    };

    /// @brief 一个物理相机字段的离散提交. 值已经换成 View 请求使用的领域单位.
    /// field 由面板选取非空成员指针, Session 只合并这一字段, 其余值在应用时读取当前 View.
    struct PhysicalCameraEdit
    {
        using Field = double Renderer::PhysicalCameraRequest::*;
        Field field;
        double value;
    };

    /// @brief 当前 UI 批次的布局及值意图. 会话封存绘制数据后消费.
    /// @note 视口图像借用当轮 ImGui 列表及字体. 编辑和窗口请求由本值持有.
    /// 相机字段意图只在本批当前文档的编辑边界消费, 先于可替换 Scene 的文件操作.
    struct WorkspaceFrame
    {
        ViewportLayout viewport;
        FieldGestures gestures;
        std::optional<Scene::EntityHandle> selection;
        std::optional<CameraSource> camera;
        std::optional<PhysicalCameraEdit> physicalCamera;
        std::optional<Renderer::AntiAliasing> antiAliasing; ///< 持久化意图, 走主 View 文档事务.
        std::optional<Renderer::PreviewMode> preview; ///< 会话内显示意图, 不进入持久化文档或撤销历史.
        std::vector<EditAction> edits;
        std::optional<Platform::SetWindowTitle> title;
        std::optional<Platform::SetWindowVSync> vsync;
        std::optional<std::uint64_t> cancelDocument; ///< 本轮看到的文件操作编号, 会话接受时再次核对资格.
        std::optional<FileIntent> file;
        std::optional<ImportIntent> import;
        std::optional<std::uint64_t> cancelImport;
    };

    /// @brief 本次布局及动作路由使用的只读借用. 会话在 UI 封存后才修改领域 owner.
    struct WorkspaceState
    {
        const Scene::Scene *scene;
        std::optional<Scene::EntityHandle> selection;
        const Renderer::View *view;
        EditState edits;
        const Platform::WindowSettings *window;
        const SceneDocumentStatus *document;
        const SceneImportStatus &model;
        const std::optional<Core::BoundedText> &editError;
        const std::optional<Core::BoundedText> &fileError;
        const std::optional<FrameDiagnostic> &frameDiagnostic;
        bool allowActions;
        bool frozen;
        bool navigating;
    };

    /// @brief 管理主窗口停靠和展示状态. 主视口显隐及比例由领域快照提供.
    /// @note Draw 只收集意图; ApplyShortcut 沿同一动作表和启用规则追加当轮意图.
    class Workspace final
    {
    public:
        /// @brief 在当前 ImGui 帧构建菜单, 控件及可见面板.
        [[nodiscard]] WorkspaceFrame Draw(const WorkspaceState &state, const Viewport &viewport);

        /// @brief 从当轮有资格的 Press 生成动作. 会话依发布顺序调用并应用意图.
        void ApplyShortcut(const Platform::InputEvent &event, const InputClaims &claims, const WorkspaceState &state,
                           WorkspaceFrame &frame);
        void EndRotationInput() noexcept { m_ScenePanels.EndRotationInput(); }
        [[nodiscard]] bool HasFileDialog() const noexcept { return m_Documents.HasDialog(); }

    private:
        [[nodiscard]] bool Visible(ActionId action, EditState edits) const noexcept;
        void SetVisible(ActionId action, bool visible, WorkspaceFrame &frame);
        [[nodiscard]] static bool CanActivate(ActionId action, const WorkspaceState &state) noexcept;
        void Activate(ActionId action, const WorkspaceState &state, WorkspaceFrame &frame);
        void DrawActions(const WorkspaceState &state, WorkspaceFrame &frame);
        void DrawWindowMenu(const Platform::WindowSettings *window, WorkspaceFrame &frame, bool allowActions);

        std::array<bool, 3> m_Visible{true, true, true}; ///< Hierarchy, Inspector 和 View 面板的展示状态.
        ScenePanels m_ScenePanels;
        DocumentPanel m_Documents;
        /// @brief 控件激活期间保留 UTF-8 草稿. 退出编辑后重新从宿主快照取值.
        std::array<char, Platform::MaxWindowTitleBytes + 1> m_TitleDraft{};
        bool m_TitleEditing = false;
    };
}
