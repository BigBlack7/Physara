#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Engine/Renderer/MaterialInstance.hpp>
#include <Engine/Scene/Components/MaterialComponent.hpp>

namespace Physara::Engine
{
    class MaterialInstanceRegistry final
    {
    public:
        void Reset();

        [[nodiscard]] MaterialInstanceId Resolve(const MaterialComponent &material);
        [[nodiscard]] const MaterialComponent *Get(MaterialInstanceId id) const;
        [[nodiscard]] std::uint64_t GetSignature(MaterialInstanceId id) const;
        [[nodiscard]] std::size_t GetCount() const { return m_Entries.size(); }

    private:
        struct Entry
        {
            MaterialInstanceId id{InvalidMaterialInstanceId};
            std::uint64_t signature{0u};
            MaterialComponent material{};
        };

        [[nodiscard]] static std::uint64_t HashMaterial(const MaterialComponent &material);
        [[nodiscard]] static std::uint64_t HashString(std::string_view value);
        static void HashCombine(std::uint64_t &seed, std::string_view value);
        static void HashCombine(std::uint64_t &seed, std::uint64_t value);
        static void HashCombine(std::uint64_t &seed, float value);
        static void HashCombine(std::uint64_t &seed, const glm::vec3 &value);
        static void HashCombine(std::uint64_t &seed, const glm::vec4 &value);
        static void HashCombine(std::uint64_t &seed, const TextureSlot &slot);
        [[nodiscard]] static bool TextureSlotEquals(const TextureSlot &lhs, const TextureSlot &rhs);
        [[nodiscard]] static bool MaterialEquals(const MaterialComponent &lhs, const MaterialComponent &rhs);

    private:
        std::vector<Entry> m_Entries{};
        std::unordered_map<std::uint64_t, std::vector<MaterialInstanceId>> m_IdsBySignature{};
    };
}