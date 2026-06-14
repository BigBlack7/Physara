#include "MaterialInstanceRegistry.hpp"

#include <utility>

#include <Engine/Renderer/MaterialSignature.hpp>

namespace Physara::Engine
{
    void MaterialInstanceRegistry::Reset()
    {
        m_Entries.clear();
        m_IdsBySignature.clear();
    }

    MaterialInstanceId MaterialInstanceRegistry::Resolve(const MaterialComponent &material)
    {
        return Resolve(MaterialSignature::Build(material), material);
    }

    MaterialInstanceId MaterialInstanceRegistry::Resolve(std::uint64_t signature, const MaterialComponent &material)
    {
        auto &candidateIds = m_IdsBySignature[signature];
        for (MaterialInstanceId candidateId : candidateIds)
        {
            const MaterialComponent *candidate = Get(candidateId);
            if (candidate != nullptr && MaterialSignature::Equals(*candidate, material))
            {
                return candidateId;
            }
        }

        const MaterialInstanceId id = static_cast<MaterialInstanceId>(m_Entries.size());
        Entry entry{};
        entry.id = id;
        entry.signature = signature;
        entry.material = material;
        m_Entries.push_back(std::move(entry));
        candidateIds.push_back(id);
        return id;
    }

    const MaterialComponent *MaterialInstanceRegistry::Get(MaterialInstanceId id) const
    {
        if (id == InvalidMaterialInstanceId || id >= m_Entries.size())
        {
            return nullptr;
        }
        return &m_Entries[id].material;
    }

    std::uint64_t MaterialInstanceRegistry::GetSignature(MaterialInstanceId id) const
    {
        if (id == InvalidMaterialInstanceId || id >= m_Entries.size())
        {
            return 0u;
        }
        return m_Entries[id].signature;
    }
}
