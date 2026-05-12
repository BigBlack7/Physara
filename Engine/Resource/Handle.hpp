#pragma once

#include <cstdint>
#include <functional>

namespace Physara::Engine
{
    using ResourceId = std::uint32_t;         // 资源ID
    using ResourceGeneration = std::uint32_t; // 资源世代

    constexpr ResourceId InvalidResourceId = 0;
    constexpr ResourceGeneration InvalidResourceGeneration = 0;

    struct UntypedResourceHandle // 无类型资源句柄
    {
        ResourceId id{InvalidResourceId};
        ResourceGeneration generation{InvalidResourceGeneration};

        [[nodiscard]] bool IsValid() const
        {
            return id != InvalidResourceId && generation != InvalidResourceGeneration;
        }

        explicit operator bool() const { return IsValid(); }

        [[nodiscard]] friend bool operator==(UntypedResourceHandle lhs, UntypedResourceHandle rhs)
        {
            return lhs.id == rhs.id && lhs.generation == rhs.generation;
        }

        [[nodiscard]] friend bool operator!=(UntypedResourceHandle lhs, UntypedResourceHandle rhs)
        {
            return !(lhs == rhs);
        }
    };

    template <typename T>
    struct ResourceHandle // 资源句柄
    {
        ResourceId id{InvalidResourceId};
        ResourceGeneration generation{InvalidResourceGeneration};

        ResourceHandle() = default;
        constexpr ResourceHandle(ResourceId resourceId, ResourceGeneration resourceGeneration)
            : id(resourceId), generation(resourceGeneration) {}

        [[nodiscard]] bool IsValid() const
        {
            return id != InvalidResourceId && generation != InvalidResourceGeneration;
        }

        explicit operator bool() const { return IsValid(); }

        [[nodiscard]] UntypedResourceHandle Untyped() const
        {
            return UntypedResourceHandle{id, generation};
        }

        [[nodiscard]] friend bool operator==(ResourceHandle lhs, ResourceHandle rhs)
        {
            return lhs.id == rhs.id && lhs.generation == rhs.generation;
        }

        [[nodiscard]] friend bool operator!=(ResourceHandle lhs, ResourceHandle rhs)
        {
            return !(lhs == rhs);
        }
    };
}

namespace std
{
    template <typename T>
    struct hash<Physara::Engine::ResourceHandle<T>>
    {
        std::size_t operator()(Physara::Engine::ResourceHandle<T> handle) const noexcept
        {
            const std::uint64_t packed =
                (static_cast<std::uint64_t>(handle.generation) << 32u) |
                static_cast<std::uint64_t>(handle.id);
            return std::hash<std::uint64_t>{}(packed);
        }
    };

    template <>
    struct hash<Physara::Engine::UntypedResourceHandle>
    {
        std::size_t operator()(Physara::Engine::UntypedResourceHandle handle) const noexcept
        {
            const std::uint64_t packed =
                (static_cast<std::uint64_t>(handle.generation) << 32u) |
                static_cast<std::uint64_t>(handle.id);
            return std::hash<std::uint64_t>{}(packed);
        }
    };
}