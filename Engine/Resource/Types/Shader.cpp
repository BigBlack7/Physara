#include "Shader.hpp"

#include <functional>

namespace Physara::Engine
{
    namespace ShaderDetail
    {
        void HashCombine(std::size_t &seed, std::size_t value)
        {
            seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
        }
    }

    bool ShaderVariantKey::operator==(const ShaderVariantKey &other) const
    {
        if (debugName != other.debugName ||
            vertexPath != other.vertexPath ||
            fragmentPath != other.fragmentPath ||
            computePath != other.computePath ||
            featureMask != other.featureMask ||
            defines.size() != other.defines.size())
        {
            return false;
        }

        for (std::size_t i = 0; i < defines.size(); ++i)
        {
            if (defines[i].name != other.defines[i].name || defines[i].value != other.defines[i].value)
            {
                return false;
            }
        }

        return true;
    }

    bool ShaderVariant::IsValid() const
    {
        if (computeShader != nullptr)
        {
            return computeShader->IsValid();
        }
        return vertexShader != nullptr && vertexShader->IsValid() &&
               fragmentShader != nullptr && fragmentShader->IsValid();
    }
}

namespace std
{
    std::size_t hash<Physara::Engine::ShaderDefine>::operator()(const Physara::Engine::ShaderDefine &define) const noexcept
    {
        std::size_t seed = std::hash<std::string>{}(define.name);
        Physara::Engine::ShaderDetail::HashCombine(seed, std::hash<std::string>{}(define.value));
        return seed;
    }

    std::size_t hash<Physara::Engine::ShaderVariantKey>::operator()(const Physara::Engine::ShaderVariantKey &key) const noexcept
    {
        std::size_t seed = std::hash<std::string>{}(key.debugName);
        Physara::Engine::ShaderDetail::HashCombine(seed, std::hash<std::string>{}(key.vertexPath));
        Physara::Engine::ShaderDetail::HashCombine(seed, std::hash<std::string>{}(key.fragmentPath));
        Physara::Engine::ShaderDetail::HashCombine(seed, std::hash<std::string>{}(key.computePath));
        Physara::Engine::ShaderDetail::HashCombine(seed, std::hash<Physara::Engine::ShaderFeatureMask>{}(key.featureMask));

        for (const Physara::Engine::ShaderDefine &define : key.defines)
        {
            Physara::Engine::ShaderDetail::HashCombine(seed, std::hash<Physara::Engine::ShaderDefine>{}(define));
        }

        return seed;
    }
}