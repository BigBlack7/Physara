#include "Backend.hpp"

#include <algorithm>
#include <bit>
#include <exception>
#include <memory>
#include <new>
#include <utility>

namespace Physara::RHI::OpenGL
{
    OpenGLTexture::OpenGLTexture(OpenGLDevice *device, const TextureDesc &desc)
        : m_Device(device), m_Desc(desc), m_Deletion(std::make_unique<NativeDeletion>(NativeKind::Texture))
    {
    }

    OpenGLTexture::~OpenGLTexture()
    {
        m_Device->ReleaseAllocation(std::move(m_Deletion), m_Published);
    }

    TextureHandle OpenGLDevice::CreateTexture(const TextureDesc &desc)
    {
        if (!RequireContext("CreateTexture"))
        {
            return nullptr;
        }
        const GLenum target = TextureTarget(desc.dimension);
        const bool multisample =
            desc.dimension == TextureDimension::Texture2DMS || desc.dimension == TextureDimension::Texture2DMSArray;
        const bool cube =
            desc.dimension == TextureDimension::TextureCube || desc.dimension == TextureDimension::TextureCubeArray;
        const bool volume = desc.dimension == TextureDimension::Texture3D;
        const auto spatialLimit = std::uint32_t(volume ? m_Caps.max3DTextureSize
                                                : cube ? m_Caps.maxCubeTextureSize
                                                       : m_Caps.maxTextureSize);
        bool shapeValid = target != 0 && desc.width > 0 && desc.width <= spatialLimit && desc.height > 0 &&
                          desc.height <= spatialLimit && desc.depth > 0 && desc.depth <= spatialLimit &&
                          desc.arraySize > 0;
        switch (desc.dimension)
        {
        case TextureDimension::Texture1D:
            shapeValid = shapeValid && desc.height == 1 && desc.depth == 1 && desc.arraySize == 1;
            break;
        case TextureDimension::Texture1DArray:
            shapeValid = shapeValid && desc.height == 1 && desc.depth == 1 &&
                         desc.arraySize <= std::uint32_t(m_Caps.maxArrayLayers);
            break;
        case TextureDimension::Texture2D:
        case TextureDimension::Texture2DMS:
            shapeValid = shapeValid && desc.depth == 1 && desc.arraySize == 1;
            break;
        case TextureDimension::Texture2DArray:
        case TextureDimension::Texture2DMSArray:
            shapeValid = shapeValid && desc.depth == 1 && desc.arraySize <= std::uint32_t(m_Caps.maxArrayLayers);
            break;
        case TextureDimension::TextureCube:
            shapeValid = shapeValid && desc.width == desc.height && desc.depth == 1 && desc.arraySize == 6;
            break;
        case TextureDimension::TextureCubeArray:
            shapeValid = shapeValid && desc.width == desc.height && desc.depth == 1 && desc.arraySize % 6 == 0 &&
                         desc.arraySize <= std::uint32_t(m_Caps.maxArrayLayers);
            break;
        case TextureDimension::Texture3D:
            shapeValid = shapeValid && desc.arraySize == 1;
            break;
        default:
            shapeValid = false;
            break;
        }
        const auto maxMipLevels = std::uint32_t(std::bit_width(std::max({desc.width, desc.height, desc.depth})));
        const auto &caps = GetFormatCaps(desc.format, desc.dimension);
        const ComponentSwizzle components[] = {desc.defaultComponentMapping.r, desc.defaultComponentMapping.g,
                                               desc.defaultComponentMapping.b, desc.defaultComponentMapping.a};
        for (auto component : components)
        {
            shapeValid = shapeValid && component <= ComponentSwizzle::One;
        }
        if (!shapeValid || desc.mipLevels == 0 || desc.mipLevels > maxMipLevels || desc.sampleQuality != 0 ||
            (multisample
                 ? (desc.mipLevels != 1 || desc.sampleCount > 8 || (caps.sampleMask & (1u << desc.sampleCount)) == 0)
                 : desc.sampleCount != 1) ||
            !ValidInitialState(desc.initialState, desc.keepInitialState))
        {
            Report("CreateTexture", "InvalidArgument: shape, mip, samples, swizzle or initial state",
                   desc.debugName.c_str());
            return nullptr;
        }
        const bool depthFormat = GetFormatInfo(desc.format).hasDepth;
        const auto required = depthFormat ? FormatSupport::DepthStencil : FormatSupport::RenderTarget;
        if (!(caps.support & FormatSupport::Texture) ||
            (desc.isShaderResource && !(caps.support & FormatSupport::ShaderLoad)) ||
            (desc.isRenderTarget && !(caps.support & required)) || desc.isUAV || desc.isVirtual || desc.isTiled ||
            desc.isShadingRateSurface || desc.sharedResourceFlags != SharedResourceFlags::None)
        {
            Report("CreateTexture", "Unsupported: format, target or texture usage", desc.debugName.c_str());
            return nullptr;
        }
        try
        {
            auto texture = std::make_unique<OpenGLTexture>(this, desc);
            if (!BeginNative("CreateTexture", desc.debugName.c_str()))
            {
                return nullptr;
            }
            auto &name = texture->m_Deletion->name;
            glCreateTextures(target, 1, &name);
            if (!CheckNative("CreateTexture.Name", name, desc.debugName.c_str()))
            {
                return nullptr;
            }
            if (name == 0)
            {
                m_Phase = Phase::Faulted;
                Report("CreateTexture", "NativeFailure: zero object name", desc.debugName.c_str());
                return nullptr;
            }
            const auto width = GLsizei(desc.width);
            const auto height = GLsizei(desc.height);
            const auto levels = GLsizei(desc.mipLevels);
            switch (desc.dimension)
            {
            case TextureDimension::Texture1D:
                glTextureStorage1D(name, levels, caps.internalFormat, width);
                break;
            case TextureDimension::Texture1DArray:
                glTextureStorage2D(name, levels, caps.internalFormat, width, GLsizei(desc.arraySize));
                break;
            case TextureDimension::Texture2D:
            case TextureDimension::TextureCube:
                glTextureStorage2D(name, levels, caps.internalFormat, width, height);
                break;
            case TextureDimension::Texture2DArray:
            case TextureDimension::TextureCubeArray:
                glTextureStorage3D(name, levels, caps.internalFormat, width, height, GLsizei(desc.arraySize));
                break;
            case TextureDimension::Texture3D:
                glTextureStorage3D(name, levels, caps.internalFormat, width, height, GLsizei(desc.depth));
                break;
            case TextureDimension::Texture2DMS:
                glTextureStorage2DMultisample(name, GLsizei(desc.sampleCount), caps.internalFormat, width, height,
                                              GL_TRUE);
                break;
            case TextureDimension::Texture2DMSArray:
                glTextureStorage3DMultisample(name, GLsizei(desc.sampleCount), caps.internalFormat, width, height,
                                              GLsizei(desc.arraySize), GL_TRUE);
                break;
            default:
                std::terminate();
            }
            if (!CheckNative("CreateTexture.Storage", name, desc.debugName.c_str()))
            {
                return nullptr;
            }
            if (!multisample)
            {
                constexpr GLint swizzles[] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA, GL_ZERO, GL_ONE};
                GLint mapping[4]{};
                for (std::size_t index = 0; index < 4; ++index)
                {
                    mapping[index] = swizzles[std::size_t(components[index])];
                }
                glTextureParameteriv(name, GL_TEXTURE_SWIZZLE_RGBA, mapping);
                if (!CheckNative("CreateTexture.Components", name, desc.debugName.c_str()))
                {
                    return nullptr;
                }
            }
            texture->Publish();
            return TextureHandle::Create(texture.release());
        }
        catch (const std::bad_alloc &)
        {
            Report("CreateTexture", "CPU allocation failed", desc.debugName.c_str());
            return nullptr;
        }
    }
}
