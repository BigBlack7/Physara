/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#include "RHI.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "Utils.hpp"

namespace Physara::RHI
{
    // Do not move this function into the header.
    bool VerifyHeaderVersion(uint32_t version)
    {
        return version == HeaderVersion;
    }

    TextureSlice TextureSlice::Resolve(const TextureDesc &desc) const
    {
        TextureSlice ret(*this);

        assert(mipLevel < desc.mipLevels && mipLevel < 32);

        if (width == uint32_t(-1))
        {
            ret.width = std::max(desc.width >> mipLevel, 1u);
        }

        if (height == uint32_t(-1))
        {
            ret.height = std::max(desc.height >> mipLevel, 1u);
        }

        if (depth == uint32_t(-1))
        {
            if (desc.dimension == TextureDimension::Texture3D)
            {
                ret.depth = std::max(desc.depth >> mipLevel, 1u);
            }
            else
            {
                ret.depth = 1;
            }
        }

        return ret;
    }

    TextureSubresourceSet TextureSubresourceSet::Resolve(const TextureDesc &desc, bool singleMipLevel) const
    {
        TextureSubresourceSet ret;
        ret.baseMipLevel = baseMipLevel;

        if (singleMipLevel)
        {
            ret.numMipLevels = baseMipLevel < desc.mipLevels ? 1u : 0u;
        }
        else
        {
            ret.numMipLevels =
                baseMipLevel < desc.mipLevels ? std::min(numMipLevels, desc.mipLevels - baseMipLevel) : 0u;
        }

        switch (desc.dimension) // NOLINT(clang-diagnostic-switch-enum)
        {
        case TextureDimension::Texture1DArray:
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
        case TextureDimension::TextureCubeArray:
        case TextureDimension::Texture2DMSArray:
        {
            ret.baseArraySlice = baseArraySlice;
            ret.numArraySlices =
                baseArraySlice < desc.arraySize ? std::min(numArraySlices, desc.arraySize - baseArraySlice) : 0u;
            break;
        }
        default:
            ret.baseArraySlice = 0;
            ret.numArraySlices = 1;
            break;
        }

        return ret;
    }

    bool TextureSubresourceSet::IsEntireTexture(const TextureDesc &desc) const
    {
        if (baseMipLevel > 0u || numMipLevels < desc.mipLevels)
        {
            return false;
        }

        switch (desc.dimension) // NOLINT(clang-diagnostic-switch-enum)
        {
        case TextureDimension::Texture1DArray:
        case TextureDimension::Texture2DArray:
        case TextureDimension::TextureCube:
        case TextureDimension::TextureCubeArray:
        case TextureDimension::Texture2DMSArray:
            if (baseArraySlice > 0u || numArraySlices < desc.arraySize)
            {
                return false;
            }
        default:
            return true;
        }
    }

    BufferRange BufferRange::Resolve(const BufferDesc &desc) const
    {
        BufferRange result;
        result.byteOffset = std::min(byteOffset, desc.byteSize);
        if (byteSize == 0)
        {
            result.byteSize = desc.byteSize - result.byteOffset;
        }
        else
        {
            result.byteSize = std::min(byteSize, desc.byteSize - result.byteOffset);
        }
        return result;
    }

    bool BlendState::RenderTarget::UsesConstantColor() const
    {
        return srcBlend == BlendFactor::ConstantColor || srcBlend == BlendFactor::OneMinusConstantColor ||
               destBlend == BlendFactor::ConstantColor || destBlend == BlendFactor::OneMinusConstantColor ||
               srcBlendAlpha == BlendFactor::ConstantColor || srcBlendAlpha == BlendFactor::OneMinusConstantColor ||
               destBlendAlpha == BlendFactor::ConstantColor || destBlendAlpha == BlendFactor::OneMinusConstantColor;
    }

    bool BlendState::UsesConstantColor(uint32_t numTargets) const
    {
        for (uint32_t rt = 0; rt < numTargets; rt++)
        {
            if (targets[rt].UsesConstantColor())
            {
                return true;
            }
        }

        return false;
    }

    FramebufferInfo::FramebufferInfo(const FramebufferDesc &desc)
    {
        for (size_t i = 0; i < desc.colorAttachments.size(); i++)
        {
            const FramebufferAttachment &attachment = desc.colorAttachments[i];
            colorFormats.push_back(attachment.format == Format::UNKNOWN && attachment.texture
                                       ? attachment.texture->GetDesc().format
                                       : attachment.format);
        }

        if (desc.depthAttachment.Valid())
        {
            const TextureDesc &textureDesc = desc.depthAttachment.texture->GetDesc();
            depthFormat = textureDesc.format;
            sampleCount = textureDesc.sampleCount;
            sampleQuality = textureDesc.sampleQuality;
        }
        else if (!desc.colorAttachments.empty() && desc.colorAttachments[0].Valid())
        {
            const TextureDesc &textureDesc = desc.colorAttachments[0].texture->GetDesc();
            sampleCount = textureDesc.sampleCount;
            sampleQuality = textureDesc.sampleQuality;
        }
    }

    FramebufferInfoEx::FramebufferInfoEx(const FramebufferDesc &desc) : FramebufferInfo(desc)
    {
        if (desc.depthAttachment.Valid())
        {
            const TextureDesc &textureDesc = desc.depthAttachment.texture->GetDesc();
            TextureSubresourceSet const subresources = desc.depthAttachment.subresources.Resolve(textureDesc, true);
            width = std::max(textureDesc.width >> subresources.baseMipLevel, 1u);
            height = std::max(textureDesc.height >> subresources.baseMipLevel, 1u);
            arraySize = subresources.numArraySlices;
        }
        else if (!desc.colorAttachments.empty() && desc.colorAttachments[0].Valid())
        {
            const TextureDesc &textureDesc = desc.colorAttachments[0].texture->GetDesc();
            TextureSubresourceSet const subresources = desc.colorAttachments[0].subresources.Resolve(textureDesc, true);
            width = std::max(textureDesc.width >> subresources.baseMipLevel, 1u);
            height = std::max(textureDesc.height >> subresources.baseMipLevel, 1u);
            arraySize = subresources.numArraySlices;
        }
    }

    void CommandList::SetResourceStatesForFramebuffer(Framebuffer *framebuffer)
    {
        const FramebufferDesc &desc = framebuffer->GetDesc();

        for (const auto &attachment : desc.colorAttachments)
        {
            SetTextureState(attachment.texture, attachment.subresources, ResourceStates::RenderTarget);
        }

        if (desc.depthAttachment.Valid())
        {
            SetTextureState(desc.depthAttachment.texture, desc.depthAttachment.subresources,
                            desc.depthAttachment.isReadOnly ? ResourceStates::DepthRead : ResourceStates::DepthWrite);
        }

        if (desc.shadingRateAttachment.Valid())
        {
            SetTextureState(desc.shadingRateAttachment.texture, desc.shadingRateAttachment.subresources,
                            Physara::RHI::ResourceStates::ShadingRateSurface);
        }
    }

    size_t CoopVec::GetDataTypeSize(CoopVec::DataType type)
    {
        switch (type)
        {
        case CoopVec::DataType::UInt8:
        case CoopVec::DataType::SInt8:
            return 1;
        case CoopVec::DataType::UInt8Packed:
        case CoopVec::DataType::SInt8Packed:
            // Not sure if this is correct or even relevant because packed types
            // cannot be used in matrices accessible from the host side.
            return 1;
        case CoopVec::DataType::UInt16:
        case CoopVec::DataType::SInt16:
            return 2;
        case CoopVec::DataType::UInt32:
        case CoopVec::DataType::SInt32:
            return 4;
        case CoopVec::DataType::UInt64:
        case CoopVec::DataType::SInt64:
            return 8;
        case CoopVec::DataType::FloatE4M3:
        case CoopVec::DataType::FloatE5M2:
            return 1;
        case CoopVec::DataType::Float16:
        case CoopVec::DataType::BFloat16:
            return 2;
        case CoopVec::DataType::Float32:
            return 4;
        case CoopVec::DataType::Float64:
            return 8;
        default:
            Utils::InvalidEnum();
            return 0;
        }
    }

    size_t CoopVec::GetOptimalMatrixStride(CoopVec::DataType type, CoopVec::MatrixLayout layout, uint32_t rows,
                                           uint32_t columns)
    {
        size_t const dataTypeSize = CoopVec::GetDataTypeSize(type);

        uint32_t count = 0;
        switch (layout)
        {
        case CoopVec::MatrixLayout::RowMajor:
            count = columns;
            break;
        case CoopVec::MatrixLayout::ColumnMajor:
            count = rows;
            break;
        default:
            return 0;
        }
        if (count != 0 && dataTypeSize > std::numeric_limits<size_t>::max() / count)
        {
            throw std::length_error("Matrix stride is not representable");
        }
        return dataTypeSize * count;
    }

} // namespace Physara::RHI
