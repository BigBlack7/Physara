#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>

namespace Physara::RHI
{
    class RHITexture;
}

namespace Physara::Engine
{
    struct RenderGraphResourceHandle
    {
        std::uint32_t index{std::numeric_limits<std::uint32_t>::max()};

        [[nodiscard]] bool IsValid() const
        {
            return index != std::numeric_limits<std::uint32_t>::max();
        }

        friend bool operator==(RenderGraphResourceHandle lhs, RenderGraphResourceHandle rhs)
        {
            return lhs.index == rhs.index;
        }
    };

    class ResourceNode final
    {
    public:
        ResourceNode() = default;
        ResourceNode(std::string name, RHI::RHITextureDesc desc, RHI::RHITexture *texture)
            : m_Name(std::move(name)), m_TextureDesc(desc), m_Texture(texture) {}

        [[nodiscard]] const std::string &GetName() const { return m_Name; }
        [[nodiscard]] const RHI::RHITextureDesc &GetTextureDesc() const { return m_TextureDesc; }
        [[nodiscard]] RHI::RHITexture *GetTexture() const { return m_Texture; }

    private:
        std::string m_Name{};
        RHI::RHITextureDesc m_TextureDesc{};
        RHI::RHITexture *m_Texture{nullptr};
    };
}