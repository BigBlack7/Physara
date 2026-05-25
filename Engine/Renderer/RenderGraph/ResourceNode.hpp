#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>
#include <Engine/RHI/Resource/RHITexture.hpp>

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
        ResourceNode(std::string name, RHI::RHITextureDesc desc, RHI::RHITexture *texture, bool imported)
            : m_Name(std::move(name)), m_TextureDesc(desc), m_Texture(texture), m_Imported(imported) {}

        [[nodiscard]] const std::string &GetName() const { return m_Name; }
        [[nodiscard]] const RHI::RHITextureDesc &GetTextureDesc() const { return m_TextureDesc; }
        [[nodiscard]] RHI::RHITexture *GetTexture() const { return m_Texture; }
        [[nodiscard]] bool IsImported() const { return m_Imported; }
        [[nodiscard]] bool IsOutput() const { return m_Output; }

        void SetOutput(bool output) { m_Output = output; }
        void SetTexture(RHI::RHITexture *texture) { m_Texture = texture; }
        void AcquireOwnedTexture(std::unique_ptr<RHI::RHITexture> texture)
        {
            m_OwnedTexture = std::move(texture);
            m_Texture = m_OwnedTexture.get();
        }
        [[nodiscard]] std::unique_ptr<RHI::RHITexture> ReleaseOwnedTexture()
        {
            m_Texture = nullptr;
            return std::move(m_OwnedTexture);
        }

    private:
        std::string m_Name{};
        RHI::RHITextureDesc m_TextureDesc{};
        RHI::RHITexture *m_Texture{nullptr};
        std::unique_ptr<RHI::RHITexture> m_OwnedTexture{};
        bool m_Imported{false};
        bool m_Output{false};
    };
}