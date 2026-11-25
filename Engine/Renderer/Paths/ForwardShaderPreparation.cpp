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

    bool ShaderDemand::ContainsRole(ProductShader role) const noexcept
    {
        assert(role < ProductShader::Count);
        return m_Keys[std::size_t(role)].any();
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

    /// @brief 在包的 ABI 和反射已验证后筛选各阶段可绑定的资源, 再逐组核对配方列出的必需项.
    /// 冷包在创建设备程序前调用, 热重载在构造候选程序表前遍历所有角色.
    static std::optional<Shader::Failure> CheckStaticForwardResources(const Shader::ShaderPackage &package,
                                                                    ProductShader role) noexcept
    {
        const auto groups = GeometryProgramResources(role);
        const bool perPass = std::ranges::any_of(groups, [](const auto &group) { return group.set == 3; });
        const bool froxels = IsFroxelProgram(role);
        for (const auto &module : package.modules)
        {
            const bool fragment = module.stage == Shader::Stage::Fragment;
            const auto unexpected = std::ranges::find_if(module.resources, [&](const auto &resource)
            {
                // 生成 ABI 已核对各槽的类型和允许阶段. 此处确认此角色确有对应布局及图绑定.
                switch (resource.set)
                {
                case 0:
                    return !(froxels && fragment) && (resource.kind != Shader::ResourceKind::ConstantBuffer ||
                                                       resource.slot != 0 || resource.name != "perView");
                case 1: return fragment;
                case 2: return false;
                case 3: return !fragment || !perPass;
                default: return true;
                }
            });
            if (unexpected != module.resources.end())
            {
                Core::BoundedText issue(module.sourcePath);
                issue.Append(": active resource has no binding for this Forward role: ");
                issue.Append(unexpected->name);
                return Shader::Failure{Shader::ErrorCode::InterfaceMismatch, {.description = issue}};
            }
        }
        // 几何位置依赖 View 变换及对象和实例行, 即使冷包从未在当前帧创建原生 Pipeline.
        static constexpr std::array<std::string_view, 1> View{"perView"};
        static constexpr std::array<std::string_view, 2> Renderable{"objectRows", "drawInstances"};
        static constexpr std::array vertex{
            Shader::ExpectedActiveResources{Shader::Stage::Vertex, 0, View},
            Shader::ExpectedActiveResources{Shader::Stage::Vertex, 1, Renderable}};
        if (const auto failure = Shader::CheckActiveResources(package, vertex))
        {
            return failure;
        }
        // 深度和阴影诊断的现有配方列出了 set 2 覆盖组, 这些角色允许片元省略 b0.
        // 普通受光的片元依赖 b0, Froxel 角色还需同一 View 的四个网格块.
        const bool coverageOnly = std::ranges::any_of(groups, [](const auto &group) { return group.set == 2; });
        if (!coverageOnly)
        {
            // 法线 UV 基底由颜色顶点入口从材质行取得, 此处独立于片元的完整材质配方核对.
            static constexpr std::array<std::string_view, 1> VertexMaterial{"materialRows"};
            static constexpr std::array<std::string_view, 5> FroxelView{
                "perView", "froxelView", "froxelHeaders", "froxelIndices", "froxelGlobalLights"};
            // 完整颜色共用材质诊断及受光入口. 基色的纹理 alpha 与数值因子共同决定 MASK 覆盖.
            static constexpr std::array<std::string_view, 11> ColorMaterial{
                "materialRows", "baseColorImage", "baseColorSampler", "metallicRoughnessImage",
                "metallicRoughnessSampler", "normalImage", "normalSampler", "occlusionImage",
                "occlusionSampler", "emissiveImage", "emissiveSampler"};
            const std::array color{
                Shader::ExpectedActiveResources{Shader::Stage::Vertex, 2, VertexMaterial},
                Shader::ExpectedActiveResources{Shader::Stage::Fragment, 0,
                    froxels ? std::span<const std::string_view>(FroxelView) : std::span<const std::string_view>(View)},
                Shader::ExpectedActiveResources{Shader::Stage::Fragment, 2, ColorMaterial}};
            if (const auto failure = Shader::CheckActiveResources(package, color))
            {
                return failure;
            }
        }
        return Shader::CheckActiveResources(package, groups);
    }

    ProductProgramResult LoadProductProgram(RHI::Device &device, ProductShader role,
        const std::filesystem::path &path, const Shader::ProductPackages &packages, std::string_view abi,
        std::span<const Shader::ExpectedActiveResources> expected)
    {
        const auto *program = Shader::FindProductProgram(role);
        assert(program);
        const auto vertex = "Source/" + std::string(program->vertex);
        const auto fragment = "Source/" + std::string(program->fragment);
        using Shader::ErrorCode;
        const auto create = [&](const Shader::ShaderPackage &package) -> ProductProgramResult
        {
            if (abi == StaticAbi::AbiHash && package.abiHash == abi)
            {
                if (const auto failure = CheckStaticForwardResources(package, role))
                {
                    return ProductProgramFailure{*failure, true};
                }
            }
            auto created = Shader::CreateRhiProgram(device, package, abi, expected, fragment, vertex);
            if (const auto *failure = std::get_if<Shader::Failure>(&created))
            {
                // 阶段、源入口和接口的核对先于原生创建. 设备或资源失败仍交推进边界处理.
                const bool rejectsRole = failure->code == ErrorCode::InterfaceMismatch ||
                                         failure->code == ErrorCode::UnsupportedProfile;
                return ProductProgramFailure{*failure, rejectsRole};
            }
            return std::get<Shader::RhiProgram>(std::move(created));
        };
        if (const auto &package = packages[std::size_t(role)]; package)
        {
            return create(*package);
        }
        const auto loaded = Shader::ReadPackage(path, abi);
        if (const auto *failure = std::get_if<Shader::Failure>(&loaded))
        {
            // 打开、长度和包内容检查针对该角色共用的磁盘输入. 内存不足和系统失败仍结束本次推进.
            const bool rejectsRole = failure->code != ErrorCode::OutOfMemory && failure->code != ErrorCode::SystemFailure;
            return ProductProgramFailure{*failure, rejectsRole};
        }
        return create(std::get<Shader::ShaderPackage>(loaded));
    }

    /// @brief CPU 包级准入已拒绝该角色的程序. 更换固定状态槽无法修复同一个包.
    struct ShaderPackageRejection
    {
        Core::BoundedText description;
    };

    void RejectProductProgram(std::string_view roleName, const ProductProgramFailure &failure)
    {
        Core::BoundedText description(roleName);
        description.Append(": ");
        description.Append(failure.cause.context.description.View());
        if (failure.rejectsRole)
        {
            throw ShaderPackageRejection{description};
        }
        throw ForwardFailure{ViewSettingReason::ScenePipelineUnavailable, description};
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

    // 失败记录已经按本帧需求裁剪. 包级拒绝覆盖同角色的所有槽, 其它失败匹配对应原生键.
    static const ShaderPreparationFailure *FindFailure(std::span<const ShaderPreparationFailure> failures,
                                                       ShaderPipelineRequest key) noexcept
    {
        const auto found = std::ranges::find_if(failures, [key](const auto &failure) {
            return failure.role == key.role && (!failure.variant || failure.variant == key.variant);
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

    // 按目录顺序寻找需求中的下一个缺口. 活动代跳过已拒的角色或单键, 候选代按自己的包和表重新核对.
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

    // 包级准入遍历全部角色, 包括当前 View 未选中的角色. 固定布局冲突应在替换活动代之前报告.
    // 角色通过 CPU 检查后才复用原生槽. 缺少的 Pipeline 随各 View 的实际需求分批创建.
    static void AdmitCandidate(ForwardDevice &device, std::optional<ShaderPipelineRequest> &checking)
    {
        const auto &result = *device.candidateInput;
        const auto &active = *device.activeShaders;
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
                if (const auto failure = CheckStaticForwardResources(*package, role))
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

    void ForwardDevice::AdvanceShaders(std::uint64_t request, std::optional<Shader::ReloadResult> result,
                                       bool continueActive) noexcept
    {
        lastShaderFailure.reset();
        auto &statistics = shaderStatistics;
        statistics.started = continueActive ? statistics.started : 0;
        statistics.demand = shaderDemand.Count();
        if (request < statistics.request)
        {
            return;
        }
        if (!continueActive)
        {
            ObserveShaderRequest(*this, request, std::move(result));
            // 只在新的帧前轮次重算退出计划的错误. 同帧的补备不撤销首轮失败的键或额度.
            std::erase_if(activeFailures, [&](const auto &failure) {
                return failure.variant ? !shaderDemand.Contains({failure.role, *failure.variant}) :
                                         !shaderDemand.ContainsRole(failure.role);
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
            if (!continueActive && candidateInput && !candidateShaders && !sourceFailure &&
                retiredShaders.size() <= shaderLimits.frames)
            {
                AdmitCandidate(*this, key);
            }
            // 共享键优先接续仍有效的对象, 设备额度只约束需要新增的 Pipeline.
            const auto advance = [&](ForwardShaderGeneration &generation, const Shader::ProductPackages &packages,
                                     const ForwardShaderGeneration *other) {
                const bool active = phase == ShaderPreparationPhase::Active;
                using Failures = std::span<const ShaderPreparationFailure>;
                while ((key = NextMissing(shaderDemand, generation, active ? Failures{activeFailures} : Failures{})))
                {
                    if (ReuseLivePipeline(*this, generation, packages, other, *key))
                    {
                        ++statistics.nativeHits;
                        continue;
                    }
                    if (statistics.started >= statistics.budget || statistics.inCache >= statistics.cacheLimit)
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
                    catch (const ShaderPackageRejection &error)
                    {
                        statistics.nativeSeconds += std::chrono::duration<double>(PrepareClock::now() - start).count();
                        if (!active)
                        {
                            throw;
                        }
                        // 包准入失败发生在原生创建前. 记录角色首因后, 剩余额度继续服务其它角色.
                        assert(activeFailures.size() < activeFailures.capacity());
                        activeFailures.push_back({key->role, error.description, std::nullopt});
                        continue;
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
            advance(*activeShaders, activePackages, candidateShaders.get());
            phase = ShaderPreparationPhase::Candidate;
            if (!continueActive && candidateShaders)
            {
                advance(*candidateShaders, candidateInput->packages, activeShaders.get());
            }
        }
        catch (const ShaderPackageRejection &error)
        {
            fail(error.description);
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
