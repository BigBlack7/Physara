#include "SceneDocuments.hpp"

/// @file
/// @brief 串行场景文件状态机. worker 读写自持值, owner 按操作编号采纳完整结果.

#include <cassert>
#include <exception>
#include <unordered_set>
#include <utility>

#include <Asset/SourceVerification.hpp>
#include <Core/Jobs.hpp>

#include "Commands/EditHistory.hpp"
#include "SceneDocumentAssets.hpp"

namespace Physara::Editor::Detail
{
    /// @brief 初次读取可复用的活动模型快照, 不包含活动 Scene 的借用.
    struct ReadDocument
    {
        std::vector<Asset::ModelVersion> pinned;
    };

    /// @brief 保存捕获的创作值与资源引用, 后续编辑不会改变本载荷.
    struct SaveDocument
    {
        std::vector<Scene::EntityRecord> entities;
        Renderer::ViewDocumentRequests view;
    };

    /// @brief 从主线程移交的已就绪资产集合, worker 独占验证期间的可变状态.
    struct PrepareCandidate
    {
        std::shared_ptr<SceneOpenInputs> inputs;
    };

    using FileWork = std::variant<ReadDocument, SaveDocument, PrepareCandidate>;

    /// @brief 单槽队列的自持输入. stop 负责合作取消, commit 单独裁决是否仍可替换目标.
    struct DocumentJob
    {
        std::uint64_t operation;
        std::filesystem::path root;
        Core::ResourcePath path;
        std::stop_source stop;
        std::shared_ptr<Core::ResourceCommit> commit;
        FileWork work;
    };

    /// @brief 已通过领域校验的新 Scene 和同文件的主 View 请求, 等待 owner 整体安装.
    struct CandidateDocument
    {
        std::unique_ptr<Scene::Scene> scene;
        Renderer::ViewDocumentRequests view;
    };

    /// @brief worker 查明缓存过期后交回 owner, 由 AssetManager 失效并重新请求实际源.
    struct RefreshSources
    {
        std::shared_ptr<SceneOpenInputs> inputs;
        std::vector<std::size_t> stale;
    };

    struct SavedDocument {};
    using FileResult = std::variant<std::shared_ptr<SceneOpenInputs>, RefreshSources, CandidateDocument, SavedDocument,
                                    Core::BoundedText>;

    struct DocumentCompletion
    {
        std::uint64_t operation;
        FileResult result;
    };

    using FileQueue = Core::JobQueue<DocumentJob, DocumentCompletion>;

    /// @brief 保存前核对每个固定模型版本. 旧版本的缓存可用性不构成当前文件的源证明.
    static void SaveSnapshot(const SaveDocument &save, const DocumentJob &job, std::stop_token stop)
    {
        std::unordered_set<const Asset::ModelRecord *> checked;
        for (const auto &entity : save.entities)
        {
            if (entity.model && checked.insert(entity.model.get()).second)
            {
                const auto source = Asset::VerifySource(job.root, *entity.model, stop);
                if (source.result != Asset::SourceMatch::Unchanged)
                {
                    Core::BoundedText error("Cannot save fixed model source: ");
                    error.Append(source.detail.View());
                    throw error;
                }
            }
        }
        const auto encoded = EncodeSceneDocument(save.entities, save.view, stop);
        const auto bytes = std::as_bytes(std::span(encoded.data(), encoded.size()));
        if (auto failure = Core::WriteResourceFile(job.root, job.path, bytes, *job.commit, stop))
        {
            throw failure->description;
        }
    }

    /// @brief 为 worker 的字段或资产诊断补充外层场景文件定位.
    static Core::BoundedText FileError(const Core::ResourcePath &path, std::string_view description)
    {
        const auto text = path.Relative().generic_u8string();
        Core::BoundedText error(std::string_view(reinterpret_cast<const char *>(text.data()), text.size()));
        error.Append(": ");
        error.Append(description);
        return error;
    }

    /// @brief 唯一 worker 入口. 输入只拥有值及已固定资产, 不借用 owner 或 UI.
    static DocumentCompletion ProcessDocument(const DocumentJob &job, std::stop_token queueStop) noexcept
    {
        std::stop_callback queueCancel(queueStop, [source = job.stop]() mutable { source.request_stop(); });
        const auto stop = job.stop.get_token();
        Core::BoundedText error;
        try
        {
            if (const auto *read = std::get_if<ReadDocument>(&job.work))
            {
                auto file = Core::ReadResourceFile(job.root, job.path, DocumentByteLimit, stop);
                if (const auto *failure = std::get_if<Core::ResourceFailure>(&file))
                {
                    throw failure->description;
                }
                const auto &bytes = std::get<Core::ResourceBytes>(file).bytes;
                return {job.operation, std::make_shared<SceneOpenInputs>(DecodeSceneDocument(bytes, stop),
                                                                         read->pinned)};
            }
            if (const auto *verify = std::get_if<PrepareCandidate>(&job.work))
            {
                auto stale = verify->inputs->Verify(job.root, stop);
                if (!stale.empty())
                {
                    return {job.operation, RefreshSources{verify->inputs, std::move(stale)}};
                }
                return {job.operation, CandidateDocument{verify->inputs->Build(stop), verify->inputs->TakeView()}};
            }
            SaveSnapshot(std::get<SaveDocument>(job.work), job, stop);
            return {job.operation, SavedDocument{}};
        }
        catch (const Core::BoundedText &failure)
        {
            error = failure;
        }
        catch (const std::exception &failure)
        {
            error.Assign(failure.what());
        }
        catch (...)
        {
            error.Assign("Unexpected scene file failure");
        }
        try
        {
            return {job.operation, FileError(job.path, error.View())};
        }
        catch (...)
        {
            return {job.operation, error};
        }
    }

    /// @brief owner 当前唯一操作的身份和捕获版本. 等资产时拥有 open, 排入 worker 后移交它.
    struct ActiveFileOperation
    {
        std::uint64_t id;
        SceneFilePhase phase;
        SceneDocumentVersion captured;
        Core::ResourcePath target;
        std::filesystem::path nextPath; ///< 成功后发布所需的自持路径, 提交阶段不再分配.
        std::stop_source stop;
        std::shared_ptr<Core::ResourceCommit> commit;
        std::shared_ptr<SceneOpenInputs> open;
        std::optional<SceneFileRequest> replaceAfterSave; ///< 只随匹配的保存成功终态继续.
    };

    /// @brief 活动文档, 保存基线和串行操作的唯一 owner, 与 worker 的结果存储分离.
    struct SceneDocuments::State
    {
        State(std::filesystem::path projectRoot, std::unique_ptr<Scene::Scene> initial, const Renderer::View &main)
            : root(std::move(projectRoot)), scene(std::move(initial)), view(main.Id())
        {
            saved = Current(main);
        }

        [[nodiscard]] SceneDocumentVersion Current(const Renderer::View &main) const noexcept
        {
            return {scene->Id(), scene->Versions().document, main.DocumentRevision()};
        }

        std::filesystem::path root;
        std::unique_ptr<Scene::Scene> scene;
        Renderer::ViewId view;
        SceneDocumentVersion saved;
        std::optional<std::filesystem::path> path;
        std::optional<Core::BoundedText> failure;
        std::optional<ActiveFileOperation> active;
        std::unique_ptr<FileQueue> queue;
        std::uint64_t serial{};
        bool stopped{};

        /// @brief 按需启动单 worker, 将下一阶段值排入已空出的槽; 拒绝时没有新工作被接受.
        void Submit(const ActiveFileOperation &operation, FileWork work)
        {
            if (!queue)
            {
                queue = std::make_unique<FileQueue>(1, 1, ProcessDocument);
            }
            auto job = std::make_unique<DocumentJob>(DocumentJob{operation.id, root, operation.target, operation.stop,
                                                                 operation.commit, std::move(work)});
            if (std::holds_alternative<Core::JobRejection>(queue->TrySubmit(std::move(job))))
            {
                throw Core::BoundedText("Scene file job could not be admitted");
            }
        }

        /// @brief 对已校验的 Scene 候选完成 View 请求及版号预检, 然后一起切换文档和保存基线.
        /// @note View 预检后只移动已拥有的状态; 选择失效由外层会话在返回后处理.
        void Install(CandidateDocument candidate, std::optional<std::filesystem::path> next,
                     Renderer::View &main, EditHistory &history)
        {
            auto result = main.PrepareDocumentUpdate(view, std::move(candidate.view));
            if (const auto *error = std::get_if<Core::BoundedText>(&result))
            {
                throw *error;
            }
            main.PublishDocumentUpdate(std::get<Renderer::ViewDocumentUpdate>(std::move(result)));
            scene = std::move(candidate.scene);
            path = std::move(next);
            saved = Current(main);
            history.Clear();
        }

        /// @brief 释放当前操作并登记结果, 不改写活动 Scene, 当前路径或保存基线.
        void Finish(const Core::BoundedText *error = nullptr) noexcept
        {
            failure = error ? std::optional(*error) : std::nullopt;
            active.reset();
        }
    };

    SceneDocuments::SceneDocuments(std::filesystem::path root, std::unique_ptr<Scene::Scene> scene,
                                   const Renderer::View &view)
        : m_State(std::make_unique<State>(std::move(root), std::move(scene), view))
    {
    }

    SceneDocuments::~SceneDocuments()
    {
        Shutdown();
    }

    Scene::Scene &SceneDocuments::Active() noexcept { return *m_State->scene; }
    Renderer::ViewId SceneDocuments::MainView() const noexcept { return m_State->view; }
    bool SceneDocuments::IsStopped() const noexcept { return m_State->stopped; }

    bool SceneDocuments::Frozen() const noexcept
    {
        if (IsStopped())
        {
            return true;
        }
        if (const auto &active = m_State->active)
        {
            return active->phase == SceneFilePhase::Reading || active->phase == SceneFilePhase::LoadingAssets ||
                active->phase == SceneFilePhase::Validating;
        }
        return false;
    }

    SceneDocumentStatus SceneDocuments::Status(const Renderer::View &view) const
    {
        const auto &state = *m_State;
        const auto current = state.Current(view);
        return {current, state.path, state.active ? state.active->phase : SceneFilePhase::Idle,
                state.active ? state.active->id : 0, current != state.saved, state.failure};
    }

    std::optional<Core::BoundedText> SceneDocuments::Begin(const SceneFileRequest &request, Renderer::View &view,
                                                          EditHistory &history)
    {
        auto &state = *m_State;
        if (state.stopped || request.scene != state.scene->Id() || view.Id() != state.view)
        {
            return Core::BoundedText("File operation belongs to an expired document or View");
        }
        if (state.active)
        {
            return Core::BoundedText("Another scene file operation is active");
        }
        try
        {
            const bool replacing = request.action == SceneFileAction::New || request.action == SceneFileAction::Open;
            if (replacing)
            {
                // 放弃授权绑定结束手势后的真实版本. 授权只允许尝试替换, 不会预先清除未保存状态.
                if (const auto error = history.CancelGesture(state.scene.get(), &view))
                {
                    return error;
                }
                const auto current = state.Current(view);
                if (current != state.saved && request.discard != current)
                {
                    return Core::BoundedText("Resolve unsaved changes before replacing this scene");
                }
            }
            if (request.action == SceneFileAction::New)
            {
                auto created = Scene::Scene::Create();
                if (const auto *error = std::get_if<Core::BoundedText>(&created))
                {
                    return *error;
                }
                state.Install({std::get<std::unique_ptr<Scene::Scene>>(std::move(created)), {}},
                              std::nullopt, view, history);
                state.failure.reset();
                return std::nullopt;
            }
            if (request.action == SceneFileAction::Save && !state.path)
            {
                return Core::BoundedText("Select a scene path with Save As");
            }
            const auto &name = request.action == SceneFileAction::Save ? *state.path : request.path;
            auto target = ParseScenePath(name);
            if (auto *error = std::get_if<Core::ResourceFailure>(&target))
            {
                return error->description;
            }
            if (!Core::AdvanceGeneration(state.serial))
            {
                return Core::BoundedText("Scene file operation identity exhausted");
            }
            auto path = std::get<Core::ResourcePath>(std::move(target));
            ActiveFileOperation started{state.serial, replacing ? SceneFilePhase::Reading : SceneFilePhase::Saving,
                state.Current(view), path, path.Relative(), std::stop_source{},
                std::make_shared<Core::ResourceCommit>(), {}, {}};
            if (replacing)
            {
                state.Submit(started, ReadDocument{state.scene->PinnedModels()});
            }
            else
            {
                state.Submit(started, SaveDocument{state.scene->Snapshot(), view.DocumentRequests()});
            }
            state.active.emplace(std::move(started));
            state.failure.reset();
            return std::nullopt;
        }
        catch (const Core::BoundedText &error)
        {
            return error;
        }
        catch (const std::exception &error)
        {
            return Core::BoundedText(error.what());
        }
    }

    // 继续意图随已接受的保存任务持有, 取消和失败会和任务一起释放.
    std::optional<Core::BoundedText> SceneDocuments::SaveBeforeReplacing(SceneFileRequest replacement,
        const std::filesystem::path &saveAs, Renderer::View &view, EditHistory &history)
    {
        auto &state = *m_State;
        if (replacement.action != SceneFileAction::New && replacement.action != SceneFileAction::Open)
        {
            return Core::BoundedText("Save continuation requires New or Open");
        }
        const auto action = state.path ? SceneFileAction::Save : SceneFileAction::SaveAs;
        if (const auto error = Begin({action, replacement.scene, saveAs, {}}, view, history))
        {
            return error;
        }
        state.active->replaceAfterSave = std::move(replacement);
        return std::nullopt;
    }

    std::optional<Core::BoundedText> SceneDocuments::Cancel(std::uint64_t operation) noexcept
    {
        auto &state = *m_State;
        if (!state.active || state.active->id != operation || state.stopped)
        {
            return Core::BoundedText("Scene file operation is no longer active");
        }
        auto &active = *state.active;
        if (!active.commit->Cancel())
        {
            return Core::BoundedText("File replacement is already committing");
        }
        active.stop.request_stop();
        // 等资产阶段没有文件作业, 已共享的 Asset 作业由其自己的请求继续.
        if (active.phase == SceneFilePhase::LoadingAssets)
        {
            state.Finish();
        }
        else
        {
            active.phase = SceneFilePhase::Cancelling;
        }
        return std::nullopt;
    }

    void SceneDocuments::Advance(Asset::AssetManager &assets, Renderer::View &view, EditHistory &history)
    {
        auto &state = *m_State;
        if (state.stopped || !state.active || view.Id() != state.view)
        {
            return;
        }
        try
        {
            auto &active = *state.active;
            if (auto completed = state.queue->TakeCompleted())
            {
                // 先退还队列槽, 再核对业务身份. 取消结果只被回收, 不进入 Scene/View 安装分支.
                if (completed->operation != active.id || active.captured.scene != state.scene->Id())
                {
                    return;
                }
                if (active.phase == SceneFilePhase::Cancelling)
                {
                    const Core::BoundedText notice("Save cancelled. Start New or Open again to replace this scene.");
                    state.Finish(active.replaceAfterSave ? &notice : nullptr);
                    return;
                }
                auto &result = completed->result;
                if (const auto *error = std::get_if<Core::BoundedText>(&result))
                {
                    state.Finish(error);
                    return;
                }
                if (auto *input = std::get_if<std::shared_ptr<SceneOpenInputs>>(&result))
                {
                    active.open = std::move(*input);
                    active.phase = SceneFilePhase::LoadingAssets;
                }
                else if (auto *refresh = std::get_if<RefreshSources>(&result))
                {
                    refresh->inputs->Refresh(assets, refresh->stale);
                    active.open = std::move(refresh->inputs);
                    active.phase = SceneFilePhase::LoadingAssets;
                }
                else if (auto *opened = std::get_if<CandidateDocument>(&result))
                {
                    state.Install(std::move(*opened), std::move(active.nextPath), view, history);
                    state.Finish();
                    return;
                }
                else
                {
                    assert(std::holds_alternative<SavedDocument>(result));
                    // 记录捕获时的两个 revision, 不用届时的新值把保存期间的编辑误标为已保存.
                    state.saved = active.captured;
                    state.path = std::move(active.nextPath);
                    auto replacement = std::move(active.replaceAfterSave);
                    // 先结束原保存并释放串行槽, 才能请求 New/Open. 成功保存不授权丢弃随后接受的编辑.
                    state.Finish();
                    if (replacement)
                    {
                        if (state.Current(view) != state.saved)
                        {
                            state.failure.emplace("Scene changed during save. Start New or Open again.");
                        }
                        else
                        {
                            state.failure = Begin(*replacement, view, history);
                        }
                    }
                    return;
                }
            }
            if (active.open && active.open->Advance(assets))
            {
                state.Submit(active, PrepareCandidate{std::move(active.open)});
                active.phase = SceneFilePhase::Validating;
            }
        }
        catch (const Core::BoundedText &failure)
        {
            state.Finish(&failure);
        }
        catch (const std::exception &failure)
        {
            const Core::BoundedText error(failure.what());
            state.Finish(&error);
        }
    }

    void SceneDocuments::Shutdown() noexcept
    {
        auto &state = *m_State;
        state.stopped = true;
        if (state.active && state.active->commit->Cancel())
        {
            state.active->stop.request_stop();
        }
        if (state.queue)
        {
            // 已取得提交资格的写入不再观察取消, join 等待实际替换或失败结束.
            state.queue->Shutdown();
        }
        state.active.reset();
    }
}
