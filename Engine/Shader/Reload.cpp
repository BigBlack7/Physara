#include "Reload.hpp"

/// @file
/// @brief 合并 Windows 源目录通知, 以串行 Compiler 核对实际输入并交付当前请求的自持包.

#include <algorithm>
#include <chrono>
#include <limits>
#include <stop_token>

#include <Windows.h>

#include <Core/Jobs.hpp>

#include "BuildSupport.hpp"
#include "Compiler.hpp"
#include "Interface.hpp"
#include "Package.hpp"

namespace Physara::Shader
{
    using ReloadClock = std::chrono::steady_clock;

    /// @brief 从原生目录通知只取有界的脏标记. 实际受影响程序由编译输入身份确定.
    class DirectoryNotice final
    {
    public:
        explicit DirectoryNotice(const std::filesystem::path &path, bool subtree)
            : m_Handle(FindFirstChangeNotificationW(path.c_str(), subtree,
                  FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                  FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE))
        {
            if (m_Handle == INVALID_HANDLE_VALUE)
            {
                throw BuildError(ErrorCode::IoFailure, "Cannot observe Shader source directory: " + PathText(path));
            }
        }

        ~DirectoryNotice() { FindCloseChangeNotification(m_Handle); }
        DirectoryNotice(const DirectoryNotice &) = delete;
        DirectoryNotice &operator=(const DirectoryNotice &) = delete;

        // 零超时观察一次. 取得信号后重新布防, 下一批是否改变包由 worker 比较文件内容决定.
        [[nodiscard]] Result<bool> Poll() const noexcept
        {
            const auto status = WaitForSingleObject(m_Handle, 0);
            if (status == WAIT_TIMEOUT)
            {
                return false;
            }
            if (status != WAIT_OBJECT_0 || !FindNextChangeNotification(m_Handle))
            {
                return MakeFailure(ErrorCode::IoFailure, "Shader directory notification is no longer available.");
            }
            return true;
        }

    private:
        HANDLE m_Handle;
    };

    /// @brief 私有接口产物只由本会话生成, 先结束 worker 再移除自己创建的目录.
    class ReloadScratch final
    {
    public:
        explicit ReloadScratch(const std::filesystem::path &parent)
        {
            const auto root = CanonicalInputPath(parent);
            const auto stamp = ReloadClock::now().time_since_epoch().count();
            auto path = root / (L"ShaderReload-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                std::to_wstring(stamp));
            if (!std::filesystem::create_directory(path))
            {
                throw BuildError(ErrorCode::IoFailure, "Cannot reserve a private Shader generation directory.");
            }
            m_Path = std::move(path);
        }

        ~ReloadScratch()
        {
            std::error_code ignored;
            std::filesystem::remove_all(m_Path, ignored);
        }

        ReloadScratch(const ReloadScratch &) = delete;
        ReloadScratch &operator=(const ReloadScratch &) = delete;
        [[nodiscard]] const std::filesystem::path &Path() const noexcept { return m_Path; }

    private:
        std::filesystem::path m_Path;
    };

    // 对照构建时记录的路径和原字节摘要. 路径消失或读取失败时, 该条目失去本次命中资格.
    static bool InputMatches(const BuildInput &input) noexcept
    {
        const auto matched = GuardBuild<bool>([&] {
            const auto path = CanonicalInputPath(input.path);
            return path == input.path && HashText(ReadText(path, MaxPackageBytes)) == input.hash;
        });
        const auto *value = std::get_if<bool>(&matched);
        return value && *value;
    }

    // 缓存只在 worker 使用. 同一编译环境固定工具身份, 命中前核对实际请求、schema 及整个源闭包.
    class ReloadCompiler final
    {
        struct CachedPackage
        {
            VariantKey variant;
            std::shared_ptr<const ShaderPackage> package;
            std::vector<BuildInput> inputs;
            std::size_t bytes = 0;
        };

    public:
        struct Output
        {
            ReloadResult result;
            ReloadStatistics statistics;
        };
        struct Input
        {
            ReloadCompiler &compiler;
            std::uint64_t request;
        };

        ReloadCompiler(std::filesystem::path source, std::filesystem::path schemas,
            const std::filesystem::path &scratch, ReloadLimits limits)
            : m_Source(std::move(source)), m_Schemas(std::move(schemas)), m_Scratch(scratch),
              m_Statistics{.limit = limits}
        {
            m_Cache.reserve(limits.entries);
        }

        // 单个作业依目录顺序组装一套包. 失败回执仅携带首错和角色, 已缓存的合格包可供后续作业复用.
        [[nodiscard]] static Output Run(const Input &input, std::stop_token stop) noexcept
        {
            auto &self = input.compiler;
            ReloadResult result{.request = input.request};
            const auto outcome = GuardBuild<bool>([&] {
                if (!self.m_Compiler)
                {
                    auto compiler = Compiler::Create();
                    if (const auto *failure = std::get_if<Failure>(&compiler))
                    {
                        throw BuildError(failure->code, failure->context.description.View());
                    }
                    self.m_Compiler = std::get<std::unique_ptr<Compiler>>(std::move(compiler));
                }
                for (const auto &program : ProductPrograms())
                {
                    if (stop.stop_requested())
                    {
                        return false;
                    }
                    const auto index = static_cast<std::size_t>(program.key.role);
                    result.failedRole = program.key.role;
                    result.packages[index] = self.Acquire(program);
                }
                // 包共享源时, 同一次最终复验要求每个包都和当前字节匹配.
                for (const auto &item : self.m_Cache)
                {
                    const auto index = static_cast<std::size_t>(item.variant.role);
                    if (item.package == result.packages[index] && !std::ranges::all_of(item.inputs, InputMatches))
                    {
                        result.failedRole = item.variant.role;
                        throw BuildError(ErrorCode::IoFailure, "Shader input changed during generation preparation.");
                    }
                }
                return !stop.stop_requested();
            });
            if (const auto *failure = std::get_if<Failure>(&outcome))
            {
                result.packages = {};
                result.failure = *failure;
            }
            else if (!std::get<bool>(outcome))
            {
                result.packages = {};
                result.failure = MakeFailure(ErrorCode::InvalidInput, "Shader request was superseded.");
            }
            else
            {
                result.failedRole = ProductShader::Count;
            }
            self.m_Statistics.entries = self.m_Cache.size();
            return {std::move(result), self.m_Statistics};
        }

    private:
        // 先复验缓存的外部输入. 缺项时生成该接口, 编译程序并计量完整包, 容量充足后才登记.
        [[nodiscard]] std::shared_ptr<const ShaderPackage> Acquire(const ProductProgram &program)
        {
            const auto found = std::ranges::find_if(m_Cache, [&](const auto &entry) {
                return entry.variant == program.key && std::ranges::all_of(entry.inputs, InputMatches);
            });
            if (found != m_Cache.end())
            {
                ++m_Statistics.hits;
                std::rotate(found, std::next(found), m_Cache.end());
                return m_Cache.back().package;
            }

            const auto schema = CanonicalInputPath(m_Schemas / (std::string(program.interfaceName) + ".json"));
            const BuildInput schemaInput{schema, HashText(ReadText(schema, MaxDeclarationBytes))};
            const auto generated = m_Scratch / program.interfaceName;
            const auto interface = GenerateInterface({schema, generated});
            if (const auto *failure = std::get_if<Failure>(&interface))
            {
                Core::BoundedText description("Engine/Shader/Interface/");
                description.Append(PathText(schema.filename()));
                description.Append(": ");
                description.Append(failure->context.description.View());
                throw BuildError(failure->code, description.View());
            }
            const auto manifest = ParseJson(ReadText(generated / "Manifest.json", MaxDeclarationBytes), "Manifest.json");
            if (manifest.at("schemaHash") != schemaInput.hash)
            {
                throw BuildError(ErrorCode::IoFailure, "Shader schema changed while generating the candidate.");
            }

            const auto start = ReloadClock::now();
            ++m_Statistics.builds;
            auto build = m_Compiler->Build({m_Source / program.request, m_Source, generated, program.key},
                                          &m_Statistics.compilation);
            m_Statistics.buildSeconds += std::chrono::duration<double>(ReloadClock::now() - start).count();
            if (const auto *failure = std::get_if<Failure>(&build))
            {
                throw BuildError(failure->code, failure->context.description.View());
            }
            auto &compiled = std::get<BuildOutput>(build);
            const auto measured = MeasurePackage(compiled.package);
            if (const auto *failure = std::get_if<Failure>(&measured))
            {
                throw BuildError(failure->code, failure->context.description.View());
            }
            const auto bytes = std::get<std::size_t>(measured);
            if (bytes > m_Statistics.limit.bytes)
            {
                throw BuildError(ErrorCode::OutOfMemory, "Shader package exceeds the CPU cache byte limit.");
            }
            // 命中过的项已移到末尾. 从最久未用的一端只腾退缓存独占的包, 保留调用方仍持有的内容.
            while (m_Cache.size() == m_Statistics.limit.entries ||
                   bytes > m_Statistics.limit.bytes - m_Statistics.bytes)
            {
                const auto discard = std::ranges::find_if(m_Cache, [](const auto &entry) {
                    return entry.package.use_count() == 1;
                });
                if (discard == m_Cache.end())
                {
                    throw BuildError(ErrorCode::OutOfMemory, "Shader CPU cache capacity is held by live packages.");
                }
                m_Statistics.bytes -= discard->bytes;
                m_Cache.erase(discard);
            }
            // 生成字节已计入包身份. 重用时从相同 schema 和固定生成工具推导这部分输入,
            // 复验实际外部文件时不依赖会被下一任务复用的私有产物.
            const auto generatedRoot = CanonicalInputPath(generated);
            std::erase_if(compiled.inputs, [&](const auto &input) {
                return input.path.parent_path() == generatedRoot;
            });
            compiled.inputs.push_back(schemaInput);
            auto package = std::make_shared<const ShaderPackage>(std::move(compiled.package));
            m_Cache.push_back({program.key, package, std::move(compiled.inputs), bytes});
            m_Statistics.bytes += bytes;
            return package;
        }

        std::filesystem::path m_Source, m_Schemas, m_Scratch;
        std::unique_ptr<Compiler> m_Compiler;
        std::vector<CachedPackage> m_Cache;
        ReloadStatistics m_Statistics;
    };

    // queue 中的输入借用 compiler, compiler 使用 scratch. 成员逆序析构保证 worker 先结束.
    struct SourceReload::State
    {
        using Queue = Core::JobQueue<ReloadCompiler::Input, ReloadCompiler::Output>;

        State(const std::filesystem::path &source, const std::filesystem::path &schemas,
              const std::filesystem::path &scratchParent, ReloadLimits limits)
            : sourceNotice(source, true), schemaNotice(schemas, true), scratch(scratchParent),
              compiler(source, schemas, scratch.Path(), limits), statistics{.limit = limits}
        {
        }

        DirectoryNotice sourceNotice, schemaNotice;
        ReloadScratch scratch;
        ReloadCompiler compiler;
        Queue queue{1, 1, &ReloadCompiler::Run};
        ReloadStatistics statistics;
        Queue::Id job;
        std::uint64_t requested = 1;
        std::uint64_t submitted = 0;
        ReloadClock::time_point quietUntil{};
        bool observationFailed = false;
    };

    Result<std::unique_ptr<SourceReload>> SourceReload::Create(const std::filesystem::path &checkout,
        const std::filesystem::path &scratchParent, ReloadLimits limits) noexcept
    {
        return GuardBuild<std::unique_ptr<SourceReload>>([&] {
            if (!limits.entries || !limits.bytes || !checkout.is_absolute())
            {
                throw BuildError(ErrorCode::InvalidInput, "Shader reload requires an absolute checkout and finite capacity.");
            }
            const auto source = CanonicalInputPath(checkout / "Shaders");
            const auto schemas = CanonicalInputPath(checkout / "Engine/Shader/Interface");
            return std::unique_ptr<SourceReload>(new SourceReload(
                std::make_unique<State>(source, schemas, scratchParent, limits)));
        });
    }

    SourceReload::SourceReload(std::unique_ptr<State> state) noexcept : m_State(std::move(state)) {}
    SourceReload::~SourceReload() = default;

    std::uint64_t SourceReload::Requested() const noexcept { return m_State->requested; }
    ReloadStatistics SourceReload::Statistics() const noexcept { return m_State->statistics; }

    std::optional<ReloadResult> SourceReload::Poll() noexcept
    {
        auto &state = *m_State;
        if (state.observationFailed)
        {
            return std::nullopt;
        }
        const auto source = state.sourceNotice.Poll();
        const auto schemas = state.schemaNotice.Poll();
        if (std::holds_alternative<Failure>(source) || std::holds_alternative<Failure>(schemas) ||
            state.requested == (std::numeric_limits<std::uint64_t>::max)())
        {
            state.observationFailed = true;
            state.queue.Cancel(state.job);
            return ReloadResult{.request = state.requested, .failure =
                MakeFailure(ErrorCode::IoFailure, "Shader reload source observation stopped.")};
        }
        const auto now = ReloadClock::now();
        // 同一轮同时收到源和接口通知只推进一个请求. 新编号立即可见, 提交延后到保存静默期结束.
        if (std::get<bool>(source) || std::get<bool>(schemas))
        {
            ++state.requested;
            state.quietUntil = now + std::chrono::milliseconds(50);
            state.queue.Cancel(state.job);
        }
        if (auto completed = state.queue.TakeCompleted())
        {
            // 缓存计数记录 worker 实际做过的工作. 完成包能否交给 Renderer 另按当前请求号判断.
            completed->statistics.discarded = state.statistics.discarded;
            state.statistics = completed->statistics;
            if (completed->result.request == state.requested)
            {
                return std::move(completed->result);
            }
            ++state.statistics.discarded;
        }
        if (state.submitted != state.requested && now >= state.quietUntil)
        {
            const auto request = GuardBuild<bool>([&] {
                const auto submitted = state.queue.TrySubmit(
                    std::make_unique<ReloadCompiler::Input>(ReloadCompiler::Input{state.compiler, state.requested}));
                if (const auto *id = std::get_if<State::Queue::Id>(&submitted))
                {
                    state.job = *id;
                    state.submitted = state.requested;
                    return true;
                }
                if (std::get<Core::JobRejection>(submitted) != Core::JobRejection::Full)
                {
                    throw BuildError(ErrorCode::SystemFailure, "Shader compilation queue is unavailable.");
                }
                return false;
            });
            if (const auto *failure = std::get_if<Failure>(&request))
            {
                state.submitted = state.requested;
                return ReloadResult{.request = state.requested, .failure = *failure};
            }
        }
        return std::nullopt;
    }
}
