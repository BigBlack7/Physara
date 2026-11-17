#pragma once

/// @file
/// @brief 层级面板缓存只读树投影, Inspector 只保留活动旋转草稿.
/// 选择、字段手势及相机引用在会话编辑边界提交.

#include <array>
#include <optional>
#include <vector>

#include "Commands/EditHistory.hpp"

namespace Physara::Editor::Detail
{
    struct WorkspaceFrame;

    /// @brief 只拥有层级展示投影和旋转输入草稿, 不拥有可写 Scene 或编辑历史.
    /// @note 在当前 ImGui 帧同步借用领域数据, 返回的意图由 Session 封存后应用.
    class ScenePanels final
    {
    public:
        /// @brief 显示活动文档的可见层级行, 按存活实体身份生成选择意图.
        void DrawHierarchy(const Scene::Scene *scene, const std::optional<Scene::EntityHandle> &selection,
                           bool allowActions, WorkspaceFrame &frame);
        /// @brief 同步借用所选实体并生成局部 TRS 手势, 不保留 Scene 记录.
        /// @note 平移使用米, 旋转控件使用 XYZ 欧拉角的度数, 提交为 xyzw 单位四元数.
        /// 非有限数和传播失败由 Scene 拒绝, 后续展示重新读取已接受值.
        void DrawInspector(const Scene::Scene *scene, const std::optional<Scene::EntityHandle> &selection,
                           bool allowEdits, WorkspaceFrame &frame);
        /// @brief 只从当前 Scene 枚举相机来源, 选择时提交稳定 ID.
        void DrawCamera(const Scene::Scene *scene, const Renderer::View &view, bool allowEdits, WorkspaceFrame &frame);
        void EndRotationInput() noexcept { m_Rotation.reset(); }

    private:
        /// @brief revision 驱动的展示节点. children 仅索引同一批 m_Nodes, 不作为领域层级的写入口.
        struct Node
        {
            Scene::EntityHandle handle;
            Scene::EntityId id;
            std::string name;
            std::vector<std::size_t> children;
            bool expanded = true;
        };

        struct Row
        {
            std::size_t node;
            std::size_t depth;
        };

        /// @brief 仅在旋转控件活跃时保存度数, 避免四元数往返转换导致欧拉分支跳变.
        struct RotationDraft
        {
            Scene::EntityHandle entity;
            std::array<double, 3> angles;
        };

        void RefreshHierarchy(const Scene::Scene &scene);
        void RebuildVisibleRows();

        Scene::SceneId m_Scene;
        std::uint64_t m_Revision{};
        std::vector<Node> m_Nodes;
        std::vector<std::size_t> m_Roots;
        std::vector<Row> m_Rows;
        bool m_RowsDirty = false;
        std::optional<RotationDraft> m_Rotation;
    };
}
