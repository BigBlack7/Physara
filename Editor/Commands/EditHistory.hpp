#pragma once

/// @file
/// @brief 会话的布局、View 字段和场景事务历史. 同一游标按接受顺序回放.
/// 文档替换退休旧记录, 布局值继续由会话持有.

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

    /// @brief 管理主视口显隐, 单活动比例事务及 Scene/View 的有界历史. 活动事务期间暂停历史重放.
    /// @note 在会话 owner 线程使用. Apply 只在本次调用期间借用 View 及动作序列.
    class EditHistory final
    {
    public:
        static constexpr std::size_t Capacity = 64;

        EditHistory() noexcept;
        EditHistory(const EditHistory &) = delete;
        EditHistory &operator=(const EditHistory &) = delete;

        [[nodiscard]] EditSessionId Id() const noexcept { return m_Id; }
        [[nodiscard]] EditState Snapshot(bool frozen = false) const noexcept;
        /// @brief 退役旧文档的历史, 固定资产引用及活动手势, 保留会话实际视口显隐.
        void Clear() noexcept;
        /// @brief 通过 View 入口恢复比例手势首值并结束手势, 返回恢复请求的拒绝原因.
        [[nodiscard]] std::optional<Core::BoundedText> CancelGesture(Renderer::View &view) noexcept;

        /// @brief 按 Scene 的真实事务准备历史, 同步失效的主 View 相机引用.
        /// @note Scene 提交前已准备好相机回退和历史存储, 拒绝不会留下半个跨 owner 事务.
        [[nodiscard]] std::optional<Core::BoundedText>
            ApplyScene(Scene::Scene &scene, Renderer::View &view, const Scene::Command &command);
        /// @brief 校验场景相机引用并提交主 View 请求, 按完成顺序加入历史.
        [[nodiscard]] std::optional<Core::BoundedText>
            ApplyView(const Scene::Scene &scene, Renderer::View &view, Renderer::ViewDocumentRequests value);

        /// @brief 封存 UI 后消费本会话的一批意图. 先处理连续手势, 再按序处理离散及重放动作.
        /// @param view 本会话当轮借用的主 View. 空指针时比例修改返回拒绝原因.
        /// @return 首个拒绝原因, 已应用的有效前缀保留. 会话身份失配时整批拒绝.
        [[nodiscard]] std::optional<Core::BoundedText> Apply(EditSessionId session, Renderer::View *view,
                                                             const ScaleGesture &gesture,
                                                             std::span<const EditAction> actions,
                                                             Scene::Scene *scene = nullptr, bool frozen = false);

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

        /// @brief 完整主 View 请求的前后值, 包括导航位姿, 相机引用和请求比例.
        struct ViewChange
        {
            Renderer::ViewId view;
            Renderer::ViewDocumentRequests before;
            Renderer::ViewDocumentRequests after;
        };

        /// @brief 只保有所改实体的记录和资产; 删除相机引起的 View 回退随本条一起重放.
        struct SceneChange
        {
            Scene::Change change;
            std::optional<ViewChange> cameraFallback;
        };

        using Entry = std::variant<VisibilityChange, ScaleChange, ViewChange, SceneChange>;

        void Commit(Entry entry) noexcept;
        [[nodiscard]] std::optional<Core::BoundedText> ApplyGesture(Renderer::View *view,
                                                                    const ScaleGesture &gesture) noexcept;
        [[nodiscard]] std::optional<Core::BoundedText> Replay(Renderer::View *view, Scene::Scene *scene,
                                                              HistoryAction action, bool frozen);

        EditSessionId m_Id;
        /// @brief 已应用记录位于 [0, m_Applied), 待重做记录位于 [m_Applied, m_Count).
        std::array<Entry, Capacity> m_Entries{};
        std::size_t m_Count = 0;
        std::size_t m_Applied = 0;
        std::optional<ScaleChange> m_Active;
        bool m_ViewportVisible = true;
    };
}
