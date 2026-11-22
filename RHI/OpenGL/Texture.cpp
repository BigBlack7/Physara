#include "Backend.hpp"

/// @file
/// @brief 按实际 target/format/样本能力创建纹理 immutable storage, initialState/clearValue 仅保存元数据.

#include <algorithm>
#include <bit>
#include <exception>
#include <memory>
#include <new>
#include <utility>

#include "../Common/Transfer.hpp"

namespace Physara::RHI::OpenGL
{
    void SetStorageImageFilters(GLuint name, Format format) noexcept
    {
        // image 单元使用纹理对象自身的过滤参数. 整数格式需要 nearest, 视图的参数也独立于原存储.
        if (GetFormatInfo(format).kind == FormatKind::Integer)
        {
            glTextureParameteri(name, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(name, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
    }

    OpenGLTexture::OpenGLTexture(OpenGLDevice *device, const TextureDesc &desc)
        : m_Device(device), m_Desc(desc), m_Deletion(std::make_unique<NativeDeletion>(NativeKind::Texture))
    {
        // image 视图共享原存储. mip 和数组层各自记录实际访问, 对普通纹理省去这份表.
        if (desc.isUAV)
        {
            m_ShaderAccess.resize(std::size_t(desc.mipLevels) * desc.arraySize);
        }
        // 联合采样需同时具备两种实际用途. 逐 mip 历史随底层存储保留, 新建对象的各 mip 初始无写入记录.
        if (desc.isRenderTarget && desc.isShaderResource &&
            (device->GetFormatCaps(desc.format, desc.dimension).support & FormatSupport::ReadOnlyDepthSampling) !=
                FormatSupport::None)
        {
            m_DepthWrites = std::make_unique<std::uint64_t[]>(desc.mipLevels);
        }
    }

    OpenGLTexture::~OpenGLTexture()
    {
        // 创建中途失败也经过这里. 每个已取得的名字有独立删除节点, 其余空节点可直接释放.
        if (m_CompressedLevels)
        {
            for (std::uint32_t mip = 1; mip < m_Desc.mipLevels; ++mip)
            {
                m_Device->ReleaseAllocation(std::move(m_CompressedLevels[mip - 1]), m_Published);
            }
        }
        m_Device->ReleaseAllocation(std::move(m_Deletion), m_Published);
    }

    // 本机非零 native mip 的压缩尾块 PBO 下载会重复块行. 每个逻辑级别用自己的实际尺寸和 native level 0.
    // 此 profile 只有二维 staging 传输, 所有 GL 名称由同一逻辑 Texture 和原物理包持有.
    bool OpenGLDevice::AllocateBC5Levels(OpenGLTexture &texture, GLenum internalFormat)
    {
        const auto &desc = texture.GetDesc();
        if (desc.mipLevels > 1)
        {
            texture.m_CompressedLevels = std::make_unique<std::unique_ptr<NativeDeletion>[]>(desc.mipLevels - 1);
            for (std::uint32_t mip = 1; mip < desc.mipLevels; ++mip)
            {
                texture.m_CompressedLevels[mip - 1] = std::make_unique<NativeDeletion>(NativeKind::Texture);
            }
        }
        if (!BeginNative("CreateTexture.BC5", desc.debugName.c_str()))
        {
            return false;
        }
        for (std::uint32_t mip = 0; mip < desc.mipLevels; ++mip)
        {
            auto &record = mip == 0 ? *texture.m_Deletion : *texture.m_CompressedLevels[mip - 1];
            glCreateTextures(GL_TEXTURE_2D, 1, &record.name);
            if (!CheckNative("CreateTexture.BC5Name", record.name, desc.debugName.c_str()))
            {
                return false;
            }
            if (record.name == 0)
            {
                Report("CreateTexture.BC5Name", "NativeFailure: zero compressed texture name", desc.debugName.c_str());
                return false;
            }
            glTextureStorage2D(record.name, 1, internalFormat, GLsizei(Transfer::MipSize(desc.width, mip)),
                               GLsizei(Transfer::MipSize(desc.height, mip)));
            if (!CheckNative("CreateTexture.BC5Storage", record.name, desc.debugName.c_str()))
            {
                return false;
            }
            ++m_CommandCounts.textureStorages;
        }
        return true;
    }

    bool OpenGLDevice::ValidTextureShape(const TextureDesc &desc) const noexcept
    {
        const bool cube =
            desc.dimension == TextureDimension::TextureCube || desc.dimension == TextureDimension::TextureCubeArray;
        const bool volume = desc.dimension == TextureDimension::Texture3D;
        // 空间尺寸按实际 target 选择上限; layer/face 不随 mip 缩小, 与空间 depth 分开校验.
        const auto spatialLimit = std::uint32_t(volume ? m_Caps.max3DTextureSize
                                                : cube ? m_Caps.maxCubeTextureSize
                                                       : m_Caps.maxTextureSize);
        bool shapeValid = desc.width > 0 && desc.width <= spatialLimit && desc.height > 0 &&
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
        return shapeValid && desc.mipLevels != 0 && desc.mipLevels <= maxMipLevels;
    }

    // 分别校验形状, 用途和设备能力, 取得存储后设置非 MS swizzle, 失败由未发布 wrapper 回滚.
    TextureHandle OpenGLDevice::CreateTexture(const TextureDesc &desc)
    {
        if (!RequireContext("CreateTexture"))
        {
            return nullptr;
        }
        const GLenum target = TextureTarget(desc.dimension);
        const bool multisample =
            desc.dimension == TextureDimension::Texture2DMS || desc.dimension == TextureDimension::Texture2DMSArray;
        const auto &caps = GetFormatCaps(desc.format, desc.dimension);
        // 描述保留完整 RHI 语义, 接受能力取当前实现与原生 target/format 观察的交集, 不自动降级格式或样本数.
        const ComponentSwizzle components[] = {desc.defaultComponentMapping.r, desc.defaultComponentMapping.g,
                                               desc.defaultComponentMapping.b, desc.defaultComponentMapping.a};
        bool swizzleValid = true;
        for (auto component : components)
        {
            swizzleValid = swizzleValid && component <= ComponentSwizzle::One;
        }
        if (!ValidTextureShape(desc) || !swizzleValid || desc.sampleQuality != 0 ||
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
            (desc.isRenderTarget && !(caps.support & required)) ||
            (desc.isUAV && (caps.support & FormatSupport::ShaderUavStore) == 0) || desc.isVirtual || desc.isTiled ||
            desc.isShadingRateSurface || desc.sharedResourceFlags != SharedResourceFlags::None)
        {
            Report("CreateTexture", "Unsupported: format, target or texture usage", desc.debugName.c_str());
            return nullptr;
        }
        try
        {
            // 描述副本与退休节点先取得, 未发布对象负责撤销失败前缀; 成功后也只拥有未初始化存储.
            auto texture = std::make_unique<OpenGLTexture>(this, desc);
            if (desc.format == Format::BC5_UNORM)
            {
                if (!AllocateBC5Levels(*texture, caps.internalFormat))
                {
                    return nullptr;
                }
                texture->Publish();
                return TextureHandle::Create(texture.release());
            }
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
            ++m_CommandCounts.textureStorages;
            if (!multisample)
            {
                // swizzle 改变未来采样的分量解释, 不变换存储字节; MS target 不接受这些普通纹理参数.
                constexpr GLint swizzles[] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA, GL_ZERO, GL_ONE};
                GLint mapping[4]{};
                for (std::size_t index = 0; index < 4; ++index)
                {
                    mapping[index] = swizzles[std::size_t(components[index])];
                }
                glTextureParameteriv(name, GL_TEXTURE_SWIZZLE_RGBA, mapping);
                if (desc.isUAV)
                {
                    SetStorageImageFilters(name, desc.format);
                }
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
