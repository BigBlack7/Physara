#include "IBLPrecompute.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <tinyexr/tinyexr.h>

#include <Engine/Core/Log.hpp>
#include <Engine/Resource/Loaders/TextureLoader.hpp>
#include <Engine/Resource/Types/Texture.hpp>

namespace Physara::Engine
{
    namespace IBLPrecomputeDetail
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float InvPi = 0.31830988618379067154f;
        constexpr std::uint32_t CacheVersion = 2u;

        struct CacheHeader
        {
            char magic[8]{'P', 'H', 'Y', 'S', 'I', 'B', 'L', '\0'};
            std::uint32_t version{CacheVersion};
            std::uint32_t cubeSize{0};
            std::uint32_t specularMipCount{0};
            std::uint32_t brdfLutSize{0};
            std::uint32_t reserved0{0};
            std::uint64_t sourceFileSize{0};
            std::int64_t sourceWriteTime{0};
        };

        [[nodiscard]] std::uint32_t CalculateMipCount(std::uint32_t size)
        {
            std::uint32_t levels = 1u;
            while (size > 1u)
            {
                size >>= 1u;
                ++levels;
            }
            return levels;
        }

        [[nodiscard]] std::uint64_t FileSize(const std::filesystem::path &path)
        {
            std::error_code error{};
            return std::filesystem::exists(path, error) ? std::filesystem::file_size(path, error) : 0u;
        }

        [[nodiscard]] std::int64_t FileWriteTime(const std::filesystem::path &path)
        {
            std::error_code error{};
            const auto time = std::filesystem::last_write_time(path, error);
            return error ? 0 : time.time_since_epoch().count();
        }

        [[nodiscard]] CacheHeader BuildHeader(const std::filesystem::path &environmentPath, const IBLPrecomputeSettings &settings)
        {
            CacheHeader header{};
            header.cubeSize = settings.cubeSize;
            header.specularMipCount = CalculateMipCount(settings.cubeSize);
            header.brdfLutSize = settings.brdfLutSize;
            header.sourceFileSize = FileSize(environmentPath);
            header.sourceWriteTime = FileWriteTime(environmentPath);
            return header;
        }

        [[nodiscard]] bool SameHeader(const CacheHeader &lhs, const CacheHeader &rhs)
        {
            return std::memcmp(lhs.magic, rhs.magic, sizeof(lhs.magic)) == 0 &&
                   lhs.version == rhs.version &&
                   lhs.cubeSize == rhs.cubeSize &&
                   lhs.specularMipCount == rhs.specularMipCount &&
                   lhs.brdfLutSize == rhs.brdfLutSize &&
                   lhs.sourceFileSize == rhs.sourceFileSize &&
                   lhs.sourceWriteTime == rhs.sourceWriteTime;
        }

        [[nodiscard]] glm::vec3 SanitizeHDR(glm::vec3 value)
        {
            value.r = std::isfinite(value.r) ? std::clamp(value.r, 0.f, 60000.f) : 0.f;
            value.g = std::isfinite(value.g) ? std::clamp(value.g, 0.f, 60000.f) : 0.f;
            value.b = std::isfinite(value.b) ? std::clamp(value.b, 0.f, 60000.f) : 0.f;
            return value;
        }

        [[nodiscard]] glm::vec3 DirectionFromEquirectPixel(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)
        {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(std::max(width, 1u));
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(std::max(height, 1u));
            const float phi = (u - 0.5f) * 2.f * Pi;
            const float theta = v * Pi;
            const float sinTheta = std::sin(theta);
            return glm::normalize(glm::vec3(std::cos(phi) * sinTheta, std::cos(theta), std::sin(phi) * sinTheta));
        }

        [[nodiscard]] glm::vec3 DirectionFromCubeFace(std::uint32_t face, std::uint32_t x, std::uint32_t y, std::uint32_t size)
        {
            const float a = 2.f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size) - 1.f;
            const float b = 2.f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size) - 1.f;
            switch (face)
            {
            case 0:
                return glm::normalize(glm::vec3(1.f, -b, -a));
            case 1:
                return glm::normalize(glm::vec3(-1.f, -b, a));
            case 2:
                return glm::normalize(glm::vec3(a, 1.f, b));
            case 3:
                return glm::normalize(glm::vec3(a, -1.f, -b));
            case 4:
                return glm::normalize(glm::vec3(a, -b, 1.f));
            default:
                return glm::normalize(glm::vec3(-a, -b, -1.f));
            }
        }

        [[nodiscard]] glm::vec2 EquirectUV(glm::vec3 direction)
        {
            const float longitude = std::atan2(direction.z, direction.x);
            const float latitude = std::acos(std::clamp(direction.y, -1.f, 1.f));
            return glm::vec2(longitude / (2.f * Pi) + 0.5f, latitude / Pi);
        }

        [[nodiscard]] glm::vec3 SamplePanorama(const Texture &panorama, glm::vec3 direction)
        {
            if (panorama.rgba32fPixels.empty() || panorama.width == 0 || panorama.height == 0)
            {
                return glm::vec3(0.f);
            }

            const glm::vec2 uv = EquirectUV(direction);
            const float x = uv.x * static_cast<float>(panorama.width) - 0.5f;
            const float y = uv.y * static_cast<float>(panorama.height) - 0.5f;
            const int x0 = static_cast<int>(std::floor(x));
            const int y0 = static_cast<int>(std::floor(y));
            const float tx = x - static_cast<float>(x0);
            const float ty = y - static_cast<float>(y0);

            const auto read = [&panorama](int px, int py)
            {
                const int wrappedX = (px % static_cast<int>(panorama.width) + static_cast<int>(panorama.width)) % static_cast<int>(panorama.width);
                const int clampedY = std::clamp(py, 0, static_cast<int>(panorama.height) - 1);
                const std::size_t base = (static_cast<std::size_t>(clampedY) * panorama.width + static_cast<std::size_t>(wrappedX)) * 4u;
                return SanitizeHDR(glm::vec3(
                    panorama.rgba32fPixels[base + 0u],
                    panorama.rgba32fPixels[base + 1u],
                    panorama.rgba32fPixels[base + 2u]));
            };

            const glm::vec3 a = glm::mix(read(x0, y0), read(x0 + 1, y0), tx);
            const glm::vec3 b = glm::mix(read(x0, y0 + 1), read(x0 + 1, y0 + 1), tx);
            return glm::mix(a, b, ty);
        }

        [[nodiscard]] std::array<float, 9> SHBasis(glm::vec3 d)
        {
            return {
                0.282095f,
                0.488603f * d.y,
                0.488603f * d.z,
                0.488603f * d.x,
                1.092548f * d.x * d.y,
                1.092548f * d.y * d.z,
                0.315392f * (3.f * d.z * d.z - 1.f),
                1.092548f * d.x * d.z,
                0.546274f * (d.x * d.x - d.y * d.y)};
        }

        [[nodiscard]] std::array<glm::vec4, 9> ComputeIrradianceSH(const Texture &panorama)
        {
            std::array<glm::vec3, 9> coefficients{};
            double weightSum = 0.0;
            for (std::uint32_t y = 0; y < panorama.height; ++y)
            {
                const float theta = (static_cast<float>(y) + 0.5f) / static_cast<float>(panorama.height) * Pi;
                const float solidAngle = std::sin(theta) * (Pi / static_cast<float>(panorama.height)) *
                                         (2.f * Pi / static_cast<float>(panorama.width));
                for (std::uint32_t x = 0; x < panorama.width; ++x)
                {
                    const glm::vec3 direction = DirectionFromEquirectPixel(x, y, panorama.width, panorama.height);
                    const std::size_t base = (static_cast<std::size_t>(y) * panorama.width + x) * 4u;
                    const glm::vec3 radiance = SanitizeHDR(glm::vec3(
                        panorama.rgba32fPixels[base + 0u],
                        panorama.rgba32fPixels[base + 1u],
                        panorama.rgba32fPixels[base + 2u]));
                    const std::array<float, 9> basis = SHBasis(direction);
                    for (std::size_t i = 0u; i < coefficients.size(); ++i)
                    {
                        coefficients[i] += radiance * basis[i] * solidAngle;
                    }
                    weightSum += solidAngle;
                }
            }

            const float convolution[9] = {
                Pi, 2.f * Pi / 3.f, 2.f * Pi / 3.f, 2.f * Pi / 3.f,
                Pi / 4.f, Pi / 4.f, Pi / 4.f, Pi / 4.f, Pi / 4.f};
            std::array<glm::vec4, 9> result{};
            const float normalization = weightSum > 0.0 ? static_cast<float>((4.0 * Pi) / weightSum) : 1.f;
            for (std::size_t i = 0u; i < result.size(); ++i)
            {
                const glm::vec3 value = coefficients[i] * convolution[i] * normalization;
                result[i] = glm::vec4(value, 0.f);
            }
            return result;
        }

        [[nodiscard]] std::array<IBLCubeFace, 6> EquirectToCube(const Texture &panorama, std::uint32_t size)
        {
            std::array<IBLCubeFace, 6> faces{};
            for (std::uint32_t face = 0; face < 6u; ++face)
            {
                faces[face].width = size;
                faces[face].height = size;
                faces[face].rgba32f.assign(static_cast<std::size_t>(size) * size * 4u, 1.f);
                for (std::uint32_t y = 0; y < size; ++y)
                {
                    for (std::uint32_t x = 0; x < size; ++x)
                    {
                        const glm::vec3 color = SamplePanorama(panorama, DirectionFromCubeFace(face, x, y, size));
                        const std::size_t base = (static_cast<std::size_t>(y) * size + x) * 4u;
                        faces[face].rgba32f[base + 0u] = color.r;
                        faces[face].rgba32f[base + 1u] = color.g;
                        faces[face].rgba32f[base + 2u] = color.b;
                        faces[face].rgba32f[base + 3u] = 1.f;
                    }
                }
            }
            return faces;
        }

        [[nodiscard]] float RadicalInverseVdC(std::uint32_t bits)
        {
            bits = (bits << 16u) | (bits >> 16u);
            bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
            bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
            bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
            bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
            return static_cast<float>(bits) * 2.3283064365386963e-10f;
        }

        [[nodiscard]] glm::vec2 Hammersley(std::uint32_t i, std::uint32_t count)
        {
            return glm::vec2(static_cast<float>(i) / static_cast<float>(count), RadicalInverseVdC(i));
        }

        [[nodiscard]] glm::vec3 ImportanceSampleGGX(glm::vec2 xi, glm::vec3 normal, float perceptualRoughness)
        {
            const float a = std::max(perceptualRoughness * perceptualRoughness, 0.001f);
            const float phi = 2.f * Pi * xi.x;
            const float cosTheta = std::sqrt((1.f - xi.y) / std::max(1.f + (a * a - 1.f) * xi.y, 0.001f));
            const float sinTheta = std::sqrt(std::max(1.f - cosTheta * cosTheta, 0.f));
            const glm::vec3 h(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);

            const glm::vec3 up = std::abs(normal.z) < 0.999f ? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(1.f, 0.f, 0.f);
            const glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
            const glm::vec3 bitangent = glm::cross(normal, tangent);
            return glm::normalize(tangent * h.x + bitangent * h.y + normal * h.z);
        }

        [[nodiscard]] std::array<IBLCubeFace, 6> PrefilterMip(const Texture &panorama, std::uint32_t size, float roughness, std::uint32_t sampleCount)
        {
            std::array<IBLCubeFace, 6> faces{};
            for (std::uint32_t face = 0; face < 6u; ++face)
            {
                faces[face].width = size;
                faces[face].height = size;
                faces[face].rgba32f.assign(static_cast<std::size_t>(size) * size * 4u, 1.f);
                for (std::uint32_t y = 0; y < size; ++y)
                {
                    for (std::uint32_t x = 0; x < size; ++x)
                    {
                        const glm::vec3 normal = DirectionFromCubeFace(face, x, y, size);
                        const glm::vec3 view = normal;
                        glm::vec3 sum(0.f);
                        float weightSum = 0.f;
                        for (std::uint32_t i = 0; i < sampleCount; ++i)
                        {
                            const glm::vec3 halfVector = ImportanceSampleGGX(Hammersley(i, sampleCount), normal, roughness);
                            const glm::vec3 light = glm::normalize(2.f * glm::dot(view, halfVector) * halfVector - view);
                            const float NoL = std::max(glm::dot(normal, light), 0.f);
                            if (NoL > 0.f)
                            {
                                sum += SamplePanorama(panorama, light) * NoL;
                                weightSum += NoL;
                            }
                        }

                        const glm::vec3 color = weightSum > 0.f ? sum / weightSum : SamplePanorama(panorama, normal);
                        const std::size_t base = (static_cast<std::size_t>(y) * size + x) * 4u;
                        faces[face].rgba32f[base + 0u] = color.r;
                        faces[face].rgba32f[base + 1u] = color.g;
                        faces[face].rgba32f[base + 2u] = color.b;
                        faces[face].rgba32f[base + 3u] = 1.f;
                    }
                }
            }
            return faces;
        }

        [[nodiscard]] float GeometrySchlickGGX(float NoV, float roughness)
        {
            const float a = roughness;
            const float k = (a * a) / 2.f;
            return NoV / std::max(NoV * (1.f - k) + k, 0.001f);
        }

        [[nodiscard]] float GeometrySmith(float NoV, float NoL, float roughness)
        {
            return GeometrySchlickGGX(NoV, roughness) * GeometrySchlickGGX(NoL, roughness);
        }

        [[nodiscard]] glm::vec2 IntegrateBRDF(float NoV, float perceptualRoughness, std::uint32_t sampleCount)
        {
            const glm::vec3 view(std::sqrt(std::max(1.f - NoV * NoV, 0.f)), 0.f, NoV);
            const glm::vec3 normal(0.f, 0.f, 1.f);
            float a = 0.f;
            float b = 0.f;
            for (std::uint32_t i = 0; i < sampleCount; ++i)
            {
                const glm::vec3 halfVector = ImportanceSampleGGX(Hammersley(i, sampleCount), normal, perceptualRoughness);
                const glm::vec3 light = glm::normalize(2.f * glm::dot(view, halfVector) * halfVector - view);
                const float NoL = std::max(light.z, 0.f);
                const float NoH = std::max(halfVector.z, 0.f);
                const float VoH = std::max(glm::dot(view, halfVector), 0.f);
                if (NoL > 0.f)
                {
                    const float roughness = perceptualRoughness * perceptualRoughness;
                    const float g = GeometrySmith(NoV, NoL, roughness);
                    const float gVis = (g * VoH) / std::max(NoH * NoV, 0.001f);
                    const float fc = std::pow(1.f - VoH, 5.f);
                    a += fc * gVis;
                    b += gVis;
                }
            }
            return glm::vec2(a, b) / static_cast<float>(sampleCount);
        }

        [[nodiscard]] std::vector<float> BuildBRDFLut(std::uint32_t size, std::uint32_t sampleCount)
        {
            std::vector<float> data(static_cast<std::size_t>(size) * size * 2u, 0.f);
            for (std::uint32_t y = 0; y < size; ++y)
            {
                const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
                for (std::uint32_t x = 0; x < size; ++x)
                {
                    const float NoV = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
                    const glm::vec2 value = IntegrateBRDF(NoV, roughness, sampleCount);
                    const std::size_t base = (static_cast<std::size_t>(y) * size + x) * 2u;
                    data[base + 0u] = value.x;
                    data[base + 1u] = value.y;
                }
            }
            return data;
        }

        template <typename T>
        void WriteValue(std::ofstream &stream, const T &value)
        {
            stream.write(reinterpret_cast<const char *>(&value), sizeof(T));
        }

        template <typename T>
        bool ReadValue(std::ifstream &stream, T &value)
        {
            stream.read(reinterpret_cast<char *>(&value), sizeof(T));
            return static_cast<bool>(stream);
        }

        void WriteFloatVector(std::ofstream &stream, const std::vector<float> &values)
        {
            const std::uint64_t count = static_cast<std::uint64_t>(values.size());
            WriteValue(stream, count);
            if (!values.empty())
            {
                stream.write(reinterpret_cast<const char *>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(float)));
            }
        }

        [[nodiscard]] bool ReadFloatVector(std::ifstream &stream, std::vector<float> &values)
        {
            std::uint64_t count = 0u;
            if (!ReadValue(stream, count))
            {
                return false;
            }
            values.assign(static_cast<std::size_t>(count), 0.f);
            if (!values.empty())
            {
                stream.read(reinterpret_cast<char *>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(float)));
            }
            return static_cast<bool>(stream);
        }

        [[nodiscard]] std::shared_ptr<IBLPrecomputeResult> ReadCache(const std::filesystem::path &cachePath, const CacheHeader &expectedHeader)
        {
            std::ifstream stream(cachePath, std::ios::binary);
            if (!stream)
            {
                return {};
            }

            CacheHeader header{};
            if (!ReadValue(stream, header) || !SameHeader(header, expectedHeader))
            {
                return {};
            }

            auto result = std::make_shared<IBLPrecomputeResult>();
            result->cubeSize = header.cubeSize;
            result->specularMipCount = header.specularMipCount;
            result->brdfLutSize = header.brdfLutSize;
            stream.read(reinterpret_cast<char *>(result->irradianceSH.data()), static_cast<std::streamsize>(result->irradianceSH.size() * sizeof(glm::vec4)));
            result->specularMipChain.resize(header.specularMipCount);
            for (std::uint32_t mip = 0; mip < header.specularMipCount; ++mip)
            {
                for (std::uint32_t face = 0; face < 6u; ++face)
                {
                    ReadValue(stream, result->specularMipChain[mip][face].width);
                    ReadValue(stream, result->specularMipChain[mip][face].height);
                    if (!ReadFloatVector(stream, result->specularMipChain[mip][face].rgba32f))
                    {
                        return {};
                    }
                }
            }
            if (!ReadFloatVector(stream, result->brdfLutRG32F) || !result->IsValid())
            {
                return {};
            }
            return result;
        }

        void WriteCache(const std::filesystem::path &cachePath, const CacheHeader &header, const IBLPrecomputeResult &result)
        {
            std::ofstream stream(cachePath, std::ios::binary | std::ios::trunc);
            if (!stream)
            {
                PHYSARA_CORE_WARN("Failed to write IBL cache '{}'.", cachePath.string());
                return;
            }

            WriteValue(stream, header);
            stream.write(reinterpret_cast<const char *>(result.irradianceSH.data()), static_cast<std::streamsize>(result.irradianceSH.size() * sizeof(glm::vec4)));
            for (const std::array<IBLCubeFace, 6> &mip : result.specularMipChain)
            {
                for (const IBLCubeFace &face : mip)
                {
                    WriteValue(stream, face.width);
                    WriteValue(stream, face.height);
                    WriteFloatVector(stream, face.rgba32f);
                }
            }
            WriteFloatVector(stream, result.brdfLutRG32F);
        }

        void WriteSHText(const std::filesystem::path &path, const std::array<glm::vec4, 9> &sh)
        {
            std::ofstream stream(path, std::ios::trunc);
            for (const glm::vec4 &coefficient : sh)
            {
                stream << coefficient.x << ' ' << coefficient.y << ' ' << coefficient.z << '\n';
            }
        }

        void WriteEXRRGBA(const std::filesystem::path &path, const std::vector<float> &rgba, std::uint32_t width, std::uint32_t height)
        {
            if (rgba.empty() || width == 0u || height == 0u)
            {
                return;
            }

            const char *error = nullptr;
            const int result = SaveEXR(
                rgba.data(),
                static_cast<int>(width),
                static_cast<int>(height),
                4,
                0,
                path.string().c_str(),
                &error);
            if (result != TINYEXR_SUCCESS)
            {
                PHYSARA_CORE_WARN("Failed to write EXR '{}': {}", path.string(), error != nullptr ? error : "Unknown error");
                if (error != nullptr)
                {
                    FreeEXRErrorMessage(error);
                }
            }
        }

        void WriteBRDFLutEXR(const std::filesystem::path &path, const std::vector<float> &rg, std::uint32_t size)
        {
            if (rg.empty() || size == 0u)
            {
                return;
            }

            std::vector<float> rgba(static_cast<std::size_t>(size) * size * 4u, 0.f);
            for (std::size_t i = 0u; i < static_cast<std::size_t>(size) * size; ++i)
            {
                rgba[i * 4u + 0u] = rg[i * 2u + 0u];
                rgba[i * 4u + 1u] = rg[i * 2u + 1u];
                rgba[i * 4u + 3u] = 1.f;
            }
            WriteEXRRGBA(path, rgba, size, size);
        }

        [[nodiscard]] bool DebugEXROutputsExist(const std::filesystem::path &cacheDirectory, const IBLPrecomputeResult &result)
        {
            if (result.specularMipChain.empty())
            {
                return false;
            }

            std::error_code error{};
            return std::filesystem::exists(cacheDirectory / "brdf_integrate.exr", error) &&
                   std::filesystem::exists(cacheDirectory / "equirect_to_cube_face0.exr", error) &&
                   std::filesystem::exists(cacheDirectory / "specular_prefilter_mip0_face0.exr", error);
        }

        void WriteDebugEXROutputs(const std::filesystem::path &cacheDirectory, const IBLPrecomputeResult &result)
        {
            if (!result.IsValid())
            {
                return;
            }

            std::error_code error{};
            std::filesystem::remove(cacheDirectory / "brdf_integrate_rg32f.bin", error);
            for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(cacheDirectory, error))
            {
                if (error || !entry.is_regular_file(error))
                {
                    continue;
                }

                const std::string fileName = entry.path().filename().generic_string();
                if (fileName.rfind("specular_prefilter_mip", 0) == 0 && fileName.ends_with("_rgba32f.bin"))
                {
                    std::filesystem::remove(entry.path(), error);
                }
            }

            WriteSHText(cacheDirectory / "irradiance_sh.txt", result.irradianceSH);
            WriteBRDFLutEXR(cacheDirectory / "brdf_integrate.exr", result.brdfLutRG32F, result.brdfLutSize);
            for (std::uint32_t mip = 0; mip < result.specularMipCount; ++mip)
            {
                for (std::uint32_t face = 0; face < 6u; ++face)
                {
                    const IBLCubeFace &cubeFace = result.specularMipChain[mip][face];
                    WriteEXRRGBA(
                        cacheDirectory / ("specular_prefilter_mip" + std::to_string(mip) + "_face" + std::to_string(face) + ".exr"),
                        cubeFace.rgba32f,
                        cubeFace.width,
                        cubeFace.height);
                    if (mip == 0u)
                    {
                        WriteEXRRGBA(
                            cacheDirectory / ("equirect_to_cube_face" + std::to_string(face) + ".exr"),
                            cubeFace.rgba32f,
                            cubeFace.width,
                            cubeFace.height);
                    }
                }
            }
        }
    }

    std::filesystem::path IBLPrecompute::GetCacheDirectory(const std::filesystem::path &environmentPath)
    {
        const std::filesystem::path normalized = environmentPath.lexically_normal();
        return normalized.parent_path() / normalized.stem();
    }

    std::shared_ptr<IBLPrecomputeResult> IBLPrecompute::LoadOrCreate(
        const std::filesystem::path &environmentPath,
        const IBLPrecomputeSettings &settings)
    {
        if (environmentPath.empty())
        {
            return {};
        }

        const IBLPrecomputeSettings clampedSettings{
            std::clamp(settings.cubeSize, 16u, 512u),
            std::clamp(settings.brdfLutSize, 32u, 512u),
            std::clamp(settings.specularSampleCount, 8u, 1024u),
            std::clamp(settings.brdfSampleCount, 16u, 2048u)};
        const IBLPrecomputeDetail::CacheHeader expectedHeader = IBLPrecomputeDetail::BuildHeader(environmentPath, clampedSettings);
        const std::filesystem::path cacheDirectory = GetCacheDirectory(environmentPath);
        const std::filesystem::path cachePath = cacheDirectory / "physara_ibl_cache_v2.bin";

        if (std::shared_ptr<IBLPrecomputeResult> cached = IBLPrecomputeDetail::ReadCache(cachePath, expectedHeader))
        {
            if (!IBLPrecomputeDetail::DebugEXROutputsExist(cacheDirectory, *cached))
            {
                std::error_code error{};
                std::filesystem::create_directories(cacheDirectory, error);
                if (!error)
                {
                    IBLPrecomputeDetail::WriteDebugEXROutputs(cacheDirectory, *cached);
                }
            }
            PHYSARA_CORE_INFO("Loaded IBL precompute cache '{}'.", cachePath.string());
            return cached;
        }

        const std::shared_ptr<Texture> panorama = TextureLoader::LoadRGBA32F(environmentPath);
        if (panorama == nullptr || !panorama->IsLoaded() || panorama->rgba32fPixels.empty())
        {
            PHYSARA_CORE_WARN("IBL precompute skipped because environment '{}' could not be loaded.", environmentPath.string());
            return {};
        }

        PHYSARA_CORE_INFO("Precomputing IBL for '{}' into '{}'.", environmentPath.string(), cacheDirectory.string());
        auto result = std::make_shared<IBLPrecomputeResult>();
        result->cubeSize = clampedSettings.cubeSize;
        result->specularMipCount = IBLPrecomputeDetail::CalculateMipCount(clampedSettings.cubeSize);
        result->brdfLutSize = clampedSettings.brdfLutSize;
        result->irradianceSH = IBLPrecomputeDetail::ComputeIrradianceSH(*panorama);
        result->specularMipChain.resize(result->specularMipCount);
        result->specularMipChain[0] = IBLPrecomputeDetail::EquirectToCube(*panorama, clampedSettings.cubeSize);
        for (std::uint32_t mip = 1u; mip < result->specularMipCount; ++mip)
        {
            const std::uint32_t mipSize = std::max(1u, clampedSettings.cubeSize >> mip);
            const float roughness = static_cast<float>(mip) / static_cast<float>(std::max(result->specularMipCount - 1u, 1u));
            result->specularMipChain[mip] = IBLPrecomputeDetail::PrefilterMip(*panorama, mipSize, roughness, clampedSettings.specularSampleCount);
        }
        result->brdfLutRG32F = IBLPrecomputeDetail::BuildBRDFLut(clampedSettings.brdfLutSize, clampedSettings.brdfSampleCount);

        if (!result->IsValid())
        {
            PHYSARA_CORE_WARN("IBL precompute for '{}' produced invalid data.", environmentPath.string());
            return {};
        }

        std::error_code error{};
        std::filesystem::create_directories(cacheDirectory, error);
        if (!error)
        {
            IBLPrecomputeDetail::WriteCache(cachePath, expectedHeader, *result);
            IBLPrecomputeDetail::WriteDebugEXROutputs(cacheDirectory, *result);
        }

        return result;
    }
}