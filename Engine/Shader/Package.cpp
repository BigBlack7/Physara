#include "Package.hpp"

#include <algorithm>
#include <array>
#include <set>

#include "Spirv.hpp"

namespace Physara::Shader
{
    static constexpr std::array<unsigned char, 8> PackageMagic{'P', 'S', 'H', 'A', 'D', 'E', 'R', 0};
    static constexpr std::size_t PrefixBytes = 64;

    static void RequirePackage(bool valid, std::string_view message)
    {
        if (!valid)
        {
            throw BuildError(ErrorCode::CorruptPackage, message);
        }
    }

    static bool IsHash(std::string_view text)
    {
        return text.size() == 64 && std::ranges::all_of(text,
                                                        [](char character)
                                                        {
                                                            return (character >= '0' && character <= '9') ||
                                                                   (character >= 'a' && character <= 'f');
                                                        });
    }

    static bool IsLogicalPath(std::string_view text)
    {
        if ((!text.starts_with("Source/") && !text.starts_with("Interface/")) ||
            text.find_first_of("\\:\r\n") != std::string_view::npos || text.find('\0') != std::string_view::npos)
        {
            return false;
        }
        const auto path =
            std::filesystem::path(std::u8string(reinterpret_cast<const char8_t *>(text.data()), text.size()));
        return path.has_filename() && PathText(path.lexically_normal()) == text &&
               std::ranges::none_of(path,
                                    [](const auto &part)
                                    {
                                        return part == ".." || part == ".";
                                    });
    }

    static void CheckPackage(const ShaderPackage &package, const InterfaceSchema &expected)
    {
        RequirePackage(package.profile == "GraphicsBaselineV1" && IsHash(package.abiHash) &&
                           IsHash(package.contentHash) && IsHash(package.compilerFingerprint),
                       "Invalid package identity/profile.");
        RequirePackage(package.abiHash == expected.abiHash, "Package ABI does not describe its interface snapshot.");
        CheckProgramInterfaces(package.modules);
        RequirePackage(!package.dependencies.empty() && package.dependencies.size() <= 256,
                       "Invalid package dependency count.");
        std::set<std::string> paths;
        for (const auto &dependency : package.dependencies)
        {
            RequirePackage(IsLogicalPath(dependency.logicalPath) && IsHash(dependency.contentHash) &&
                               paths.insert(dependency.logicalPath).second,
                           "Invalid/duplicate package dependency.");
        }
        for (const auto &edge : package.sourceMap)
        {
            RequirePackage(paths.contains(edge.includingPath) && paths.contains(edge.includedPath) && edge.line > 0 &&
                               edge.line <= MaxSourceBytes,
                           "Invalid package source mapping.");
        }
        std::size_t payloadBytes = 0;
        for (const auto &module : package.modules)
        {
            RequirePackage(module.entry == "main" && paths.contains(module.sourcePath), "Invalid module source/entry.");
            RequirePackage(module.words.size() <= (MaxPackageBytes - payloadBytes) / 4,
                           "Package payload exceeds budget.");
            payloadBytes += module.words.size() * 4;
        }
        for (const auto &module : package.modules)
        {
            auto reflected = module;
            ReflectModule(reflected, expected);
            RequirePackage(Json(reflected.resources) == Json(module.resources) &&
                               Json(reflected.inputs) == Json(module.inputs) &&
                               Json(reflected.outputs) == Json(module.outputs),
                           "Package metadata does not describe its final IR.");
        }
    }

    static void PutInteger(std::vector<std::byte> &bytes, std::size_t offset, std::uint64_t value, std::size_t width)
    {
        for (std::size_t index = 0; index < width; ++index)
        {
            bytes[offset + index] = static_cast<std::byte>((value >> (index * 8)) & 255);
        }
    }

    static std::uint64_t GetInteger(std::span<const std::byte> bytes, std::size_t offset, std::size_t width)
    {
        std::uint64_t result = 0;
        for (std::size_t index = 0; index < width; ++index)
        {
            result |= std::uint64_t(std::to_integer<unsigned char>(bytes[offset + index])) << (index * 8);
        }
        return result;
    }

    static unsigned char HexDigit(char character)
    {
        return static_cast<unsigned char>(character <= '9' ? character - '0' : character - 'a' + 10);
    }

    static std::vector<std::byte> EncodePackage(const ShaderPackage &package)
    {
        const auto declaration = DeclarationSchema(package.declaration);
        RequirePackage(declaration.dump().size() <= MaxDeclarationBytes,
                       "Interface snapshot exceeds declaration budget.");
        const auto expected = ParseInterfaceSchema(declaration);
        RequirePackage(ComputeInterfaceAbi(package.declaration) == expected.abiHash,
                       "Interface snapshot contains noncanonical derived layout/binding values.");
        CheckPackage(package, expected);
        Json modules = Json::array();
        std::size_t payloadBytes = 0;
        for (const auto &module : package.modules)
        {
            modules.push_back({{"stage", StageName(module.stage)},
                               {"entry", module.entry},
                               {"source", module.sourcePath},
                               {"offset", payloadBytes},
                               {"wordCount", module.words.size()},
                               {"resources", module.resources},
                               {"inputs", module.inputs},
                               {"outputs", module.outputs}});
            payloadBytes += module.words.size() * 4;
        }
        Json dependencies = Json::array();
        for (const auto &dependency : package.dependencies)
        {
            dependencies.push_back({{"path", dependency.logicalPath}, {"hash", dependency.contentHash}});
        }
        Json sourceMap = Json::array();
        for (const auto &edge : package.sourceMap)
        {
            sourceMap.push_back({{"from", edge.includingPath},
                                 {"to", edge.includedPath},
                                 {"line", edge.line},
                                 {"expanded", edge.expanded}});
        }
        const Json metadata{{"interface", declaration},
                            {"profile", package.profile},
                            {"abiHash", package.abiHash},
                            {"contentHash", package.contentHash},
                            {"compilerFingerprint", package.compilerFingerprint},
                            {"modules", modules},
                            {"dependencies", dependencies},
                            {"sourceMap", sourceMap}};
        const auto text = metadata.dump();
        const auto paddedBytes = (text.size() + 3) / 4 * 4;
        RequirePackage(paddedBytes <= MaxPackageBytes - PrefixBytes &&
                           payloadBytes <= MaxPackageBytes - PrefixBytes - paddedBytes,
                       "Complete package exceeds 64 MiB.");
        std::vector<std::byte> bytes(PrefixBytes + paddedBytes + payloadBytes);
        for (std::size_t index = 0; index < PackageMagic.size(); ++index)
        {
            bytes[index] = static_cast<std::byte>(PackageMagic[index]);
        }
        PutInteger(bytes, 8, 1, 4);
        PutInteger(bytes, 12, PrefixBytes, 4);
        PutInteger(bytes, 16, text.size(), 8);
        PutInteger(bytes, 24, payloadBytes, 8);
        std::ranges::copy(std::as_bytes(std::span(text.data(), text.size())), bytes.begin() + PrefixBytes);
        auto cursor = PrefixBytes + paddedBytes;
        for (const auto &module : package.modules)
        {
            for (const auto word : module.words)
            {
                PutInteger(bytes, cursor, word, 4);
                cursor += 4;
            }
        }
        // 摘要覆盖定宽头(摘要槽置零)、UTF-8 元数据、对齐 padding 与载荷, 不序列化宿主对象.
        const auto digest = Sha256(bytes);
        for (std::size_t index = 0; index < 32; ++index)
        {
            bytes[32 + index] =
                static_cast<std::byte>((HexDigit(digest[index * 2]) << 4) | HexDigit(digest[index * 2 + 1]));
        }
        return bytes;
    }

    static ShaderPackage DecodePackage(std::vector<std::byte> bytes, std::string_view expectedAbiHash)
    {
        RequirePackage(bytes.size() >= PrefixBytes, "Truncated package prefix.");
        for (std::size_t index = 0; index < PackageMagic.size(); ++index)
        {
            RequirePackage(bytes[index] == static_cast<std::byte>(PackageMagic[index]), "Invalid package magic.");
        }
        RequirePackage(GetInteger(bytes, 8, 4) == 1 && GetInteger(bytes, 12, 4) == PrefixBytes,
                       "Unknown package version.");
        const auto metadataBytes = GetInteger(bytes, 16, 8);
        const auto payloadBytes = GetInteger(bytes, 24, 8);
        RequirePackage(metadataBytes > 0 && metadataBytes <= MaxPackageBytes && payloadBytes <= MaxPackageBytes,
                       "Package length exceeds budget.");
        const auto paddedBytes = (metadataBytes + 3) / 4 * 4;
        RequirePackage(paddedBytes <= bytes.size() - PrefixBytes &&
                           payloadBytes == bytes.size() - PrefixBytes - paddedBytes && payloadBytes % 4 == 0,
                       "Package length/alignment mismatch.");
        std::array<std::byte, 32> storedDigest{};
        std::copy_n(bytes.begin() + 32, 32, storedDigest.begin());
        std::fill_n(bytes.begin() + 32, 32, std::byte{0});
        const auto digest = Sha256(bytes);
        for (std::size_t index = 0; index < 32; ++index)
        {
            RequirePackage(storedDigest[index] == static_cast<std::byte>((HexDigit(digest[index * 2]) << 4) |
                                                                         HexDigit(digest[index * 2 + 1])),
                           "Package digest mismatch.");
        }
        for (auto index = metadataBytes; index < paddedBytes; ++index)
        {
            RequirePackage(bytes[PrefixBytes + static_cast<std::size_t>(index)] == std::byte{0},
                           "Invalid package padding.");
        }
        const auto metadata = ParseJson(
            {reinterpret_cast<const char *>(bytes.data() + PrefixBytes), static_cast<std::size_t>(metadataBytes)},
            "package metadata");
        CheckFields(metadata,
                    {"interface", "profile", "abiHash", "contentHash", "compilerFingerprint", "modules", "dependencies",
                     "sourceMap"},
                    "package metadata");
        ShaderPackage package;
        package.profile = metadata.at("profile").get<std::string>();
        package.abiHash = metadata.at("abiHash").get<std::string>();
        package.contentHash = metadata.at("contentHash").get<std::string>();
        package.compilerFingerprint = metadata.at("compilerFingerprint").get<std::string>();
        if (package.abiHash != expectedAbiHash)
        {
            throw BuildError(ErrorCode::InterfaceMismatch, "Package ABI differs from the consumer's expected ABI.");
        }
        RequirePackage(metadata.at("interface").dump().size() <= MaxDeclarationBytes,
                       "Interface snapshot exceeds declaration budget.");
        const auto expected = ParseInterfaceSchema(metadata.at("interface"));
        package.declaration = SnapshotInterface(expected);
        const auto &modules = metadata.at("modules");
        RequirePackage(modules.is_array() && modules.size() == 2, "Invalid module count.");
        // offset 从 payload 起点按字节计, wordCount 以 32 位字计.
        // cursor 总是指向下一段起点, 两个模块读取后必须恰好耗尽 payload.
        std::size_t cursor = 0;
        for (const auto &json : modules)
        {
            CheckFields(json, {"stage", "entry", "source", "offset", "wordCount", "resources", "inputs", "outputs"},
                        "module");
            const auto offset = UInt(json.at("offset"), MaxPackageBytes, "module/offset");
            const auto count = UInt(json.at("wordCount"), MaxPackageBytes / 4, "module/wordCount");
            RequirePackage(offset % 4 == 0 && offset == cursor && count >= 5 && count <= (payloadBytes - cursor) / 4,
                           "Module ranges overlap or exceed the aligned payload.");
            ShaderModule module;
            module.stage = ParseStage(json.at("stage").get<std::string>());
            module.entry = json.at("entry").get<std::string>();
            module.sourcePath = json.at("source").get<std::string>();
            module.resources = json.at("resources").get<std::vector<ResourceReflection>>();
            module.inputs = json.at("inputs").get<std::vector<IoReflection>>();
            module.outputs = json.at("outputs").get<std::vector<IoReflection>>();
            module.words.resize(count);
            for (auto &word : module.words)
            {
                word = static_cast<std::uint32_t>(
                    GetInteger(bytes, PrefixBytes + static_cast<std::size_t>(paddedBytes) + cursor, 4));
                cursor += 4;
            }
            package.modules.push_back(std::move(module));
        }
        RequirePackage(cursor == payloadBytes, "Unclaimed package payload.");
        RequirePackage(metadata.at("dependencies").is_array() && metadata.at("dependencies").size() <= 256 &&
                           metadata.at("sourceMap").is_array(),
                       "Invalid dependency/source-map arrays.");
        for (const auto &json : metadata.at("dependencies"))
        {
            CheckFields(json, {"path", "hash"}, "dependency");
            package.dependencies.push_back({json.at("path").get<std::string>(), json.at("hash").get<std::string>()});
        }
        for (const auto &json : metadata.at("sourceMap"))
        {
            CheckFields(json, {"from", "to", "line", "expanded"}, "source map");
            RequirePackage(json.at("expanded").is_boolean(), "Invalid source-map expansion flag.");
            package.sourceMap.push_back(
                {json.at("from").get<std::string>(), json.at("to").get<std::string>(),
                 UInt(json.at("line"), static_cast<std::uint32_t>(MaxSourceBytes), "source map/line"),
                 json.at("expanded").get<bool>()});
        }
        CheckPackage(package, expected);
        return package;
    }

    Result<ShaderPackage> ReadPackage(const std::filesystem::path &path, std::string_view expectedAbiHash) noexcept
    {
        return GuardBuild<ShaderPackage>(
            [&]
            {
                if (!IsHash(expectedAbiHash))
                {
                    throw BuildError(ErrorCode::InvalidInput,
                                     "Reading a package requires the consumer's 64-digit ABI hash.");
                }
                auto bytes = ReadBytes(path, MaxPackageBytes);
                try
                {
                    return DecodePackage(std::move(bytes), expectedAbiHash);
                }
                catch (const BuildError &error)
                {
                    const auto code = error.GetFailure().code;
                    if (code == ErrorCode::InterfaceMismatch &&
                        error.GetFailure().context.description.View().starts_with("Package ABI differs"))
                    {
                        throw;
                    }
                    if (code == ErrorCode::SystemFailure || code == ErrorCode::OutOfMemory)
                    {
                        throw;
                    }
                    throw BuildError(ErrorCode::CorruptPackage, error.what());
                }
                catch (const Json::exception &error)
                {
                    throw BuildError(ErrorCode::CorruptPackage, error.what());
                }
            });
    }

    Result<bool> WritePackage(const std::filesystem::path &path, const ShaderPackage &package) noexcept
    {
        return GuardBuild<bool>(
            [&]
            {
                const auto bytes = EncodePackage(package);
                return WriteAtomically(path, bytes);
            });
    }
}
