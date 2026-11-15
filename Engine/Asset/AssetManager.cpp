#include "AssetManager.hpp"

/// @file
/// @brief 组织图片导入并裁决发布. worker 不访问资产表, 只按请求票据交还候选.

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <limits>
#include <new>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <Core/Jobs.hpp>
#include <Core/ResourceIO.hpp>

#include "ImageDecode.hpp"

namespace Physara::Asset
{
    /// @brief 来源在有效 AssetId 的寿命内固定. 源代表示 owner 已知的失效顺序.
    struct ImportTicket
    {
        AssetRequest request;
        std::uint64_t sourceGeneration;
        ContentRevision baseRevision;
    };

    /// @brief worker 独占的请求快照. 不借用 AssetEntry, previousFingerprint 用于免解码确认旧内容.
    struct ImageJob
    {
        ImportTicket ticket;
        std::filesystem::path root;
        Core::ResourcePath source;
        std::optional<Core::Sha256Digest> previousFingerprint;
        std::shared_ptr<PixelBudget> budget;
        std::size_t sourceLimit;
        std::size_t imageLimit;
    };

    /// @brief 已完成解码但尚未发布的拥有型候选, 其像素已占用共享预算.
    struct ImageCandidate
    {
        Core::Sha256Digest fingerprint;
        Rgba8Image image;
    };

    /// @brief 输入指纹与准入时的已发布版本相同, 不携带第二份像素.
    struct SameContent
    {
    };

    /// @brief worker 在阶段边界观察到取消, owner 不得据此推进内容版本.
    struct CancelledImport
    {
    };

    /// @brief 与输入同票据的完成包, 可在 worker 退出后独立等待 owner 裁决.
    struct ImageCompletion
    {
        ImportTicket ticket;
        std::variant<ImageCandidate, SameContent, CancelledImport, AssetFailure> result;
    };

    using ImageJobs = Core::JobQueue<ImageJob, ImageCompletion>;

    /// @brief 每个逻辑来源一条登记记录. 当前请求的状态和最后有效内容分开持有.
    struct AssetEntry
    {
        Core::ResourcePath source;
        AssetId id;
        std::uint64_t sourceGeneration = 1;
        AssetStatus status{};
        ImageJobs::Id activeJob{};

        [[nodiscard]] ContentRevision Revision() const noexcept
        {
            return status.published ? status.published->revision : ContentRevision{};
        }
    };

    /// @brief 用自持错误描述跨越 worker 和 owner 的寿命边界.
    static AssetFailure Fail(AssetError reason, std::string_view text) noexcept
    {
        return {reason, Core::BoundedText(text)};
    }

    /// @brief 保留 IO 的路径、读入和预算分类. 取消由作业结果单独表达.
    static AssetFailure ResourceImportFailure(const Core::ResourceFailure &error) noexcept
    {
        switch (error.reason)
        {
        case Core::ResourceError::InvalidPath:
            return {AssetError::InvalidPath, error.description};
        case Core::ResourceError::CannotOpen:
            return {AssetError::CannotOpen, error.description};
        case Core::ResourceError::IncompleteRead:
            return {AssetError::IncompleteRead, error.description};
        case Core::ResourceError::ByteLimit:
            return {AssetError::SourceByteLimit, error.description};
        case Core::ResourceError::HashFailed:
            return {AssetError::HashFailed, error.description};
        case Core::ResourceError::Cancelled:
            break;
        }
        return {AssetError::UnexpectedFailure, error.description};
    }

    /// @brief 完成读取、摘要比较和解码. 任何异常都作为拥有型失败值交回.
    static ImageCompletion ImportImage(const ImageJob &job, std::stop_token stop) noexcept
    {
        try
        {
            // 路径、文件长度和完整快照由 Core 建立; 取消优先于本次读取产生的诊断.
            auto input = Core::ReadResourceFile(job.root, job.source, job.sourceLimit, stop);
            if (stop.stop_requested())
            {
                return {job.ticket, CancelledImport{}};
            }
            if (const auto *error = std::get_if<Core::ResourceFailure>(&input))
            {
                return {job.ticket, ResourceImportFailure(*error)};
            }
            const auto &snapshot = std::get<Core::ResourceBytes>(input);
            // 相同字节复用旧记录, 避免重复解码挤占仍由读者持有的像素额度.
            if (job.previousFingerprint == snapshot.fingerprint)
            {
                return {job.ticket, SameContent{}};
            }

            auto decoded = DecodeImage(snapshot.bytes, job.imageLimit, job.budget);
            if (stop.stop_requested())
            {
                return {job.ticket, CancelledImport{}};
            }
            if (const auto *error = std::get_if<AssetFailure>(&decoded))
            {
                return {job.ticket, *error};
            }
            return {job.ticket, ImageCandidate{snapshot.fingerprint, std::move(std::get<Rgba8Image>(decoded))}};
        }
        catch (const std::bad_alloc &)
        {
            return {job.ticket, Fail(AssetError::OutOfMemory, "Cannot allocate the image import result")};
        }
        catch (...)
        {
            return {job.ticket, Fail(AssetError::UnexpectedFailure, "Unexpected image import failure")};
        }
    }

    /// @brief 给新登记条目分配非零 generation, 也避免不同 owner 的同一槽被当作旧目标复用.
    static std::uint64_t AllocateAssetGeneration() noexcept
    {
        static std::atomic<std::uint64_t> last{0};
        auto value = last.load(std::memory_order_relaxed);
        for (;;)
        {
            auto next = value;
            if (!Core::AdvanceGeneration(next))
            {
                return 0;
            }
            if (last.compare_exchange_weak(value, next, std::memory_order_relaxed))
            {
                return next;
            }
        }
    }

    /// @brief owner 专有状态. 作业队列析构前保留输入需要的 CPU 所有权.
    struct AssetManager::Impl
    {
        const std::filesystem::path root;
        const ImportLimits limits;
        const std::shared_ptr<PixelBudget> budget;
        std::vector<std::unique_ptr<AssetEntry>> entries = std::vector<std::unique_ptr<AssetEntry>>(0);
        std::unique_ptr<ImageJobs> jobs;
        std::uint64_t lastRequest = 0;
        bool stopping = false;

        /// @brief 固定服务额度并预留登记表, 使作业准入后的 owner 提交步骤不再分配.
        Impl(std::filesystem::path resourceRoot, ImportLimits importLimits)
            : root(std::move(resourceRoot)), limits(importLimits),
              budget(std::make_shared<PixelBudget>(limits.retainedPixels))
        {
            entries.reserve(limits.assetCount);
        }

        /// @brief 通过槽与 generation 查找, 避免过期身份访问另一条记录.
        [[nodiscard]] AssetEntry *Find(AssetId id) const noexcept
        {
            if (id.IsValid() && id.index < entries.size() && entries[id.index]->id == id)
            {
                return entries[id.index].get();
            }
            return nullptr;
        }

        /// @brief 取得当前来源. 路径查找只出现在有界的冷请求入口.
        [[nodiscard]] AssetEntry *Find(const Core::ResourcePath &source) const
        {
            const auto existing = std::ranges::find_if(entries, [&source](const auto &entry)
            {
                return entry->source.Relative() == source.Relative();
            });
            return existing == entries.end() ? nullptr : existing->get();
        }

        /// @brief 先撤销当前请求资格, 再通知作业. 已发布的旧记录继续可读.
        void CancelCurrent(AssetEntry &entry) noexcept
        {
            assert(jobs && entry.status.state == AssetState::Loading);
            entry.status.state = AssetState::Cancelled;
            jobs->Cancel(std::exchange(entry.activeJob, {}));
        }

        /// @brief 只在作业准入成功后推进请求状态并登记新记录.
        [[nodiscard]] AssetManager::Submission Submit(Core::ResourcePath source)
        {
            auto *entry = Find(source);
            if (entry && (entry->status.state == AssetState::Loading || entry->status.state == AssetState::CpuReady))
            {
                return AssetRequest{entry->id, entry->status.request};
            }

            std::unique_ptr<AssetEntry> pending;
            if (!entry)
            {
                if (entries.size() == limits.assetCount)
                {
                    return Fail(AssetError::AssetCapacity, "CPU asset table is full");
                }
                const auto generation = AllocateAssetGeneration();
                if (generation == 0)
                {
                    return Fail(AssetError::IdentityExhausted, "Asset identity range exhausted");
                }
                const AssetId id{static_cast<std::uint32_t>(entries.size()), generation};
                pending = std::make_unique<AssetEntry>(std::move(source), id);
                entry = pending.get();
            }
            auto nextRequest = lastRequest;
            if (!Core::AdvanceGeneration(nextRequest))
            {
                return Fail(AssetError::IdentityExhausted, "Asset request identity range exhausted");
            }

            auto fingerprint = entry->status.published ?
                std::optional(entry->status.published->sourceFingerprint) : std::nullopt;
            const AssetRequest request{entry->id, RequestId{nextRequest}};
            const ImportTicket ticket{request, entry->sourceGeneration, entry->Revision()};
            auto job = std::make_unique<ImageJob>(ticket, root, entry->source, fingerprint, budget,
                                                  limits.sourceBytes, limits.imageBytes);
            if (!jobs)
            {
                // 只有已校验且确实要排队的请求启动 CPU worker.
                jobs = std::make_unique<ImageJobs>(limits.pendingJobs, limits.workerCount, &ImportImage);
            }
            const auto submitted = jobs->TrySubmit(std::move(job));
            if (const auto *error = std::get_if<Core::JobRejection>(&submitted))
            {
                switch (*error)
                {
                case Core::JobRejection::Full:
                    return Fail(AssetError::QueueFull, "Asset work is full, retry after completions are taken");
                case Core::JobRejection::Stopped:
                    return Fail(AssetError::Stopped, "Asset work has stopped");
                case Core::JobRejection::Exhausted:
                    return Fail(AssetError::IdentityExhausted, "Asset job identity range exhausted");
                }
            }

            // 这些操作不分配. entries 在创建时已为全部允许记录预留容量.
            lastRequest = nextRequest;
            entry->activeJob = std::get<ImageJobs::Id>(submitted);
            entry->status.request = request.request;
            entry->status.state = AssetState::Loading;
            entry->status.failure.reset();
            if (pending)
            {
                entries.push_back(std::move(pending));
            }
            return request;
        }

        /// @brief 核对完成包并在一个 owner 步骤内替换有效引用. 返回是否发布了新版本.
        [[nodiscard]] bool Publish(ImageCompletion &completion) noexcept
        {
            const auto &ticket = completion.ticket;
            auto *entry = Find(ticket.request.asset);
            // 先检查完整资格, 过期成功和过期失败都不能覆盖新请求的状态或诊断.
            if (!entry || entry->status.state != AssetState::Loading ||
                entry->status.request != ticket.request.request ||
                entry->sourceGeneration != ticket.sourceGeneration || entry->Revision() != ticket.baseRevision)
            {
                return false;
            }

            entry->activeJob = {};
            if (std::holds_alternative<CancelledImport>(completion.result))
            {
                entry->status.state = AssetState::Cancelled;
                return false;
            }
            if (const auto *error = std::get_if<AssetFailure>(&completion.result))
            {
                entry->status.state = AssetState::Failed;
                entry->status.failure = *error;
                return false;
            }
            if (std::holds_alternative<SameContent>(completion.result))
            {
                assert(entry->status.published);
                entry->status.state = AssetState::CpuReady;
                return false;
            }

            auto revision = entry->Revision();
            if (Core::AdvanceGeneration(revision.value))
            {
                auto &candidate = std::get<ImageCandidate>(completion.result);
                try
                {
                    // 新记录构造成功后才替换 owner 的引用. 旧读者继续保活旧像素及其预算.
                    auto record = std::make_shared<const ImageRecord>(entry->id, revision, entry->source.Relative(),
                        candidate.fingerprint, std::move(candidate.image));
                    entry->status.published = std::move(record);
                    entry->status.state = AssetState::CpuReady;
                    return true;
                }
                catch (const std::bad_alloc &)
                {
                    entry->status.failure = Fail(AssetError::OutOfMemory, "Cannot allocate the published image");
                }
            }
            else
            {
                entry->status.failure = Fail(AssetError::IdentityExhausted, "Asset content revision range exhausted");
            }
            entry->status.state = AssetState::Failed;
            return false;
        }
    };

    AssetManager::AssetManager(std::unique_ptr<Impl> state) noexcept : m_State(std::move(state))
    {
    }

    AssetManager::~AssetManager()
    {
        Shutdown();
    }

    AssetManager::Creation AssetManager::Create(std::filesystem::path root, ImportLimits limits)
    {
        if (!root.is_absolute() || limits.assetCount == 0 || limits.workerCount == 0 ||
            limits.workerCount > limits.pendingJobs || limits.sourceBytes == 0 ||
            limits.sourceBytes > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
            limits.imageBytes == 0 || limits.retainedPixels == 0)
        {
            return Fail(AssetError::InvalidConfiguration, "Invalid asset resource root or import limits");
        }
        try
        {
            return std::unique_ptr<AssetManager>(new AssetManager(std::make_unique<Impl>(std::move(root), limits)));
        }
        catch (const std::bad_alloc &)
        {
            return Fail(AssetError::OutOfMemory, "Cannot allocate the CPU asset service");
        }
    }

    AssetManager::Submission AssetManager::RequestImage(const std::filesystem::path &source)
    {
        if (m_State->stopping)
        {
            return Fail(AssetError::Stopped, "Asset service has stopped");
        }
        try
        {
            constexpr std::array<std::wstring_view, 3> Directories{L"Textures", L"Editor", L"Environments"};
            auto path = Core::ResourcePath::Parse(source, Directories);
            if (const auto *error = std::get_if<Core::ResourceFailure>(&path))
            {
                return ResourceImportFailure(*error);
            }
            auto &resource = std::get<Core::ResourcePath>(path);
            if (resource.Directory() == L"Environments")
            {
                return Fail(AssetError::UnsupportedEncoding, "Environment pixel decoding is not supported");
            }
            return m_State->Submit(std::move(resource));
        }
        catch (const std::bad_alloc &)
        {
            return Fail(AssetError::OutOfMemory, "Cannot allocate the image import request");
        }
        catch (const std::system_error &)
        {
            return Fail(AssetError::WorkerUnavailable, "Cannot start CPU asset work");
        }
    }

    std::optional<AssetFailure> AssetManager::InvalidateSource(AssetId asset) noexcept
    {
        if (m_State->stopping)
        {
            return Fail(AssetError::Stopped, "Asset service has stopped");
        }
        auto *entry = m_State->Find(asset);
        if (!entry)
        {
            return Fail(AssetError::InvalidAsset, "Cannot invalidate an unknown asset");
        }
        // 源变化立即独立生效, 不以能否接纳后继任务为条件.
        if (!Core::AdvanceGeneration(entry->sourceGeneration))
        {
            return Fail(AssetError::IdentityExhausted, "Asset source generation range exhausted");
        }
        if (entry->status.state == AssetState::Loading)
        {
            m_State->CancelCurrent(*entry);
        }
        else
        {
            entry->status.state = AssetState::Unloaded;
            entry->status.request = {};
            entry->status.failure.reset();
        }
        return std::nullopt;
    }

    void AssetManager::Cancel(AssetRequest request) noexcept
    {
        auto *entry = m_State->Find(request.asset);
        if (entry && entry->status.request == request.request && entry->status.state == AssetState::Loading)
        {
            m_State->CancelCurrent(*entry);
        }
    }

    AssetStatus AssetManager::Status(AssetId asset) const noexcept
    {
        const auto *entry = m_State->Find(asset);
        return entry ? entry->status : AssetStatus{};
    }

    std::size_t AssetManager::PublishCompleted() noexcept
    {
        if (m_State->stopping || !m_State->jobs)
        {
            return 0;
        }
        std::size_t published = 0;
        while (auto completion = m_State->jobs->TakeCompleted())
        {
            // owner 在此循环内不提交新任务, 处理量由固定作业槽数限制.
            published += m_State->Publish(*completion) ? 1u : 0u;
        }
        return published;
    }

    void AssetManager::Shutdown() noexcept
    {
        // 先关闭 owner 的发布入口, 再取消并等待队列; join 期间的新完成只能释放, 不能发布.
        if (std::exchange(m_State->stopping, true))
        {
            return;
        }
        for (auto &entry : m_State->entries)
        {
            if (entry->status.state == AssetState::Loading)
            {
                m_State->CancelCurrent(*entry);
            }
        }
        if (m_State->jobs)
        {
            m_State->jobs->Shutdown();
        }
    }
}
