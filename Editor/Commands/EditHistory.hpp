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

    using EditAction = std::variant<SetViewportVisibility, SetViewScale, HistoryAction, Scene::EraseSubtree>;

    /// @brief 本地 TRS 的字段身份. 一次编辑只写所选字段, 不回写面板读取的整份属性快照.
    enum class TransformField : std::uint8_t
    {
        Translation,
        Rotation,
        Scale
    };

    /// @brief 预览绑定原 Scene 与实体代; 只有已完成历史才允许通过稳定 ID 解析恢复后的实体.
    struct LocalFieldTarget
    {
        Scene::EntityHandle entity;
        TransformField field;
        bool operator==(const LocalFieldTarget &) const = default;
    };

    /// @brief 导航同时绑定文档与 View, 避免同一 View 在换文档后接收旧位姿.
    struct NavigationTarget
    {
        Scene::SceneId scene;
        Renderer::ViewId view;
        bool operator==(const NavigationTarget &) const = default;
    };

    /// @brief 空目标表示取消, ViewId 表示比例; 本地字段和导航各有独立身份.
    using FieldTarget = std::variant<std::monostate, Renderer::ViewId, LocalFieldTarget, NavigationTarget>;
    /// @brief 值类型与目标配对: 比例为 float, 平移/缩放为三分量, 旋转为 xyzw, 导航为 Pose.
    using FieldValue = std::variant<float, std::array<double, 3>, std::array<double, 4>, Scene::Pose>;

    /// @brief 当前批次的手势, 空目标表示活动项已消失或取消.
    /// @note 首次激活记录权威首值. 仅有 preview 时才修改 owner, finished 可与 began 同批出现.
    struct FieldGesture
    {
        FieldTarget target;
        std::optional<FieldValue> preview;
        bool began = false;
        bool finished = false;
    };

    /// @brief UI 封存的字段交接. 旧控件的末值和结束先于本轮活动项的启动或预览.
    /// @note 空批次取消尚未完成的手势. completed 仅延续上轮活动项.
    struct FieldGestures
    {
        std::optional<FieldGesture> completed;
        FieldGesture current;
    };

    /// @brief 菜单与面板使用的会话布局及历史启用快照.
    struct EditState
    {
        bool viewportVisible;
        bool canUndo;
        bool canRedo;
    };

    /// @brief 管理主视口显隐, 单活动字段事务及 Scene/View 的有界历史. 活动事务期间暂停历史重放.
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
        [[nodiscard]] bool HasGesture() const noexcept { return m_Active.has_value(); }
        /// @brief 退役旧文档的历史, 固定资产引用及活动手势, 保留会话实际视口显隐.
        void Clear() noexcept;
        /// @brief 先释放活动记录, 再通过原 owner 恢复首值. 目标退役时不向新目标写入, 返回拒绝原因.
        [[nodiscard]] std::optional<Core::BoundedText> CancelGesture(Scene::Scene *scene, Renderer::View *view);
        /// @brief 应用本轮手势的末值, 完成时只为净变化写一条历史. 空目标取消尚未完成的手势.
        /// @note 新 began 或不同目标会取消旧预览, 不等同于正常完成旧手势. 失败预览保留最近接受值.
        [[nodiscard]] std::optional<Core::BoundedText> ApplyGesture(Scene::Scene *scene, Renderer::View *view,
                                                                    const FieldGesture &gesture);
        /// @brief 先完成旧手势, 再从已接受的实际值开始新手势. 返回本轮首个拒绝原因.
        /// @note 旧项末值被拒绝时仍按此前接受值结束, 随后尝试当前项, 不回滚已成功完成的编辑.
        [[nodiscard]] std::optional<Core::BoundedText> ApplyGestures(Scene::Scene *scene, Renderer::View *view,
                                                                     const FieldGestures &gestures);

        /// @brief 按 Scene 的真实事务准备历史, 同步失效的主 View 相机引用.
        /// @note Scene 提交前已准备好相机回退和历史存储, 拒绝不会留下半个跨 owner 事务.
        [[nodiscard]] std::optional<Core::BoundedText>
            ApplyScene(Scene::Scene &scene, Renderer::View &view, const Scene::Command &command);
        /// @brief 校验场景相机引用并提交主 View 请求, 按完成顺序加入历史.
        [[nodiscard]] std::optional<Core::BoundedText>
            ApplyView(const Scene::Scene &scene, Renderer::View &view, Renderer::ViewDocumentRequests value);

        /// @brief 封存 UI 后按序消费本会话的离散和重放动作.
        /// @param view 本会话当轮借用的主 View. 空指针时比例修改返回拒绝原因.
        /// @return 首个拒绝原因, 已应用的有效前缀保留. 会话身份失配时整批拒绝.
        [[nodiscard]] std::optional<Core::BoundedText> Apply(EditSessionId session, Renderer::View *view,
                                                             std::span<const EditAction> actions,
                                                             Scene::Scene *scene = nullptr, bool frozen = false);

    private:
        /// @brief 一个字段的首末值. 预览校验 live handle, 历史按同文档稳定 ID 解析恢复后的实体.
        struct FieldChange
        {
            FieldTarget target;
            Scene::EntityId stableEntity;
            FieldValue before;
            FieldValue after;
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

        using Entry = std::variant<VisibilityChange, FieldChange, ViewChange, SceneChange>;

        /// @brief 接收已成功应用的净变化, 截断 Redo; 容量已满时退休最早记录和它的资产引用.
        void Commit(Entry entry) noexcept;
        /// @brief 解析当前字段首值. 目标不存活或导航仍绑定实体相机时返回空.
        [[nodiscard]] static std::optional<FieldChange> BeginGesture(Scene::Scene *scene, Renderer::View *view,
                                                                      const FieldTarget &target);
        /// @brief 经原领域入口写单字段. replay 仅放宽本地实体的运行期代, 仍要求同一 Scene 和稳定 ID.
        [[nodiscard]] static std::optional<Core::BoundedText> WriteField(Scene::Scene *scene, Renderer::View *view,
            const FieldChange &field, const FieldValue &value, bool replay);
        [[nodiscard]] std::optional<Core::BoundedText> Replay(Renderer::View *view, Scene::Scene *scene,
                                                              HistoryAction action, bool frozen);

        EditSessionId m_Id;
        /// @brief 已应用记录位于 [0, m_Applied), 待重做记录位于 [m_Applied, m_Count).
        std::array<Entry, Capacity> m_Entries{};
        std::size_t m_Count = 0;
        std::size_t m_Applied = 0;
        std::optional<FieldChange> m_Active;
        bool m_ViewportVisible = true;
    };
}
