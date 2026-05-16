#include "ShaderLoader.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Engine
{
    namespace ShaderLoaderDetail
    {
        std::string NormalizeAssetPath(const std::filesystem::path &path)
        {
            return Platform::FileSystem::NormalizeForCompare(Platform::FileSystem::ToAssetsRelativePath(path.string()));
        }

        std::string Trim(std::string_view text)
        {
            const std::size_t first = text.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos)
            {
                return {};
            }

            const std::size_t last = text.find_last_not_of(" \t\r\n");
            return std::string(text.substr(first, last - first + 1u));
        }

        bool TryParseInclude(std::string_view line, std::string &includePath)
        {
            const std::string trimmed = Trim(line);
            if (trimmed.rfind("#include", 0) != 0)
            {
                return false;
            }

            const std::size_t firstQuote = trimmed.find('"');
            const std::size_t lastQuote = trimmed.find_last_of('"');
            if (firstQuote == std::string::npos || lastQuote == firstQuote)
            {
                throw std::runtime_error("Malformed shader include directive: " + trimmed);
            }

            includePath = trimmed.substr(firstQuote + 1u, lastQuote - firstQuote - 1u);
            return true;
        }

        void AppendDefine(std::vector<ShaderDefine> &defines, std::string name)
        {
            defines.push_back(ShaderDefine{std::move(name), "1"});
        }

        void SortDefines(std::vector<ShaderDefine> &defines)
        {
            std::sort(defines.begin(), defines.end(), [](const ShaderDefine &lhs, const ShaderDefine &rhs)
                      {
                          if (lhs.name == rhs.name)
                          {
                              return lhs.value < rhs.value;
                          }
                          return lhs.name < rhs.name;
                      });
        }

        bool DependencyContains(const std::vector<std::string> &dependencies, const std::string &path)
        {
            return std::find(dependencies.begin(), dependencies.end(), path) != dependencies.end();
        }
    }

    ShaderSource ShaderLoader::Load(const ShaderLoadDesc &desc)
    {
        std::vector<std::string> dependencies;
        std::vector<std::string> includeStack;
        std::string source = LoadExpandedSource(desc.path, dependencies, includeStack);

        std::vector<ShaderDefine> defines = BuildFeatureDefines(desc.featureMask);
        defines.insert(defines.end(), desc.defines.begin(), desc.defines.end());
        defines.push_back(ShaderDefine{StageToDefine(desc.stage), "1"});
        ShaderLoaderDetail::SortDefines(defines);

        ShaderSource result{};
        result.stage = desc.stage;
        result.path = ShaderLoaderDetail::NormalizeAssetPath(desc.path);
        result.source = InjectDefines(std::move(source), defines);
        result.dependencies = std::move(dependencies);
        return result;
    }

    std::vector<ShaderDefine> ShaderLoader::BuildFeatureDefines(ShaderFeatureMask featureMask)
    {
        std::vector<ShaderDefine> defines;
        if ((featureMask & ShaderFeature::HasBaseColorMap) != 0ull)
        {
            ShaderLoaderDetail::AppendDefine(defines, "HAS_BASE_COLOR_MAP");
        }
        if ((featureMask & ShaderFeature::HasMetallicRoughnessMap) != 0ull)
        {
            ShaderLoaderDetail::AppendDefine(defines, "HAS_METALLIC_ROUGHNESS_MAP");
        }
        if ((featureMask & ShaderFeature::HasNormalMap) != 0ull)
        {
            ShaderLoaderDetail::AppendDefine(defines, "HAS_NORMAL_MAP");
        }
        if ((featureMask & ShaderFeature::HasOcclusionMap) != 0ull)
        {
            ShaderLoaderDetail::AppendDefine(defines, "HAS_OCCLUSION_MAP");
        }
        if ((featureMask & ShaderFeature::HasEmissiveMap) != 0ull)
        {
            ShaderLoaderDetail::AppendDefine(defines, "HAS_EMISSIVE_MAP");
        }
        if ((featureMask & ShaderFeature::AlphaTest) != 0ull)
        {
            ShaderLoaderDetail::AppendDefine(defines, "ALPHA_TEST");
        }
        if ((featureMask & ShaderFeature::AlphaBlend) != 0ull)
        {
            ShaderLoaderDetail::AppendDefine(defines, "ALPHA_BLEND");
        }
        if ((featureMask & ShaderFeature::Skinning) != 0ull)
        {
            ShaderLoaderDetail::AppendDefine(defines, "SKINNING");
        }
        if ((featureMask & ShaderFeature::ReceiveShadow) != 0ull)
        {
            ShaderLoaderDetail::AppendDefine(defines, "RECEIVE_SHADOW");
        }
        if ((featureMask & ShaderFeature::CastShadow) != 0ull)
        {
            ShaderLoaderDetail::AppendDefine(defines, "CAST_SHADOW");
        }
        if ((featureMask & ShaderFeature::Unlit) != 0ull)
        {
            ShaderLoaderDetail::AppendDefine(defines, "SHADING_MODEL_UNLIT");
        }
        return defines;
    }

    const char *ShaderLoader::StageToDefine(RHI::ShaderStage stage)
    {
        switch (stage)
        {
        case RHI::ShaderStage::Vertex:
            return "SHADER_STAGE_VERTEX";
        case RHI::ShaderStage::Fragment:
            return "SHADER_STAGE_FRAGMENT";
        case RHI::ShaderStage::Compute:
            return "SHADER_STAGE_COMPUTE";
        }

        return "SHADER_STAGE_UNKNOWN";
    }

    std::string ShaderLoader::LoadExpandedSource(const std::filesystem::path &path,
                                                 std::vector<std::string> &dependencies,
                                                 std::vector<std::string> &includeStack)
    {
        const std::string normalizedPath = ShaderLoaderDetail::NormalizeAssetPath(path);
        if (std::find(includeStack.begin(), includeStack.end(), normalizedPath) != includeStack.end())
        {
            throw std::runtime_error("Recursive shader include detected: " + normalizedPath);
        }

        includeStack.push_back(normalizedPath);
        if (!ShaderLoaderDetail::DependencyContains(dependencies, normalizedPath))
        {
            dependencies.push_back(normalizedPath);
        }

        const std::string source = Platform::FileSystem::ReadTextFile(normalizedPath);
        std::istringstream stream(source);
        std::ostringstream expanded;
        std::string line;
        std::uint32_t lineNumber = 1;

        while (std::getline(stream, line))
        {
            std::string includePath;
            if (ShaderLoaderDetail::TryParseInclude(line, includePath))
            {
                const std::filesystem::path resolvedInclude = ResolveInclude(normalizedPath, includePath);
                expanded << "\n#line 1\n";
                expanded << LoadExpandedSource(resolvedInclude, dependencies, includeStack);
                expanded << "\n#line " << (lineNumber + 1u) << '\n';
            }
            else
            {
                expanded << line << '\n';
            }

            ++lineNumber;
        }

        includeStack.pop_back();
        return expanded.str();
    }

    std::string ShaderLoader::InjectDefines(std::string source, const std::vector<ShaderDefine> &defines)
    {
        std::ostringstream defineBlock;
        for (const ShaderDefine &define : defines)
        {
            if (define.name.empty())
            {
                continue;
            }
            defineBlock << "#define " << define.name << ' ' << (define.value.empty() ? "1" : define.value) << '\n';
        }

        const std::size_t versionPosition = source.find("#version");
        if (versionPosition == std::string::npos)
        {
            return defineBlock.str() + source;
        }

        const std::size_t versionEnd = source.find('\n', versionPosition);
        if (versionEnd == std::string::npos)
        {
            return source + '\n' + defineBlock.str();
        }

        return source.substr(0, versionEnd + 1u) + defineBlock.str() + source.substr(versionEnd + 1u);
    }

    std::filesystem::path ShaderLoader::ResolveInclude(const std::filesystem::path &includingPath, std::string_view includePath)
    {
        const std::filesystem::path include(includePath);
        const std::filesystem::path relativeCandidate = (includingPath.parent_path() / include).lexically_normal();
        if (Platform::FileSystem::Exists(ShaderLoaderDetail::NormalizeAssetPath(relativeCandidate)))
        {
            return relativeCandidate;
        }

        const std::filesystem::path includesCandidate = (std::filesystem::path("Shaders/Includes") / include).lexically_normal();
        if (Platform::FileSystem::Exists(ShaderLoaderDetail::NormalizeAssetPath(includesCandidate)))
        {
            return includesCandidate;
        }

        if (Platform::FileSystem::Exists(ShaderLoaderDetail::NormalizeAssetPath(include)))
        {
            return include;
        }

        throw std::runtime_error("Shader include not found: " + std::string(includePath) +
                                 " included from " + ShaderLoaderDetail::NormalizeAssetPath(includingPath));
    }
}