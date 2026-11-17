#pragma once

/// @file
/// @brief EditorSession 的单文档协调器. 文件工作与活动 Scene/View 隔离.

#include "SceneDocumentTypes.hpp"

#include <memory>

#include <Renderer/View.hpp>

namespace Physara::Editor::Detail
{
    class EditHistory;

    /// @brief owner 线程的单文档状态机. 只允许一个文件操作, worker 不访问活动 Scene/View/历史.
    class SceneDocuments final
    {
    public:
        /// @brief 接管活动 Scene, 绑定主 View 身份并以当前版本建立初始保存基线.
        /// @pre scene 非空, root 为项目资源根; View 仍由组合调用者持有.
        SceneDocuments(std::filesystem::path root, std::unique_ptr<Scene::Scene> scene, const Renderer::View &view);
        ~SceneDocuments();
        SceneDocuments(const SceneDocuments &) = delete;
        SceneDocuments &operator=(const SceneDocuments &) = delete;

        [[nodiscard]] Scene::Scene &Active() noexcept;
        [[nodiscard]] Renderer::ViewId MainView() const noexcept;
        /// @brief 文档会话已停止接收操作及发布结果.
        [[nodiscard]] bool IsStopped() const noexcept;
        /// @brief Open 的准备阶段及关闭后冻结文档编辑, Save 和已接受取消后的收尾不冻结.
        [[nodiscard]] bool Frozen() const noexcept;
        [[nodiscard]] SceneDocumentStatus Status(const Renderer::View &view) const;

        /// @brief 在编辑边界接受文件意图或返回原因. New 当场提交, 其余由 Advance 推进.
        /// @note New/Open 先取消活动字段预览, 再核对 discard 是否仍匹配当前持久化版本.
        /// Save 捕获自持快照后允许继续编辑, 当前路径和保存基线只随成功结果更新.
        [[nodiscard]] std::optional<Core::BoundedText> Begin(const SceneFileRequest &request, Renderer::View &view,
                                                           EditHistory &history);
        /// @brief 将替换意图附到本次保存. 只在同一操作成功且当前版本仍等于保存版本时继续.
        /// @note 保存阶段仍可编辑. 失败, 取消或保存期间版本改变时丢弃继续意图, 保留活动文档.
        [[nodiscard]] std::optional<Core::BoundedText> SaveBeforeReplacing(SceneFileRequest replacement,
            const std::filesystem::path &saveAs, Renderer::View &view, EditHistory &history);
        /// @brief 取消指定操作的发布资格. 已进入文件替换时拒绝取消, 等待实际结果.
        /// @note 等资产阶段立即结束, 文件 worker 仍占槽时进入 Cancelling; 不取消共享资产作业.
        [[nodiscard]] std::optional<Core::BoundedText> Cancel(std::uint64_t operation) noexcept;
        /// @brief 资产完成发布后非阻塞取回文件结果, 跟进准入或整体安装候选, 与 GPU 槽无关.
        /// @note 只推进匹配的主 View 和当前操作; 失败记入 Status 并保留原活动文档.
        void Advance(Asset::AssetManager &assets, Renderer::View &view, EditHistory &history);
        /// @brief 停止新操作及发布, 收拢文件工作. 活动 Scene 由会话继续持有至销毁.
        void Shutdown() noexcept;

    private:
        struct State;
        std::unique_ptr<State> m_State;
    };
}
