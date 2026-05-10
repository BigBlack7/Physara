#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace Physara::Engine
{
    struct RelationshipComponent
    {
        entt::entity parent{entt::null};
        entt::entity firstChild{entt::null};
        entt::entity nextSibling{entt::null};
        entt::entity previousSibling{entt::null};
        std::uint32_t childCount{0};

        [[nodiscard]] bool HasParent() const { return parent != entt::null; }
        [[nodiscard]] bool HasChildren() const { return firstChild != entt::null; }
        [[nodiscard]] bool HasNextSibling() const { return nextSibling != entt::null; }
        [[nodiscard]] bool HasPreviousSibling() const { return previousSibling != entt::null; }
        [[nodiscard]] bool IsRoot() const { return parent == entt::null; }
    };
}