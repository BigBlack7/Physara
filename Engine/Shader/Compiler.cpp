#include "Compiler.hpp"

#include <algorithm>
#include <array>
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
    struct ProgramStage
    {
        Stage stage = Stage::Vertex;
        std::string source;
        std::string entry;
    };

    struct Program
    {
        std::string name;
        std::string interfaceName;
        std::vector<ProgramStage> stages;
        std::map<std::string, std::uint32_t> defines;
    };

    static Program ParseProgram(const Json &json)
    {
        CheckFields(json, {"version", "name", "interface", "profile", "stages", "defines"}, "/program");
        if (UInt(json.at("version"), 1, "/program/version") != 1 || json.at("profile") != "GraphicsBaselineV1")
        {
            throw BuildError(ErrorCode::UnsupportedProfile, "/program/version or profile is unsupported.");
        }
        Program program;
        program.name = Identifier(json.at("name"), "/program/name");
        program.interfaceName = Identifier(json.at("interface"), "/program/interface");
        const auto &stages = json.at("stages");
        if (!stages.is_array() || stages.size() != 2)
        {
            throw BuildError(ErrorCode::UnsupportedProfile, "/program/stages requires one VS and one FS.");
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
        if (program.stages[0].stage != Stage::Vertex || program.stages[1].stage != Stage::Fragment)
        {
            throw BuildError(ErrorCode::InvalidInput, "/program/stages contains duplicate stages.");
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

    struct InterfaceInput
    {
        InterfaceSchema schema;
        std::vector<std::filesystem::path> files;
        Json identity;
        std::size_t bytes = 0;
    };

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
        constexpr std::array Names{"Data.hpp", "Bindings.hpp", "Vertex.glsl", "Fragment.glsl", "Expected.json"};
        const auto &artifacts = manifest.at("artifacts");
        if (!artifacts.is_object() || artifacts.size() != Names.size())
        {
            throw BuildError(ErrorCode::InterfaceMismatch, "Interface manifest must describe all five artifacts.");
        }
        InterfaceInput input;
        input.files.push_back(manifestPath);
        input.bytes = manifestText.size();
        input.identity = {
            {"manifest", HashText(manifestText)}, {"schema", manifest.at("schemaHash")}, {"artifacts", artifacts}};
        for (const auto *name : Names)
        {
            const auto path = CanonicalInputPath(root / name);
            const auto text = ReadText(path, MaxPackageBytes);
            if (HashText(text) != artifacts.at(name).get<std::string>())
            {
                throw BuildError(ErrorCode::InterfaceMismatch,
                                 std::string("Incomplete/mixed interface artifact: ") + name);
            }
            input.files.push_back(path);
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
        return input;
    }

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

    Result<BuildOutput> Compiler::Build(const BuildRequest &request) noexcept
    {
        return GuardBuild<BuildOutput>(
            [&]
            {
                const auto programPath = CanonicalInputPath(request.programPath);
                const auto interfaceRoot = CanonicalInputPath(request.interfaceRoot);
                const auto requestText = ReadText(programPath, MaxDeclarationBytes);
                const auto program = ParseProgram(ParseJson(requestText, PathText(programPath)));
                const auto interfaceInput = ReadInterface(interfaceRoot, program);
                SourceGraph graph(request.sourceRoot, interfaceRoot);
                std::array<const SourceFile *, 2> sources{};
                for (std::size_t index = 0; index < sources.size(); ++index)
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
                std::string preamble;
                for (const auto &[name, value] : program.defines)
                {
                    preamble += "#define " + name + " " + std::to_string(value) + "\n";
                }
                constexpr std::array Languages{EShLangVertex, EShLangFragment};
                // 名称/长度数组与源内容一起活到 parse/link/生成全部结束. program 先于 shader 析构.
                std::array<std::unique_ptr<glslang::TShader>, 2> shaders;
                std::array<const char *, 2> texts{};
                std::array<const char *, 2> names{};
                std::array<int, 2> lengths{};
                const auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
                for (std::size_t index = 0; index < shaders.size(); ++index)
                {
                    const auto text = sources[index]->GetCompilerText();
                    texts[index] = text.data();
                    names[index] = sources[index]->logicalPath.c_str();
                    lengths[index] = static_cast<int>(text.size());
                    auto &shader = shaders[index];
                    shader = std::make_unique<glslang::TShader>(Languages[index]);
                    shader->setStringsWithLengthsAndNames(&texts[index], &lengths[index], &names[index], 1);
                    shader->setPreamble(preamble.c_str());
                    shader->setEntryPoint("main");
                    shader->setEnvInput(glslang::EShSourceGlsl, Languages[index], glslang::EShClientVulkan, 450);
                    shader->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
                    shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);
                    if (!shader->parse(GetDefaultResources(), 450, ECoreProfile, false, false, messages, graph))
                    {
                        throw BuildError(ErrorCode::CompilationFailed,
                                         CompileDiagnostic(program.name, StageName(program.stages[index].stage),
                                                           shader->getInfoLog(), graph));
                    }
                }
                // 两阶段先链接, 再分别从链接后的中间表示生成 words. 反射和包发布共用这份最终字节码.
                glslang::TProgram linked;
                for (const auto &shader : shaders)
                {
                    linked.addShader(shader.get());
                }
                if (!linked.link(messages))
                {
                    throw BuildError(ErrorCode::CompilationFailed,
                                     CompileDiagnostic(program.name, "VS/FS link", linked.getInfoLog(), graph));
                }
                BuildOutput output;
                auto &package = output.package;
                package.profile = "GraphicsBaselineV1";
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
                for (std::size_t index = 0; index < sources.size(); ++index)
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
                    glslang::GlslangToSpv(*linked.getIntermediate(Languages[index]), module.words, &logger,
                                          &spirvOptions);
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
                CheckProgramInterfaces(package.modules);
                output.fileDependencies = interfaceInput.files;
                for (auto &file : output.fileDependencies)
                {
                    file = std::filesystem::canonical(file);
                }
                output.fileDependencies.push_back(programPath);
                Json dependencies = Json::array();
                for (const auto &[logical, file] : graph.GetFiles())
                {
                    const auto hash = HashText(file.text);
                    package.dependencies.push_back({logical, hash});
                    dependencies.push_back({{"path", logical}, {"hash", hash}});
                    if (!logical.starts_with("Interface/"))
                    {
                        output.fileDependencies.push_back(file.path);
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
                    {"request", HashText(requestText)},
                    {"interface", interfaceInput.identity},
                    {"sources", dependencies},
                    {"options", options},
                    {"compiler",
                     package.compilerFingerprint}}.dump());
                return output;
            });
    }
}
