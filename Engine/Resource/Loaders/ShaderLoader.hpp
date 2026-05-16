#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <Engine/Resource/Types/Shader.hpp>

namespace Physara::Engine
{
    struct ShaderLoadDesc
    {
        RHI::ShaderStage stage{RHI::ShaderStage::Vertex};
        std::filesystem::path path{};
        ShaderFeatureMask featureMask{ShaderFeature::None};
        std::vector<ShaderDefine> defines{};
    };

    class ShaderLoader final
    {
    public:
        [[nodiscard]] static ShaderSource Load(const ShaderLoadDesc &desc);
        [[nodiscard]] static std::vector<ShaderDefine> BuildFeatureDefines(ShaderFeatureMask featureMask);
        [[nodiscard]] static const char *StageToDefine(RHI::ShaderStage stage);

    private:
        [[nodiscard]] static std::string LoadExpandedSource(const std::filesystem::path &path,
                                                            std::vector<std::string> &dependencies,
                                                            std::vector<std::string> &includeStack);
        [[nodiscard]] static std::string InjectDefines(std::string source, const std::vector<ShaderDefine> &defines);
        [[nodiscard]] static std::filesystem::path ResolveInclude(const std::filesystem::path &includingPath,
                                                                  std::string_view includePath);
    };
}