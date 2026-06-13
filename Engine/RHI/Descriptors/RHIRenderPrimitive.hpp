#pragma once

#include <cstdint>
#include <span>

namespace Physara::RHI
{
    class RHIBuffer;

    struct RHIVertexBufferBinding
    {
        std::uint32_t slot{0};
        RHIBuffer *buffer{nullptr};
        std::uint32_t offset{0};
    };

    struct RHIIndexBufferBinding
    {
        RHIBuffer *buffer{nullptr};
        std::uint32_t offset{0};
    };

    struct RHIRenderPrimitive
    {
        std::uint64_t stableId{0};
        std::span<const RHIVertexBufferBinding> vertexBuffers{};
        RHIIndexBufferBinding indexBuffer{};
    };
}