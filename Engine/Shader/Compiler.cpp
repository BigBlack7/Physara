#include "Compiler.hpp"

/// @file
/// @brief 将程序请求与受控源图编译为 canonical SPIR-V, 核对最终反射后形成包身份.

#include <algorithm>
#include <array>
#include <chrono>
#include <map>
#include <set>
#include <sstream>

#include <glslang/Public/ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>

#include "BuildIdentity.hpp"
#include "SourceGraph.hpp"
#include "Spirv.hpp"

namespace Physara::Shader
{
    // 已校验程序的一阶段入口, source 仍是受控源根内的相对拼写.
    struct ProgramStage
    {
        Stage stage = Stage::Vertex;
        std::string source;
        std::string entry;
    };

    // 程序声明固定图形的 VS/FS 顺序或计算的单 CS, defines 按名称排序以稳定构建身份.
    struct Program
    {
        std::string name;
        std::string interfaceName;
        std::string profile;
        std::vector<ProgramStage> stages;
        std::map<std::string, std::uint32_t> defines;
    };

    // 按请求的 profile 校验阶段组合及 main 入口, 并拒绝不受支持的 define 值.
    static Program ParseProgram(const Json &json, std::string_view origin)
    try
    {
        CheckFields(json, {"version", "name", "interface", "profile", "stages", "defines"}, "/program");
        if (UInt(json.at("version"), 1, "/program/version") != 1)
        {
            throw BuildError(ErrorCode::UnsupportedProfile, "/program/version or profile is unsupported.");
        }
        Program program;
        program.profile = json.at("profile").get<std::string>();
        const bool compute = program.profile == ComputeProfile;
        if (!compute && program.profile != GraphicsProfile)
        {
            throw BuildError(ErrorCode::UnsupportedProfile, "/program/profile is unsupported.");
        }
        program.name = Identifier(json.at("name"), "/program/name");
        program.interfaceName = Identifier(json.at("interface"), "/program/interface");
        const auto &stages = json.at("stages");
        if (!stages.is_array() || stages.size() != (compute ? 1u : 2u))
        {
            throw BuildError(ErrorCode::UnsupportedProfile, "/program/stages has the wrong size for the requested profile.");
        }
        for (const auto &stage : stages)
        {
            CheckFields(stage, {"stage", "source", "entry"}, "/program/stages");
            program.stages.push_back({ParseStage(stage.at("stage").get<std::string>()),
                                      stage.at("source").get<std::string>(), stage.at("entry").get<std::string>()});
            if (program.stages.back().entry != "main")
            {
                throw BuildError(ErrorCode::UnsupportedProfile, "/program/stages/entry requires main.");
            }
        }
        std::ranges::sort(program.stages, {}, &ProgramStage::stage);
        if (compute ? program.stages.front().stage != Stage::Compute :
                      (program.stages[0].stage != Stage::Vertex || program.stages[1].stage != Stage::Fragment))
        {
            throw BuildError(ErrorCode::InvalidInput, "/program/stages does not match the requested profile.");
        }
        const auto &defines = json.at("defines");
        if (!defines.is_array() || defines.size() > 64)
        {
            throw BuildError(ErrorCode::InvalidInput, "/program/defines requires an array of at most 64 declarations.");
        }
        for (const auto &define : defines)
        {
            CheckFields(define, {"name", "allowed", "value"}, "/program/defines");
            const auto name = Identifier(define.at("name"), "/program/defines/name");
            const auto value = UInt(define.at("value"), 16, "/program/defines/value");
            const auto &allowed = define.at("allowed");
            if (!allowed.is_array() || allowed.empty() || allowed.size() > 8)
            {
                throw BuildError(ErrorCode::InvalidInput, "/program/defines/allowed requires 1..8 discrete values.");
            }
            std::set<std::uint32_t> choices;
            for (const auto &choice : allowed)
            {
                if (!choices.insert(UInt(choice, 16, "/program/defines/allowed")).second)
                {
                    throw BuildError(ErrorCode::InvalidInput, "/program/defines/allowed contains a duplicate.");
                }
            }
            if (!choices.contains(value) || !program.defines.emplace(name, value).second)
            {
                throw BuildError(ErrorCode::InvalidInput,
                                 "/program/defines has an undeclared value or duplicate name.");
            }
        }
        return program;
    }
    catch (const BuildError &error)
    {
        throw BuildError(error.GetFailure().code, std::string(origin) + ": " + error.what());
    }
    catch (const Json::exception &error)
    {
        throw BuildError(ErrorCode::InvalidInput, std::string(origin) + ": " + error.what());
    }

    // 本次已核对的生成接口快照, 真实路径及身份输入, 字节预算包含全部必要产物.
    struct InterfaceInput
    {
        InterfaceSchema schema;
        std::vector<BuildInput> files;
        Json identity;
        std::size_t bytes = 0;
    };

    // 图形接口核对五份产物, 计算接口还核对 Compute.glsl. 再从 Expected 独立推导布局和 ABI.
    static InterfaceInput ReadInterface(const std::filesystem::path &root, const Program &program)
    {
        // 先按 manifest 核对每份产物的字节. 读到 Expected.json 时再从其中的声明重算布局和 ABI.
        const auto manifestPath = CanonicalInputPath(root / "Manifest.json");
        const auto manifestText = ReadText(manifestPath, MaxDeclarationBytes);
        const auto manifest = ParseJson(manifestText, "Interface/Manifest.json");
        CheckFields(manifest, {"version", "name", "abiHash", "schemaHash", "artifacts"}, "Interface/Manifest.json");
        if (UInt(manifest.at("version"), 1, "manifest/version") != 1 || manifest.at("name") != program.interfaceName)
        {
            throw BuildError(ErrorCode::InterfaceMismatch, "Interface manifest version/name mismatch.");
        }
        constexpr std::array Names{"Data.hpp", "Bindings.hpp", "Vertex.glsl", "Fragment.glsl", "Expected.json",
                                   "Compute.glsl"};
        const auto &artifacts = manifest.at("artifacts");
        const bool compute = program.profile == ComputeProfile;
        const auto names = std::span(Names).first(Names.size() - (compute ? 0u : 1u));
        if (!artifacts.is_object() || artifacts.size() != names.size())
        {
            throw BuildError(ErrorCode::InterfaceMismatch, "Interface manifest does not match the requested profile.");
        }
        InterfaceInput input;
        input.files.push_back({manifestPath, HashText(manifestText)});
        input.bytes = manifestText.size();
        input.identity = {
            {"manifest", input.files.front().hash}, {"schema", manifest.at("schemaHash")}, {"artifacts", artifacts}};
        for (const auto *name : names)
        {
            const auto path = CanonicalInputPath(root / name);
            const auto text = ReadText(path, MaxPackageBytes);
            auto hash = HashText(text);
            if (hash != artifacts.at(name).get<std::string>())
            {
                throw BuildError(ErrorCode::InterfaceMismatch,
                                 std::string("Incomplete/mixed interface artifact: ") + name);
            }
            input.files.push_back({path, std::move(hash)});
            if (text.size() > MaxPackageBytes - input.bytes)
            {
                throw BuildError(ErrorCode::InvalidInput, "Generated interface exceeds input byte budget.");
            }
            input.bytes += text.size();
            if (std::string_view(name) == "Expected.json")
            {
                const auto expected = ParseJson(text, "Interface/Expected.json");
                CheckFields(expected, {"schema", "abiHash", "blocks", "bindings"}, "Interface/Expected.json");
                if (expected.at("schema").dump().size() > MaxDeclarationBytes)
                {
                    throw BuildError(ErrorCode::InvalidInput, "Expected schema exceeds declaration byte budget.");
                }
                input.schema = ParseInterfaceSchema(expected.at("schema"));
                Json bindings = Json::array();
                for (const auto &binding : input.schema.bindings)
                {
                    bindings.push_back({{"resource", binding.resource}, {"stages", binding.stageMask}});
                }
                if (input.schema.name != program.interfaceName ||
                    input.schema.abiHash != expected.at("abiHash").get<std::string>() ||
                    input.schema.abiHash != manifest.at("abiHash").get<std::string>() ||
                    Json(input.schema.blocks) != expected.at("blocks") || bindings != expected.at("bindings"))
                {
                    throw BuildError(ErrorCode::InterfaceMismatch, "Interface expected layout/ABI is inconsistent.");
                }
            }
        }
        if ((input.schema.localSize[0] != 0) != compute)
        {
            throw BuildError(ErrorCode::InterfaceMismatch, "Interface workgroup declaration does not match the profile.");
        }
        return input;
    }

    // 优先保留程序/阶段与编译器原因, 再按预算补充实际展开的 include 来源.
    static std::string CompileDiagnostic(std::string_view program, std::string_view stage, const char *log,
                                         const SourceGraph &graph)
    {
        std::string diagnostic = std::string(program) + " / " + std::string(stage) + ":\n";
        if (log)
        {
            diagnostic.append(std::string_view(log).substr(0, Core::MaxTextBytes / 2));
        }
        for (const auto &edge : graph.GetEdges())
        {
            if (edge.expanded && diagnostic.size() < Core::MaxTextBytes)
            {
                diagnostic += "\nIncluded from " + edge.includingPath + ":" + std::to_string(edge.line) + " -> " +
                              edge.includedPath;
            }
        }
        return diagnostic;
    }

    // 只在真正进入 glslang 时计时, 编译失败的调用也累计实际耗时.
    struct CompilationTimer
    {
        using Clock = std::chrono::steady_clock;
        CompilationStatistics *statistics;
        Clock::time_point start = statistics ? Clock::now() : Clock::time_point{};

        ~CompilationTimer()
        {
            if (statistics)
            {
                statistics->seconds += std::chrono::duration<double>(Clock::now() - start).count();
            }
        }
    };

    // 先核对产品入口的固定字段, 再保留目录返回的组合诊断. 同 ABI 不代表阶段源或结构值可互换.
    static void CheckProductProgram(const Program &program, VariantKey requested, std::string_view origin)
    {
        const auto *expected = FindProductProgram(requested.role);
        if (!expected)
        {
            throw BuildError(ErrorCode::UnsupportedProfile, std::string(origin) + ": unknown requested product role.");
        }
        const auto require = [&](std::string_view field, std::string_view actual, std::string_view approved)
        {
            if (actual != approved)
            {
                throw BuildError(ErrorCode::UnsupportedProfile, std::string(origin) + ": " + std::string(field) +
                    " = '" + std::string(actual) + "' is not approved; expected '" + std::string(approved) + "'.");
            }
        };
        require("/program/name", program.name, expected->name);
        require("/program/profile", program.profile, GraphicsProfile);
        require("/program/interface", program.interfaceName, expected->interfaceName);
        require("/program/stages/vertex/source", program.stages[0].source, expected->vertex);
        require("/program/stages/fragment/source", program.stages[1].source, expected->fragment);

        std::vector<VariantField> fields;
        for (const auto &[name, value] : program.defines)
        {
            fields.push_back({name, value});
        }
        const auto variant = ResolveVariant(program.name, fields);
        if (const auto *failure = std::get_if<Failure>(&variant))
        {
            throw BuildError(failure->code, std::string(origin) + ": " +
                             std::string(failure->context.description.View()));
        }
        if (const auto approved = std::get<VariantKey>(variant); approved != requested)
        {
            require("requested PHYSARA_PHYSICAL_LIGHTING", std::to_string(requested.physicalLighting),
                    std::to_string(approved.physicalLighting));
            require("requested PHYSARA_IOR_SPECULAR", std::to_string(requested.iorSpecular),
                    std::to_string(approved.iorSpecular));
            require("requested PHYSARA_CLEARCOAT", std::to_string(requested.clearcoat),
                    std::to_string(approved.clearcoat));
        }
    }

    Result<BuildOutput> Compiler::Build(const BuildRequest &request, CompilationStatistics *statistics) noexcept
    {
        return GuardBuild<BuildOutput>(
            [&]
            {
                const auto programPath = CanonicalInputPath(request.programPath);
                const auto interfaceRoot = CanonicalInputPath(request.interfaceRoot);
                const auto requestText = ReadText(programPath, MaxDeclarationBytes);
                const auto relative = request.programPath.lexically_relative(request.sourceRoot);
                const auto origin = PathText(!relative.empty() && *relative.begin() != ".." ? relative : programPath);
                const auto program = ParseProgram(ParseJson(requestText, origin), origin);
                if (request.variant)
                {
                    CheckProductProgram(program, *request.variant, origin);
                }
                const auto interfaceInput = ReadInterface(interfaceRoot, program);
                SourceGraph graph(request.sourceRoot, interfaceRoot);
                std::array<const SourceFile *, 2> sources{};
                for (std::size_t index = 0; index < program.stages.size(); ++index)
                {
                    try
                    {
                        sources[index] = &graph.AddSource(program.stages[index].source);
                    }
                    catch (const BuildError &error)
                    {
                        throw BuildError(error.GetFailure().code,
                                         program.name + " / " + std::string(StageName(program.stages[index].stage)) +
                                             " / main: " + error.what());
                    }
                }
                // 合并请求, 生成文件和传递源的预算; 生成 include 必须与前面核对的产物属于同一代.
                auto totalBytes = interfaceInput.bytes + requestText.size();
                auto totalFiles = interfaceInput.files.size() + 1;
                if (totalBytes > MaxPackageBytes)
                {
                    throw BuildError(ErrorCode::InvalidInput, "Program declarations/generated files exceed 64 MiB.");
                }
                for (const auto &[logical, file] : graph.GetFiles())
                {
                    if (logical.starts_with("Interface/"))
                    {
                        const auto name = logical.substr(10);
                        const auto &artifacts = interfaceInput.identity.at("artifacts");
                        if (!artifacts.contains(name) || HashText(file.text) != artifacts.at(name).get<std::string>())
                        {
                            throw BuildError(ErrorCode::InterfaceMismatch,
                                             "Unverified/changed generated include: " + logical);
                        }
                        // 同一生成文件只按一个输入计费, 并校验 glslang 实际借用的这一份字节.
                        continue;
                    }
                    if (file.text.size() > MaxPackageBytes - totalBytes)
                    {
                        throw BuildError(ErrorCode::InvalidInput, "Program total input exceeds 64 MiB.");
                    }
                    totalBytes += file.text.size();
                    ++totalFiles;
                }
                if (totalBytes > MaxPackageBytes || totalFiles > 256)
                {
                    throw BuildError(ErrorCode::InvalidInput, "Program input exceeds total byte/file budget.");
                }
                // 声明图完成后才进入编译器, defines 使用确定顺序, 源字节与名称借用保持到链接结束.
                std::string preamble;
                for (const auto &[name, value] : program.defines)
                {
                    preamble += "#define " + name + " " + std::to_string(value) + "\n";
                }
                constexpr std::array Languages{EShLangVertex, EShLangFragment, EShLangCompute};
                // 名称/长度数组与源内容一起活到 parse/link/生成全部结束. program 先于 shader 析构.
                std::array<std::unique_ptr<glslang::TShader>, 2> shaders;
                std::array<const char *, 2> texts{};
                std::array<const char *, 2> names{};
                std::array<int, 2> lengths{};
                const auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
                for (std::size_t index = 0; index < program.stages.size(); ++index)
                {
                    const auto text = sources[index]->GetCompilerText();
                    texts[index] = text.data();
                    names[index] = sources[index]->logicalPath.c_str();
                    lengths[index] = static_cast<int>(text.size());
                    const auto language = Languages[static_cast<std::size_t>(program.stages[index].stage)];
                    auto &shader = shaders[index];
                    shader = std::make_unique<glslang::TShader>(language);
                    shader->setStringsWithLengthsAndNames(&texts[index], &lengths[index], &names[index], 1);
                    shader->setPreamble(preamble.c_str());
                    shader->setEntryPoint("main");
                    shader->setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, 450);
                    shader->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
                    shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);
                    bool parsed;
                    {
                        const CompilationTimer timer{statistics};
                        if (statistics)
                        {
                            ++statistics->stages;
                        }
                        parsed = shader->parse(GetDefaultResources(), 450, ECoreProfile, false, false, messages, graph);
                    }
                    if (!parsed)
                    {
                        throw BuildError(ErrorCode::CompilationFailed,
                                         CompileDiagnostic(program.name, StageName(program.stages[index].stage),
                                                           shader->getInfoLog(), graph));
                    }
                }
                // 各阶段先进入同一程序链接. 从链接后的中间表示生成最终字节码, 供反射及包发布共用.
                glslang::TProgram linked;
                for (std::size_t index = 0; index < program.stages.size(); ++index)
                {
                    linked.addShader(shaders[index].get());
                }
                bool compiled;
                {
                    const CompilationTimer timer{statistics};
                    compiled = linked.link(messages);
                }
                if (!compiled)
                {
                    throw BuildError(ErrorCode::CompilationFailed,
                                     CompileDiagnostic(program.name, "link", linked.getInfoLog(), graph));
                }
                // 链接后的 IR 是反射与包载荷的共同来源, 不对另一次重编译的结果做旁路核对.
                BuildOutput output;
                auto &package = output.package;
                package.profile = program.profile;
                package.abiHash = interfaceInput.schema.abiHash;
                package.declaration = SnapshotInterface(interfaceInput.schema);
                const Json options{
                    {"rule", 1},          {"source", "GLSL450"},   {"client", "Vulkan1.1"},  {"target", "SPIRV1.3"},
                    {"optimizer", false}, {"autoBindings", false}, {"autoLocations", false}, {"debugInfo", false}};
                package.compilerFingerprint = HashText(Json{
                    {"inputs", CompilerInputHash},
                    {"configuration", BuildConfiguration},
                    {"options", options},
                    {"hostFlags", CompilerFlagHash}}.dump());
                for (std::size_t index = 0; index < program.stages.size(); ++index)
                {
                    ShaderModule module;
                    module.stage = program.stages[index].stage;
                    module.entry = "main";
                    module.sourcePath = sources[index]->logicalPath;
                    glslang::SpvOptions spirvOptions;
                    spirvOptions.disableOptimizer = true;
                    spirvOptions.generateDebugInfo = false;
                    spirvOptions.stripDebugInfo = false;
                    spirvOptions.validate = false;
                    spv::SpvBuildLogger logger;
                    const auto language = Languages[static_cast<std::size_t>(module.stage)];
                    {
                        const CompilationTimer timer{statistics};
                        glslang::GlslangToSpv(*linked.getIntermediate(language), module.words, &logger,
                                             &spirvOptions);
                    }
                    const auto messagesText = logger.getAllMessages();
                    if (!messagesText.empty())
                    {
                        throw BuildError(ErrorCode::CompilationFailed, messagesText);
                    }
                    try
                    {
                        ReflectModule(module, interfaceInput.schema);
                    }
                    catch (const BuildError &error)
                    {
                        throw BuildError(error.GetFailure().code,
                                         program.name + " / " + std::string(StageName(module.stage)) + " / main / " +
                                             module.sourcePath + ": " + error.what());
                    }
                    package.modules.push_back(std::move(module));
                }
                CheckProgramInterfaces(package.modules, package.profile);
                // 包内只存逻辑身份, depfile 单独保存本机路径; 静态图包含未活动分支以保守触发重建.
                output.inputs = interfaceInput.files;
                const auto requestHash = HashText(requestText);
                output.inputs.push_back({programPath, requestHash});
                Json dependencies = Json::array();
                for (const auto &[logical, file] : graph.GetFiles())
                {
                    const auto hash = HashText(file.text);
                    package.dependencies.push_back({logical, hash});
                    dependencies.push_back({{"path", logical}, {"hash", hash}});
                    if (!logical.starts_with("Interface/"))
                    {
                        output.inputs.push_back({file.path, hash});
                    }
                }
                package.sourceMap = graph.GetEdges();
                std::ranges::sort(package.sourceMap,
                                  [](const auto &left, const auto &right)
                                  {
                                      return std::tie(left.includingPath, left.line, left.includedPath) <
                                             std::tie(right.includingPath, right.line, right.includedPath);
                                  });
                // JSON 的字段/字符串长度编码避免简单串接歧义. 原始声明格式变化只使内容身份变化.
                package.contentHash = HashText(Json{
                    {"version", 1},
                    {"request", requestHash},
                    {"interface", interfaceInput.identity},
                    {"sources", dependencies},
                    {"options", options},
                    {"compiler",
                     package.compilerFingerprint}}.dump());
                return output;
            });
    }
}
