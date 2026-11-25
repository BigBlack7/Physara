#include "ForwardShaderKeys.hpp"

/// @file
/// @brief 推进有限的 Forward 原生候选, 以当前需求选择完整代, 沿实际帧槽保活旧表.

#include <algorithm>
#include <cassert>
#include <chrono>
#include <limits>
#include <span>

#include <DisplayPost/Data.hpp>
#include <ForwardDisplay/Data.hpp>
#include <ForwardSky/Data.hpp>
#include <Shader/Package.hpp>

#include "../Materials/StaticGpu.hpp"

namespace Physara::Renderer::Detail
{
    using Shader::ProductShader;
    using PrepareClock = std::chrono::steady_clock;

    void ShaderDemand::Add(ShaderPipelineRequest key) noexcept
    {
        assert(key.role < ProductShader::Count && key.variant < GeometryPipelineSlots);
        m_Keys[std::size_t(key.role)][key.variant] = true;
    }

    bool ShaderDemand::Contains(ShaderPipelineRequest key) const noexcept
    {
        assert(key.role < ProductShader::Count && key.variant < GeometryPipelineSlots);
        return m_Keys[std::size_t(key.role)][key.variant];
    }

    std::size_t ShaderDemand::Count() const noexcept
    {
        std::size_t count = 0;
        for (const auto &keys : m_Keys)
        {
            count += keys.count();
        }
        return count;
    }

    Shader::Result<Shader::RhiProgram> LoadProductProgram(RHI::Device &device, ProductShader role,
        const std::filesystem::path &path, const Shader::ProductPackages &packages, std::string_view abi,
        std::span<const Shader::ExpectedActiveResources> expected)
    {
        const auto *program = Shader::FindProductProgram(role);
        assert(program);
        const auto vertex = "Source/" + std::string(program->vertex);
        const auto fragment = "Source/" + std::string(program->fragment);
        if (const auto &package = packages[std::size_t(role)]; package)
        {
            return Shader::CreateRhiProgram(device, *package, abi, expected, fragment, vertex);
        }
        const auto loaded = Shader::ReadPackage(path, abi);
        if (const auto *failure = std::get_if<Shader::Failure>(&loaded))
        {
            return *failure;
        }
        return Shader::CreateRhiProgram(device, std::get<Shader::ShaderPackage>(loaded), abi, expected, fragment, vertex);
    }

    // 只查询指定代已有的句柄. 显示槽按目标格式选择, 几何槽按阶段和固定状态选择.
    static RHI::GraphicsPipelineHandle FindPipeline(const ForwardShaderGeneration &generation,
                                                    ShaderPipelineRequest key) noexcept
    {
        if (PostProcess::DisplayDevice::IsRole(key.role))
        {
            return generation.display[PostProcess::DisplayDevice::ProgramIndex(key.role)].variants[key.variant];
        }
        return generation.programs.FindPipeline(key);
    }

    // 失败记录已经按本帧需求裁剪, 以角色和固定状态槽取得这项工作的首因.
    static const ShaderPreparationFailure *FindFailure(std::span<const ShaderPreparationFailure> failures,
                                                       ShaderPipelineRequest key) noexcept
    {
        const auto found = std::ranges::find_if(failures, [key](const auto &failure) {
            return failure.role == key.role && failure.variant == key.variant;
        });
        return found == failures.end() ? nullptr : &*found;
    }

    RHI::GraphicsPipelineHandle ForwardDevice::RequestShader(ShaderPipelineRequest key)
    {
        shaderDemand.Add(key);
        if (auto handle = FindPipeline(*activeShaders, key))
        {
            ++shaderStatistics.nativeHits;
            return handle;
        }
        if (const auto *failure = FindFailure(activeFailures, key))
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, failure->text.View());
        }
        return {};
    }

    bool ForwardDevice::RequestDisplay(const ViewLayouts &layouts, DisplaySettings settings)
    {
        const auto requests = PostProcess::DisplayDevice::SelectStages(layouts, settings);
        for (const auto key : requests.Stages())
        {
            shaderDemand.Add(key);
        }
        bool ready = true;
        for (const auto key : requests.Stages())
        {
            if (FindPipeline(*activeShaders, key))
            {
                ++shaderStatistics.nativeHits;
            }
            else if (const auto *failure = FindFailure(activeFailures, key))
            {
                Reject(ViewSettingReason::ScenePipelineUnavailable, failure->text.View());
            }
            else
            {
                ready = false;
            }
        }
        return ready;
    }

    // 以正在运行的 C++ 消费者所编入的接口为准, 相同接口名的多个程序角色共用这一项检查.
    static std::string_view ConsumerAbi(ProductShader role) noexcept
    {
        namespace Generated = Shader::Generated;
        switch (role)
        {
        case ProductShader::ForwardSky: return Generated::ForwardSky::AbiHash;
        case ProductShader::ForwardDisplay: return Generated::ForwardDisplay::AbiHash;
        case ProductShader::DisplayColorGrading:
        case ProductShader::DisplayFxaa:
        case ProductShader::DisplayScale: return Generated::DisplayPost::AbiHash;
        default: return StaticAbi::AbiHash;
        }
    }

    // 按目录顺序寻找一项已登记的缺口. Active 可跳过已有失败记录的键, 候选从自己的表重新核对.
    static std::optional<ShaderPipelineRequest> NextMissing(const ShaderDemand &demand,
        const ForwardShaderGeneration &generation, std::span<const ShaderPreparationFailure> rejected = {}) noexcept
    {
        for (const auto &program : Shader::ProductPrograms())
        {
            for (std::size_t variant = 0; variant < GeometryPipelineSlots; ++variant)
            {
                const ShaderPipelineRequest key{program.key.role, variant};
                if (demand.Contains(key) && !FindPipeline(generation, key) && !FindFailure(rejected, key))
                {
                    return key;
                }
            }
        }
        return std::nullopt;
    }

    // 按 RHI Pipeline 句柄指向的对象去重, 将 Active, 候选及仍被帧持有的旧代合计为设备占用.
    static std::size_t CountPipelines(ForwardDevice &device) noexcept
    {
        // 同一句柄可在多张表中出现. 装配时按所有表的槽数预留, 收集完成后才按唯一对象计费.
        auto &handles = device.countedPipelines;
        handles.clear();
        const auto add = [&](const ForwardShaderGeneration &generation) {
            const auto insert = [&](const auto &handle) {
                if (handle)
                {
                    assert(handles.size() < handles.capacity());
                    handles.push_back(handle.Get());
                }
            };
            for (const auto &pipeline : generation.programs.pipelines)
            {
                insert(pipeline);
            }
            insert(generation.programs.skyPipeline);
            for (const auto &stage : generation.display)
            {
                for (const auto &pipeline : stage.variants)
                {
                    insert(pipeline);
                }
            }
        };
        add(*device.activeShaders);
        if (device.candidateShaders)
        {
            add(*device.candidateShaders);
        }
        for (const auto &entry : device.retiredShaders)
        {
            if (const auto generation = entry.lock())
            {
                add(*generation);
            }
        }
        std::ranges::sort(handles, std::less{});
        const auto count = static_cast<std::size_t>(std::ranges::unique(handles).begin() - handles.begin());
        handles.clear();
        return count;
    }

    /// @brief 用包摘要和角色的固定配方确定原生对象身份. 两张表共享设备的固定绑定布局.
    static bool ReusePipeline(ForwardShaderGeneration &target, const ForwardShaderGeneration &source,
                              const Shader::ProductPackages &packages, ShaderPipelineRequest key)
    {
        assert(target.device == source.device && target.driver.View() == source.driver.View());
        const auto &package = packages[std::size_t(key.role)];
        if (!PostProcess::DisplayDevice::IsRole(key.role))
        {
            const auto content = package ? std::string_view(package->contentHash)
                                        : target.programs.ProgramContent(key.role);
            return !content.empty() && target.programs.ReusePipeline(source.programs, key, content);
        }
        const auto index = PostProcess::DisplayDevice::ProgramIndex(key.role);
        auto &next = target.display[index];
        const auto &current = source.display[index];
        const auto content = package ? std::string_view(package->contentHash) : std::string_view(next.shaders.contentHash);
        const auto &pipeline = current.variants[key.variant];
        if (!pipeline || current.shaders.contentHash != content)
        {
            return false;
        }
        if (!next.shaders.vertex)
        {
            auto shaders = current.shaders;
            next.shaders = std::move(shaders);
        }
        next.variants[key.variant] = pipeline;
        return true;
    }

    // 先查本轮另一张表, 再借用仍由帧持有的旧表. 只接续兼容对象, 不延长旧表的寿命.
    static bool ReuseLivePipeline(const ForwardDevice &device, ForwardShaderGeneration &target,
        const Shader::ProductPackages &packages, const ForwardShaderGeneration *other, ShaderPipelineRequest key)
    {
        if (other && ReusePipeline(target, *other, packages, key))
        {
            return true;
        }
        for (const auto &entry : device.retiredShaders)
        {
            if (const auto retired = entry.lock(); retired && ReusePipeline(target, *retired, packages, key))
            {
                return true;
            }
        }
        return false;
    }

    /// @brief 核对各阶段在 PerView 中实际使用的资源是否能由现有 Forward 布局提供.
    /// @pre 包已核对 StaticForward ABI. 当前所有几何角色共用同一份 PerView 常量绑定.
    /// @return 活动绑定均可提供时返回空. 首项不匹配时返回阶段源路径和资源名, 供准入方报告该角色.
    static std::optional<Core::BoundedText> CheckCandidatePerViewBindings(const Shader::ShaderPackage &package,
                                                                          const RHI::BindingLayoutDesc &layout) noexcept
    {
        for (const auto &module : package.modules)
        {
            const auto unsupported = std::ranges::find_if(module.resources, [&](const auto &resource) {
                if (resource.set != layout.registerSpace)
                {
                    return false;
                }
                return resource.kind != Shader::ResourceKind::ConstantBuffer ||
                       std::ranges::none_of(layout.bindings, [&](const auto &binding) {
                           return binding.type == RHI::ResourceType::ConstantBuffer && binding.slot == resource.slot;
                       });
            });
            if (unsupported != module.resources.end())
            {
                Core::BoundedText issue(module.sourcePath);
                issue.Append(": active resource has no Forward PerView binding: ");
                issue.Append(unsupported->name);
                return issue;
            }
        }
        return std::nullopt;
    }

    // 包级准入遍历全部角色, 包括当前 View 未选中的角色. 固定布局冲突应在替换活动代之前报告.
    // 角色通过 CPU 检查后才复用原生槽. 缺少的 Pipeline 随各 View 的实际需求分批创建.
    static void AdmitCandidate(ForwardDevice &device, std::optional<ShaderPipelineRequest> &checking)
    {
        const auto &result = *device.candidateInput;
        const auto &active = *device.activeShaders;
        const auto &perView = *active.programs.layouts.front()->GetDesc();
        for (const auto &program : Shader::ProductPrograms())
        {
            const auto role = program.key.role;
            const auto &package = result.packages[std::size_t(role)];
            const auto abi = ConsumerAbi(role);
            if (!package || package->abiHash != abi)
            {
                checking = ShaderPipelineRequest{role, 0};
                Reject(ViewSettingReason::ScenePipelineUnavailable,
                       "Candidate Shader ABI does not match the interface compiled into the running renderer");
            }
            if (abi == StaticAbi::AbiHash)
            {
                if (const auto failure = CheckCandidatePerViewBindings(*package, perView))
                {
                    checking = ShaderPipelineRequest{role, 0};
                    Reject(ViewSettingReason::ScenePipelineUnavailable, failure->View());
                }
            }
            if (const auto expected = ShadowProgramResources(role); !expected.empty())
            {
                // 当前 View 未请求阴影时也核对变体绑定, 保证已发布代可在后续帧正确准备.
                if (const auto failure = Shader::CheckActiveResources(*package, expected))
                {
                    checking = ShaderPipelineRequest{role, 0};
                    Reject(ViewSettingReason::ScenePipelineUnavailable, failure->context.description.View());
                }
            }
        }
        // 原生表只可在同一设备及驱动身份接续. 角色对应的包内容、ABI 与固定描述确定表中各槽.
        if (active.device != &device.device || active.driver.View() != device.shaderDevice.View())
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Native Shader table belongs to a different device or driver");
        }
        auto candidate = std::make_shared<ForwardShaderGeneration>(active);
        for (const auto &program : Shader::ProductPrograms())
        {
            const auto role = program.key.role;
            const auto &hash = result.packages[std::size_t(role)]->contentHash;
            if (PostProcess::DisplayDevice::IsRole(role))
            {
                auto &display = candidate->display[PostProcess::DisplayDevice::ProgramIndex(role)];
                if (display.shaders.contentHash != hash)
                {
                    display = {};
                }
            }
            else if (candidate->programs.ProgramContent(role) != hash)
            {
                candidate->programs.ReplaceProgram(role);
            }
        }
        if (candidate->generation == (std::numeric_limits<std::uint64_t>::max)())
        {
            Reject(ViewSettingReason::ScenePipelineUnavailable, "Shader generation identities exhausted");
        }
        ++candidate->generation;
        device.candidateShaders = std::move(candidate);
    }

    // 只归还候选表的原生句柄. 是否保留已编译的 CPU 包, 由取消或失败的调用方决定.
    static void ReleaseCandidateTable(ForwardDevice &device) noexcept
    {
        device.shaderCacheDirty |= bool(device.candidateShaders);
        device.candidateShaders.reset();
    }

    // CPU 回执和发布前通知使用同一入口. 新修订使旧候选及旧请求的失败去重记录同时失效.
    static void ObserveShaderRequest(ForwardDevice &device, std::uint64_t request,
                                     std::optional<Shader::ReloadResult> result) noexcept
    {
        if (device.shaderStatistics.request != request)
        {
            device.shaderStatistics.request = request;
            device.sourcePending = request != 0;
            device.sourceFailure.reset();
            device.activeFailures.clear();
            device.candidateInput.reset();
            device.retryOnReselection = false;
            ReleaseCandidateTable(device);
        }
        if (result && result->request == request)
        {
            if (result->failure)
            {
                device.sourceFailure = ShaderPreparationFailure{result->failedRole, result->failure->context.description};
                device.sourcePending = false;
                device.candidateInput.reset();
                device.retryOnReselection = false;
                ReleaseCandidateTable(device);
            }
            else if (device.sourcePending && !device.candidateInput)
            {
                // 每个请求只接纳一次完整输入. 重复交付同一回执会保留原生失败键的重试状态.
                const auto matched = std::ranges::equal(device.activePackages, result->packages,
                    [](const auto &current, const auto &next) {
                        return current && next && current->contentHash == next->contentHash;
                    });
                if (!matched)
                {
                    device.candidateInput = std::move(result);
                }
                device.sourcePending = !matched;
            }
        }
    }

    // 更新面板事实时只读取准备器的表. 就绪代与最新请求的失败状态可以同时有效.
    static void UpdateShaderStatus(ForwardDevice &device) noexcept
    {
        auto &statistics = device.shaderStatistics;
        if (device.shaderCacheDirty)
        {
            statistics.inCache = CountPipelines(device);
            device.shaderCacheDirty = false;
        }
        const auto missing = NextMissing(device.shaderDemand, *device.activeShaders);
        if (statistics.demand && !missing)
        {
            statistics.active = device.activeShaders->generation;
        }
        const auto *failure = device.sourceFailure ? &*device.sourceFailure :
            device.displayFailure ? &*device.displayFailure :
            device.activeFailures.empty() ? nullptr : &device.activeFailures.back();
        if (failure)
        {
            statistics.state = ShaderPreparationState::Failed;
            statistics.role = failure->role;
            statistics.reason = failure->text;
        }
        else if (device.sourcePending || missing || !statistics.active)
        {
            statistics.state = ShaderPreparationState::Pending;
            const auto candidateMissing = device.candidateShaders
                ? NextMissing(device.shaderDemand, *device.candidateShaders) : std::nullopt;
            statistics.role = candidateMissing ? candidateMissing->role : missing ? missing->role : ProductShader::Count;
            if (device.candidateShaders && !candidateMissing)
            {
                statistics.reason = Core::BoundedText("Waiting for an eligible frame boundary");
            }
            else if (candidateMissing || missing)
            {
                statistics.reason = Core::BoundedText(statistics.inCache >= statistics.cacheLimit
                    ? "Waiting for native Shader cache capacity" : "Waiting for frame preparation budget");
            }
            else
            {
                statistics.reason = Core::BoundedText("Waiting for Shader inputs");
            }
        }
        else
        {
            statistics.state = ShaderPreparationState::Active;
            statistics.role = ProductShader::Count;
            statistics.reason.reset();
        }
    }

    void ForwardDevice::AdvanceShaders(std::uint64_t request, std::optional<Shader::ReloadResult> result) noexcept
    {
        lastShaderFailure.reset();
        auto &statistics = shaderStatistics;
        statistics.started = 0;
        statistics.demand = shaderDemand.Count();
        if (request < statistics.request)
        {
            return;
        }
        ObserveShaderRequest(*this, request, std::move(result));
        // 只移除退出计划的错误, 其余键各自保留原因和失败顺序.
        std::erase_if(activeFailures, [&](const auto &failure) {
            return !shaderDemand.Contains({failure.role, *failure.variant});
        });
        if (candidateInput && sourceFailure && sourceFailure->variant)
        {
            // 先观察到失败键离开计划, 再观察到同一键重新出现, 才用保留的包重建原生表.
            const bool requested = shaderDemand.Contains({sourceFailure->role, *sourceFailure->variant});
            if (requested && retryOnReselection)
            {
                sourceFailure.reset();
                sourcePending = true;
            }
            retryOnReselection = !requested;
        }
        if (std::erase_if(retiredShaders, [](const auto &entry) { return entry.expired(); }) || shaderCacheDirty)
        {
            statistics.inCache = CountPipelines(*this);
            shaderCacheDirty = false;
        }

        auto phase = ShaderPreparationPhase::Admission;
        std::optional<ShaderPipelineRequest> key;
        const auto fail = [&](const Core::BoundedText &description) {
            lastShaderFailure = phase;
            if (phase == ShaderPreparationPhase::Active)
            {
                assert(key && activeFailures.size() < activeFailures.capacity());
                activeFailures.push_back({key->role, description, key->variant});
                return;
            }
            // 释放失败候选的原生对象. 原生键可在同一源码修订内重选, 继续借用原 CPU 结果.
            ReleaseCandidateTable(*this);
            const auto role = key ? key->role : ProductShader::Count;
            sourceFailure = ShaderPreparationFailure{role, description};
            sourcePending = false;
            if (phase == ShaderPreparationPhase::Candidate)
            {
                assert(key);
                sourceFailure->variant = key->variant;
            }
            else
            {
                candidateInput.reset();
            }
        };
        try
        {
            if (candidateInput && !candidateShaders && !sourceFailure && retiredShaders.size() <= shaderLimits.frames)
            {
                AdmitCandidate(*this, key);
            }
            // 共享键优先接续仍有效的对象, 设备额度只约束需要新增的 Pipeline.
            const auto advance = [&](ForwardShaderGeneration &generation, const Shader::ProductPackages &packages,
                                     const ForwardShaderGeneration *other,
                                     std::span<const ShaderPreparationFailure> rejected) {
                while ((key = NextMissing(shaderDemand, generation, rejected)))
                {
                    if (ReuseLivePipeline(*this, generation, packages, other, *key))
                    {
                        ++statistics.nativeHits;
                        continue;
                    }
                    if (statistics.started == statistics.budget || statistics.inCache >= statistics.cacheLimit)
                    {
                        return;
                    }
                    const auto start = PrepareClock::now();
                    ++statistics.started;
                    try
                    {
                        if (PostProcess::DisplayDevice::IsRole(key->role))
                        {
                            display.PrepareStage(generation.display, *key, packages);
                        }
                        else
                        {
                            generation.programs.PreparePipeline(*this, *key, packages);
                        }
                    }
                    catch (...)
                    {
                        statistics.nativeSeconds += std::chrono::duration<double>(PrepareClock::now() - start).count();
                        throw;
                    }
                    statistics.nativeSeconds += std::chrono::duration<double>(PrepareClock::now() - start).count();
                    ++statistics.nativeCreated;
                    ++statistics.inCache;
                }
            };

            // 优先让当前有效代应对新的 View 需求, 余下额度用于源候选.
            phase = ShaderPreparationPhase::Active;
            advance(*activeShaders, activePackages, candidateShaders.get(), activeFailures);
            phase = ShaderPreparationPhase::Candidate;
            if (candidateShaders)
            {
                advance(*candidateShaders, candidateInput->packages, activeShaders.get(), {});
            }
        }
        catch (const ForwardFailure &error)
        {
            fail(error.description);
        }
        catch (const Core::BoundedText &error)
        {
            fail(error);
        }
        catch (const std::exception &error)
        {
            fail(Core::BoundedText(error.what()));
        }
        UpdateShaderStatus(*this);
    }

    bool ForwardDevice::ShaderCandidateReady() const noexcept
    {
        return candidateShaders && !displayFailure && shaderDemand.Count() != 0 &&
               retiredShaders.size() <= shaderLimits.frames && !NextMissing(shaderDemand, *candidateShaders);
    }

    void ForwardDevice::RecordShaderDiagnostic(const Core::BoundedText &reason) noexcept
    {
        if (lastShaderFailure)
        {
            auto &failure = *lastShaderFailure == ShaderPreparationPhase::Active
                ? activeFailures.back() : *sourceFailure;
            failure.text.Append("\n");
            failure.text.Append(reason.View());
            lastShaderFailure.reset();
            UpdateShaderStatus(*this);
        }
    }

    void ForwardDevice::PublishShaders(std::uint64_t request, std::optional<Shader::ReloadResult> result) noexcept
    {
        if (request < shaderStatistics.request)
        {
            return;
        }
        lastShaderFailure.reset();
        ObserveShaderRequest(*this, request, std::move(result));
        if (ShaderCandidateReady())
        {
            // 退休槽和计数空间已预留. 从决定切换到交付新统计, 无分配或原生准备可能拆开事务.
            if (activeShaders.use_count() > 1)
            {
                retiredShaders.push_back(activeShaders);
            }
            activeShaders = std::move(candidateShaders);
            activePackages = std::move(candidateInput->packages);
            candidateInput.reset();
            activeFailures.clear();
            sourcePending = false;
            shaderStatistics.active = activeShaders->generation;
            shaderCacheDirty = true;
        }
        UpdateShaderStatus(*this);
    }
}
