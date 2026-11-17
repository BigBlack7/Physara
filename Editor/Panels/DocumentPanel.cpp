#include "DocumentPanel.hpp"

/// @file
/// @brief 把 File 动作展开成项目路径和未保存决策, 仅向会话移交拥有型意图.

#include <algorithm>
#include <cstring>

#include <imgui.h>

#include "Workspace.hpp"

namespace Physara::Editor::Detail
{
    // 同一编辑窗口复用固定大小输入草稿. 新操作建立全新的正文和文档身份.
    void DocumentPanel::Open(Scene::SceneId scene, const char *initialPath)
    {
        m_Scene = scene;
        std::fill(m_Path.begin(), m_Path.end(), '\0');
        std::fill(m_SavePath.begin(), m_SavePath.end(), '\0');
        std::fill(m_SourceScene.begin(), m_SourceScene.end(), '\0');
        std::copy_n(initialPath, std::strlen(initialPath), m_Path.begin());
        constexpr char defaultSave[] = "Assets/Scenes/";
        std::copy(std::begin(defaultSave), std::end(defaultSave), m_SavePath.begin());
        m_OpenNext = true;
    }

    // 普通保存和无修改的新建可直接提交, 其它路径选择与未保存处置留给独立模态.
    void DocumentPanel::Request(SceneFileAction action, const SceneDocumentStatus &document, WorkspaceFrame &frame)
    {
        if ((action == SceneFileAction::Save && document.path) ||
            (action == SceneFileAction::New && !document.modified))
        {
            frame.file = FileIntent{document.version, action};
            return;
        }
        m_Action = action == SceneFileAction::Save ? SceneFileAction::SaveAs : action;
        m_Import = false;
        Open(document.version.scene, "Assets/Scenes/");
    }

    void DocumentPanel::RequestImport(Scene::SceneId scene)
    {
        m_Import = true;
        m_Action.reset();
        Open(scene, "Assets/Models/");
    }

    // 显示标题随动作改变, ### 后的 ImGui 身份保持一致, 同时只存在一份模态草稿.
    const char *DocumentPanel::Title() const noexcept
    {
        if (m_Import)
        {
            return "Import Model###SceneFileDialog";
        }
        switch (m_Action.value_or(SceneFileAction::New))
        {
        case SceneFileAction::Open: return "Open Scene###SceneFileDialog";
        case SceneFileAction::SaveAs: return "Save Scene As###SceneFileDialog";
        default: return "New Scene###SceneFileDialog";
        }
    }

    // 当前路径和操作状态分别来自文档 owner, 模型状态由导入 owner 独立推进.
    void DocumentPanel::DrawStatus(const SceneDocumentStatus &document, const SceneImportStatus &model,
        const std::optional<Core::BoundedText> &failure, bool allowActions, WorkspaceFrame &frame) const
    {
        if (!ImGui::BeginMenu(document.modified ? "Scene status *###SceneStatus" : "Scene status###SceneStatus"))
        {
            return;
        }
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 36.0f * ImGui::GetFontSize());
        if (document.path)
        {
            const auto text = document.path->generic_u8string();
            ImGui::TextUnformatted(reinterpret_cast<const char *>(text.c_str()));
        }
        else
        {
            ImGui::TextUnformatted("Untitled scene");
        }
        if (document.modified)
        {
            ImGui::TextUnformatted("Unsaved changes");
        }
        switch (document.phase)
        {
        case SceneFilePhase::Idle: break;
        case SceneFilePhase::Reading: ImGui::TextUnformatted("Reading scene file"); break;
        case SceneFilePhase::LoadingAssets: ImGui::TextUnformatted("Loading referenced assets"); break;
        case SceneFilePhase::Validating: ImGui::TextUnformatted("Validating scene"); break;
        case SceneFilePhase::Saving: ImGui::TextUnformatted("Saving scene"); break;
        case SceneFilePhase::Cancelling: ImGui::TextUnformatted("Cancelling scene file operation"); break;
        }
        if (document.lastFailure)
        {
            ImGui::TextUnformatted(document.lastFailure->View().data());
        }
        if (failure)
        {
            ImGui::TextUnformatted(failure->View().data());
        }
        if (document.operation != 0 && document.phase != SceneFilePhase::Cancelling &&
            ImGui::MenuItem("Cancel scene operation", nullptr, false, allowActions))
        {
            frame.cancelDocument = document.operation;
        }
        if (model.phase != SceneImportPhase::Idle)
        {
            ImGui::SeparatorText("Model import");
            ImGui::TextUnformatted(model.path.View().data());
            switch (model.phase)
            {
            case SceneImportPhase::Idle: break;
            case SceneImportPhase::Loading: ImGui::TextUnformatted("Loading model (CPU)"); break;
            case SceneImportPhase::CpuReady: ImGui::TextUnformatted("CPU ready - waiting to instantiate"); break;
            case SceneImportPhase::Instantiated: ImGui::TextUnformatted("Instantiated in the scene"); break;
            case SceneImportPhase::Failed: ImGui::TextUnformatted("Import failed"); break;
            case SceneImportPhase::Cancelled: ImGui::TextUnformatted("Import cancelled"); break;
            }
            if (!model.detail.View().empty())
            {
                ImGui::TextUnformatted(model.detail.View().data());
            }
            if (model.Pending() && ImGui::MenuItem("Cancel model import", nullptr, false, allowActions))
            {
                frame.cancelImport = model.operation;
            }
        }
        ImGui::PopTextWrapPos();
        ImGui::EndMenu();
    }

    // 只在当前活动文档内使用路径草稿. 对已在途的文件工作继续显示可退出的模态.
    void DocumentPanel::DrawDialog(const SceneDocumentStatus *document, bool allowActions, WorkspaceFrame &frame)
    {
        if (!m_Action && !m_Import)
        {
            return;
        }
        const char *title = Title();
        const bool opening = m_OpenNext && document && document->version.scene == m_Scene;
        if (opening)
        {
            ImGui::OpenPopup(title);
        }
        m_OpenNext = false;
        if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            m_Action.reset();
            m_Import = false;
            return;
        }
        if (!document || document->version.scene != m_Scene ||
            (allowActions && ImGui::IsKeyPressed(ImGuiKey_Escape, false)))
        {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            m_Action.reset();
            m_Import = false;
            return;
        }
        const auto action = m_Action.value_or(SceneFileAction::New);
        const bool replacing = !m_Import && (action == SceneFileAction::New || action == SceneFileAction::Open);
        // 以本轮实际版本确认处置, 不把打开模态时的 dirty 或路径长期缓存为领域状态.
        const bool dirty = replacing && document->modified;
        const bool busy = document->operation != 0 &&
            !(m_Import && (document->phase == SceneFilePhase::Saving || document->phase == SceneFilePhase::Cancelling));
        ImGui::BeginDisabled(!allowActions || busy);
        if (m_Import || action != SceneFileAction::New)
        {
            ImGui::SetNextItemWidth(360.0f);
            ImGui::InputText("Project-relative path", m_Path.data(), m_Path.size());
        }
        if (m_Import)
        {
            ImGui::InputText("Source scene (optional)", m_SourceScene.data(), m_SourceScene.size(),
                             ImGuiInputTextFlags_CharsDecimal);
            if (ImGui::Button("Import"))
            {
                frame.import = ImportIntent{m_Scene, m_Path.data(), m_SourceScene.data()};
            }
        }
        else if (dirty)
        {
            ImGui::TextUnformatted("The current scene has unsaved changes.");
            if (!document->path)
            {
                ImGui::SetNextItemWidth(360.0f);
                ImGui::InputText("Save to", m_SavePath.data(), m_SavePath.size());
            }
            if (ImGui::Button("Save and Continue"))
            {
                frame.file = FileIntent{document->version, action, ReplaceDecision::Save, m_Path.data(),
                                        m_SavePath.data()};
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard"))
            {
                frame.file = FileIntent{document->version, action, ReplaceDecision::Discard, m_Path.data()};
            }
        }
        else if (ImGui::Button(action == SceneFileAction::SaveAs ? "Save" :
                               action == SceneFileAction::Open ? "Open" : "New"))
        {
            frame.file = FileIntent{document->version, action, ReplaceDecision::None, m_Path.data()};
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if ((allowActions && ImGui::Button("Cancel")) || frame.file || frame.import)
        {
            ImGui::CloseCurrentPopup();
            m_Action.reset();
            m_Import = false;
        }
        ImGui::EndPopup();
    }
}
