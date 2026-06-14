#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Physara::Engine::UploadHash
{
    constexpr std::uint64_t Offset = 14695981039346656037ull;
    constexpr std::uint64_t Prime = 1099511628211ull;

    inline std::uint64_t Bytes(std::uint64_t hash, const void *data, std::size_t size)
    {
        const auto *bytes = static_cast<const std::uint8_t *>(data);
        for (std::size_t i = 0; i < size; ++i)
        {
            hash ^= bytes[i];
            hash *= Prime;
        }
        return hash;
    }

    template <typename T>
    std::uint64_t Value(std::uint64_t hash, const T &value)
    {
        return Bytes(hash, &value, sizeof(T));
    }

    inline std::uint64_t String(std::uint64_t hash, std::string_view value)
    {
        hash = Value(hash, value.size());
        return value.empty() ? hash : Bytes(hash, value.data(), value.size());
    }

    inline std::uint64_t String(std::uint64_t hash, const std::string &value)
    {
        return String(hash, std::string_view{value});
    }

    template <typename T>
    std::uint64_t Vector(std::uint64_t hash, const std::vector<T> &values)
    {
        hash = Value(hash, values.size());
        return values.empty() ? hash : Bytes(hash, values.data(), values.size() * sizeof(T));
    }
}
