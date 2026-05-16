#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <Engine/RHI/Resource/RHIShader.hpp>

namespace Physara::Engine
{
    using ShaderFeatureMask = std::uint64_t;

    namespace ShaderFeature
    {
        constexpr ShaderFeatureMask None = 0ull;
        constexpr ShaderFeatureMask HasBaseColorMap = 1ull << 0;
        constexpr ShaderFeatureMask HasMetallicRoughnessMap = 1ull << 1;
        constexpr ShaderFeatureMask HasNormalMap = 1ull << 2;
        constexpr ShaderFeatureMask HasOcclusionMap = 1ull << 3;
        constexpr ShaderFeatureMask HasEmissiveMap = 1ull << 4;
        constexpr ShaderFeatureMask AlphaTest = 1ull << 5;
        constexpr ShaderFeatureMask AlphaBlend = 1ull << 6;
        constexpr ShaderFeatureMask Skinning = 1ull << 7;
        constexpr ShaderFeatureMask ReceiveShadow = 1ull << 8;
        constexpr ShaderFeatureMask CastShadow = 1ull << 9;
        constexpr ShaderFeatureMask Unlit = 1ull << 10;
    }

    struct ShaderDefine
    {
        std::string name{};
        std::string value{"1"};
    };

    struct ShaderSource
    {
        RHI::ShaderStage stage{RHI::ShaderStage::Vertex};
        std::string path{};
        std::string source{};
        std::vector<std::string> dependencies{};
    };

    struct ShaderVariantKey
    {
        std::string debugName{};
        std::string vertexPath{};
        std::string fragmentPath{};
        std::string computePath{};
        ShaderFeatureMask featureMask{ShaderFeature::None};
        std::vector<ShaderDefine> defines{};

        [[nodiscard]] bool operator==(const ShaderVariantKey &other) const;
    };

    struct ShaderVariant
    {
        ShaderVariantKey key{};
        std::unique_ptr<RHI::RHIShader> vertexShader{};
        std::unique_ptr<RHI::RHIShader> fragmentShader{};
        std::unique_ptr<RHI::RHIShader> computeShader{};
        std::vector<std::string> dependencies{};

        [[nodiscard]] bool IsValid() const;
        [[nodiscard]] bool IsCompute() const { return computeShader != nullptr; }
    };
}

namespace std
{
    template <>
    struct hash<Physara::Engine::ShaderDefine>
    {
        std::size_t operator()(const Physara::Engine::ShaderDefine &define) const noexcept;
    };

    template <>
    struct hash<Physara::Engine::ShaderVariantKey>
    {
        std::size_t operator()(const Physara::Engine::ShaderVariantKey &key) const noexcept;
    };
}