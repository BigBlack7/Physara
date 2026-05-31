#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include <glm/vec4.hpp>

namespace Physara::Engine
{
    struct IBLPrecomputeSettings
    {
        std::uint32_t cubeSize{512};
        std::uint32_t brdfLutSize{256};
        std::uint32_t specularSampleCount{256};
        std::uint32_t brdfSampleCount{1024};
        bool useCache{true};
        bool createIfMissing{true};
        bool writeCache{true};
#if defined(PHYSARA_DEBUG)
        bool writeDebugOutputs{true};
#else
        bool writeDebugOutputs{false};
#endif
    };

    struct IBLCubeFace
    {
        std::uint32_t width{0};
        std::uint32_t height{0};
        std::vector<float> rgba32f{};
    };

    struct IBLPrecomputeResult
    {
        std::uint32_t cubeSize{0};
        std::uint32_t specularMipCount{0};
        std::uint32_t brdfLutSize{0};
        std::array<glm::vec4, 9> irradianceSH{};
        std::vector<std::array<IBLCubeFace, 6>> specularMipChain{};
        std::vector<float> brdfLutRG32F{};

        [[nodiscard]] bool IsValid() const
        {
            return cubeSize > 0 && specularMipCount > 0 && brdfLutSize > 0 &&
                   specularMipChain.size() == specularMipCount &&
                   brdfLutRG32F.size() == static_cast<std::size_t>(brdfLutSize) * brdfLutSize * 2u;
        }
    };

    class IBLPrecompute final
    {
    public:
        [[nodiscard]] static std::shared_ptr<IBLPrecomputeResult> LoadOrCreate(
            const std::filesystem::path &environmentPath,
            const IBLPrecomputeSettings &settings = {});

        [[nodiscard]] static std::filesystem::path GetCacheDirectory(const std::filesystem::path &environmentPath);
    };
}