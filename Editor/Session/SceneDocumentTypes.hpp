#pragma once

/// @file
/// @brief 编辑器单文档的操作和状态值. 主线程使用, 可由后续编辑器动作直接消费.

#include <filesystem>
#include <optional>

#include <Scene/Scene.hpp>

namespace Physara::Editor
{
    enum class SceneFileAction : std::uint8_t
    {
        New,
        Open,
        Save,
        SaveAs
    };

    /// @brief 文件操作的可观察阶段. Cancelling 表示已撤销发布资格, 正在回收仍占槽的文件作业.
    enum class SceneFilePhase : std::uint8_t
    {
        Idle,
        Reading,
        LoadingAssets,
        Validating,
        Saving,
        Cancelling
    };

    /// @brief 标识当前文档及两个持久化 owner 的已观察 revision.
    struct SceneDocumentVersion
    {
        Scene::SceneId scene;
        std::uint64_t document{};
        std::uint64_t view{};

        bool operator==(const SceneDocumentVersion &) const = default;
    };

    /// @brief 绑定当前文档身份的文件意图. discard 是对指定版本的放弃授权, 不预先清除原内容.
    struct SceneFileRequest
    {
        SceneFileAction action;
        Scene::SceneId scene;
        std::filesystem::path path; ///< Open 或 Save As 的项目相对路径.
        std::optional<SceneDocumentVersion> discard; ///< 只放弃该版本, 更晚编辑需要重新处理.
    };

    /// @brief 本轮文件操作和未保存状态. 空路径表示尚未保存的内存文档.
    struct SceneDocumentStatus
    {
        SceneDocumentVersion version;
        std::optional<std::filesystem::path> path;
        SceneFilePhase phase = SceneFilePhase::Idle;
        std::uint64_t operation{};
        bool modified{};
        std::optional<Core::BoundedText> lastFailure;
    };
}
