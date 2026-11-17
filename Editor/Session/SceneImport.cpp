#include "SceneImport.hpp"

/// @file
/// @brief 从已发布的模型请求创建独立静态实例. 任何失配都在场景事务前终止.

#include <exception>
#include <utility>

#include <Asset/ModelAsset.hpp>

#include "Commands/EditHistory.hpp"

namespace Physara::Editor::Detail
{
    // 先分配会话操作号, 再请求共享模型. path/detail 只用于展示, 实例资格由 Pending 保存的身份决定.
    std::optional<Core::BoundedText> SceneImport::Begin(Asset::AssetManager &assets, Scene::SceneId scene,
        const std::filesystem::path &path, Asset::ModelImportOptions options)
    {
        if (m_Pending)
        {
            return Core::BoundedText("Another model import is active");
        }
        if (!Core::AdvanceGeneration(m_Status.operation))
        {
            return Core::BoundedText("Model import identity exhausted");
        }
        try
        {
            const auto text = path.generic_u8string();
            m_Status.path.Assign({reinterpret_cast<const char *>(text.data()), text.size()});
            m_Status.detail = {};
            const auto submitted = assets.RequestModel(path, options);
            if (const auto *error = std::get_if<Asset::AssetFailure>(&submitted))
            {
                Fail(error->description);
                return error->description;
            }
            m_Pending = Pending{scene, std::get<Asset::AssetRequest>(submitted)};
            m_Status.phase = SceneImportPhase::Loading;
            return std::nullopt;
        }
        catch (const std::exception &error)
        {
            const Core::BoundedText failure(error.what());
            Fail(failure);
            return failure;
        }
    }

    // 文档身份和准确请求代先于 CpuReady 判断, 避免把 manager 保留的旧有效版本当作本次结果.
    void SceneImport::Advance(Asset::AssetManager &assets, Scene::Scene &scene, Renderer::View &view,
                              EditHistory &history)
    {
        if (!m_Pending)
        {
            return;
        }
        if (m_Pending->scene != scene.Id())
        {
            Retire();
            return;
        }
        const auto outcome = assets.StatusModel(m_Pending->request.asset);
        if (outcome.request != m_Pending->request.request)
        {
            Fail(Core::BoundedText("Model import request was superseded"));
            return;
        }
        if (outcome.state == Asset::AssetState::Loading)
        {
            return;
        }
        if (outcome.state != Asset::AssetState::CpuReady)
        {
            Fail(outcome.failure ? outcome.failure->description : Core::BoundedText("Model request is no longer ready"));
            return;
        }
        const auto &model = outcome.published;
        m_Status.phase = SceneImportPhase::CpuReady;
        m_Status.detail = model->supportReport;
        if (history.HasGesture())
        {
            // 不把实例事务插进字段预览与完成之间. 下一次 CPU 发布边界仍须重新核对请求代.
            return;
        }
        try
        {
            const auto stem = model->source.stem().u8string();
            std::string name(reinterpret_cast<const char *>(stem.data()), stem.size());
            if (const auto error = history.ApplyScene(scene, view, Scene::Instantiate{{}, model, std::move(name)}))
            {
                Fail(*error);
                return;
            }
            m_Pending.reset();
            m_Status.phase = SceneImportPhase::Instantiated;
        }
        catch (const std::exception &error)
        {
            Fail(Core::BoundedText(error.what()));
        }
    }

    // 仅撤销实例发布资格, 不取消可能被文档打开或其它实例请求复用的 Asset 作业.
    void SceneImport::Retire() noexcept
    {
        if (m_Pending)
        {
            m_Pending.reset();
            m_Status.phase = SceneImportPhase::Cancelled;
            m_Status.detail = {};
        }
    }

    // 保留有界失败和操作号供工作区显示, 不保留可被之后误提交的 Pending.
    void SceneImport::Fail(const Core::BoundedText &error) noexcept
    {
        m_Pending.reset();
        m_Status.phase = SceneImportPhase::Failed;
        m_Status.detail = error;
    }
}
