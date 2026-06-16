#pragma once

#include <cstdint>

#include <Engine/Scene/Components/MaterialComponent.hpp>

namespace Physara::Engine::MaterialSignature
{
    [[nodiscard]] std::uint64_t Build(const MaterialComponent &material);
    [[nodiscard]] std::uint64_t BuildTextureSet(const MaterialComponent &material);
    [[nodiscard]] bool Equals(const MaterialComponent &lhs, const MaterialComponent &rhs);
}