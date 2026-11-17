#pragma once

/// @file
/// @brief 文件与模型导入菜单的路径草稿及未保存操作选择. 文件工作由 Session 独立推进.

#include <array>
#include <string>

#include "Session/SceneDocumentTypes.hpp"

namespace Physara::Editor::Detail
{
    struct WorkspaceFrame;

    /// @brief 未保存提示的处置选择; None 不携带放弃授权, Save 仍需等待实际保存结果.
    enum class ReplaceDecision : std::uint8_t
    {
        None,
        Discard,
        Save
    };

    /// @brief 文件确认时看到的文档身份和版本, 文本在应用边界转换为项目相对路径.
    struct FileIntent
    {
        SceneDocumentVersion version;
        SceneFileAction action;
        ReplaceDecision decision = ReplaceDecision::None;
        std::string path;
        std::string savePath;
    };

    /// @brief 用户确认的模型路径与源 scene 草稿. 下标解析和资产准入由 Session/Asset 负责.
    struct ImportIntent
    {
        Scene::SceneId scene;
        std::string path;
        std::string sourceScene; ///< 可空的十进制 glTF scene 下标.
    };

    /// @brief 单个文件/导入模态的展示 owner. 草稿绑定 Scene, 不直接发起 I/O 或清除 dirty.
    class DocumentPanel final
    {
    public:
        /// @brief 在确认无需额外输入时直接产生文件意图, 其它动作先创建路径或确认草稿.
        void Request(SceneFileAction action, const SceneDocumentStatus &document, WorkspaceFrame &frame);
        /// @brief 为当前文档打开模型路径草稿, 不取消已有共享资产工作.
        void RequestImport(Scene::SceneId scene);
        /// @brief 显示当前文档、在途操作及导入诊断, 为当前操作号提供取消入口.
        void DrawStatus(const SceneDocumentStatus &document, const SceneImportStatus &model,
                        const std::optional<Core::BoundedText> &failure, bool allowActions, WorkspaceFrame &frame) const;
        /// @brief 在正常工作区层级收集当前文档的路径和处置选择, 模态期间屏蔽全局快捷键.
        /// @note 确认时复制当轮文档版本, 交 Session 再验证; 换文档或取消只清理本面板草稿.
        void DrawDialog(const SceneDocumentStatus *document, bool allowActions, WorkspaceFrame &frame);
        [[nodiscard]] bool HasDialog() const noexcept { return m_Action.has_value() || m_Import; }

    private:
        void Open(Scene::SceneId scene, const char *initialPath);
        [[nodiscard]] const char *Title() const noexcept;

        Scene::SceneId m_Scene;
        std::optional<SceneFileAction> m_Action;
        bool m_Import = false;
        bool m_OpenNext = false;
        std::array<char, 2048> m_Path{};
        std::array<char, 2048> m_SavePath{};
        std::array<char, 16> m_SourceScene{};
    };
}
