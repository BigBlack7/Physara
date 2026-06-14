#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <Engine/Renderer/MaterialInstance.hpp>
#include <Engine/Scene/Components/MaterialComponent.hpp>

namespace Physara::Engine
{
    class MaterialInstanceRegistry final
    {
    public:
        void Reset();

        [[nodiscard]] MaterialInstanceId Resolve(const MaterialComponent &material);
        [[nodiscard]] MaterialInstanceId Resolve(std::uint64_t signature, const MaterialComponent &material);
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

        std::vector<Entry> m_Entries{};
        std::unordered_map<std::uint64_t, std::vector<MaterialInstanceId>> m_IdsBySignature{};
    };
}
