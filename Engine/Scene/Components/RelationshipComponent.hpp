#pragma once

#include <cstdint>

#include <Engine/Scene/EntityId.hpp>

namespace Physara::Engine
{
    struct RelationshipComponent
    {
        EntityId parent{NullEntity};
        EntityId firstChild{NullEntity};
        EntityId nextSibling{NullEntity};
        EntityId previousSibling{NullEntity};
        std::uint32_t childCount{0};

        [[nodiscard]] bool HasParent() const { return parent != NullEntity; }
        [[nodiscard]] bool HasChildren() const { return firstChild != NullEntity; }
        [[nodiscard]] bool HasNextSibling() const { return nextSibling != NullEntity; }
        [[nodiscard]] bool HasPreviousSibling() const { return previousSibling != NullEntity; }
        [[nodiscard]] bool IsRoot() const { return parent == NullEntity; }
    };
}