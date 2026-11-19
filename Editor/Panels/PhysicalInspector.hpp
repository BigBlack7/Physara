#pragma once

/// @file
/// @brief 用活动场景及资源请求快照构建物理 Inspector. 控件只向本轮追加领域意图.

#include "Commands/PhysicalEdits.hpp"

namespace Physara::Editor::Detail
{
    struct WorkspaceFrame;

    /// @brief 环境和来源状态在无实体选择时也可用. 材质槽只在当前实体的源网格上展开.
    /// @note 调用期间借用 Scene 和资源状态. 确认的字段和路径随 frame 返回, 由会话封存布局后应用.
    void DrawPhysicalInspector(const Scene::Scene &scene, std::optional<Scene::EntityHandle> selected,
                               const PhysicalResourceStatus &resource, bool allowEdits, WorkspaceFrame &frame);
}
