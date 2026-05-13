#include "ResourceNode.hpp"

namespace Physara::Engine
{
    ResourceNode::ResourceNode(std::string name, RHI::RHITextureDesc desc, RHI::RHITexture *texture, bool imported)
        : m_Name(std::move(name)),
          m_TextureDesc(desc),
          m_Texture(texture),
          m_Imported(imported)
    {
    }
}