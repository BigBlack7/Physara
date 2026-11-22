#include "Backend.hpp"

/// @file
/// @brief 统一格式/target/上传映射并查询实际 caps, 格式元数据, 分配用途与命令支持分别判断.

#include <cmath>
#include <vector>

#include "../Common/Transfer.hpp"

namespace Physara::RHI::OpenGL
{
    // 分配 internal format 与颜色上传解释共享一表, 无 external format 的格式不允许 CPU 上传.
    struct FormatMapping
    {
        Format format;
        GLenum internalFormat;
        GLenum uploadFormat = 0;
        GLenum uploadType = 0;
    };

    static constexpr FormatMapping FormatMappings[] = {
        {Format::R8_UNORM, GL_R8, GL_RED, GL_UNSIGNED_BYTE},
        {Format::R8_UINT, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE},
        {Format::RG8_UNORM, GL_RG8, GL_RG, GL_UNSIGNED_BYTE},
        {Format::RGBA8_UNORM, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE},
        {Format::RGBA8_UINT, GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE},
        {Format::SRGBA8_UNORM, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE},
        {Format::R16_FLOAT, GL_R16F, GL_RED, GL_HALF_FLOAT},
        {Format::RG16_FLOAT, GL_RG16F, GL_RG, GL_HALF_FLOAT},
        {Format::RGBA16_FLOAT, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT},
        {Format::R32_UINT, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT},
        {Format::R32_FLOAT, GL_R32F, GL_RED, GL_FLOAT},
        {Format::RG32_FLOAT, GL_RG32F, GL_RG, GL_FLOAT},
        {Format::RGBA32_FLOAT, GL_RGBA32F, GL_RGBA, GL_FLOAT},
        {Format::D16, GL_DEPTH_COMPONENT16},
        {Format::D24S8, GL_DEPTH24_STENCIL8},
        {Format::D32, GL_DEPTH_COMPONENT32F},
        {Format::D32S8, GL_DEPTH32F_STENCIL8},
        {Format::BC5_UNORM, GL_COMPRESSED_RG_RGTC2}};

    bool GetUploadFormat(Format format, GLenum &externalFormat, GLenum &externalType) noexcept
    {
        for (const auto &mapping : FormatMappings)
        {
            if (mapping.format == format && mapping.uploadFormat != 0)
            {
                externalFormat = mapping.uploadFormat;
                externalType = mapping.uploadType;
                return true;
            }
        }
        return false;
    }

    GLenum TextureTarget(TextureDimension dimension) noexcept
    {
        switch (dimension)
        {
        case TextureDimension::Texture1D:
            return GL_TEXTURE_1D;
        case TextureDimension::Texture1DArray:
            return GL_TEXTURE_1D_ARRAY;
        case TextureDimension::Texture2D:
            return GL_TEXTURE_2D;
        case TextureDimension::Texture2DArray:
            return GL_TEXTURE_2D_ARRAY;
        case TextureDimension::TextureCube:
            return GL_TEXTURE_CUBE_MAP;
        case TextureDimension::TextureCubeArray:
            return GL_TEXTURE_CUBE_MAP_ARRAY;
        case TextureDimension::Texture2DMS:
            return GL_TEXTURE_2D_MULTISAMPLE;
        case TextureDimension::Texture2DMSArray:
            return GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
        case TextureDimension::Texture3D:
            return GL_TEXTURE_3D;
        default:
            return 0;
        }
    }

    // 对实际 target 求项目 profile 与驱动观察的交集, 多样本使用离散集合, 不由 maxSamples 推测.
    static bool ReadFormat(OpenGLDevice &device, const DeviceCaps &caps, const FormatMapping &mapping,
                           TextureDimension dimension, FormatCaps &result)
    {
        const auto &format = GetFormatInfo(mapping.format);
        // GL 4.6 8.5 排除 3D 深度/模板存储; 不能由驱动的乐观 query 放行非法 target.
        if ((dimension == TextureDimension::Texture3D && format.hasDepth) ||
            (format.blockSize > 1 && dimension != TextureDimension::Texture2D))
        {
            return true;
        }
        const GLenum target = TextureTarget(dimension);
        const auto query = [&](GLenum property)
        {
            GLint value = 0;
            glGetInternalformativ(target, mapping.internalFormat, property, 1, &value);
            return value;
        };
        const GLint supported = query(GL_INTERNALFORMAT_SUPPORTED);
        const GLint renderable = query(GL_FRAMEBUFFER_RENDERABLE);
        const GLint blend = query(GL_FRAMEBUFFER_BLEND);
        const GLint color = query(GL_COLOR_RENDERABLE);
        const GLint depth = query(GL_DEPTH_RENDERABLE);
        const GLint stencil = query(GL_STENCIL_RENDERABLE);
        const GLint vertex = query(GL_VERTEX_TEXTURE);
        const GLint fragment = query(GL_FRAGMENT_TEXTURE);
        const bool filterable = format.kind != FormatKind::Integer && !format.hasDepth &&
                                Transfer::Planar(dimension) && query(GL_FILTER) == GL_FULL_SUPPORT;
        const bool sampledCube = dimension != TextureDimension::TextureCube || filterable;
        const GLint viewClass = query(GL_VIEW_COMPATIBILITY_CLASS);
        if (!device.CheckNative("CreateDevice.FormatQueries"))
        {
            return false;
        }
        if (supported != GL_TRUE)
        {
            return true;
        }
        result.internalFormat = mapping.internalFormat;
        result.viewClass = viewClass;
        result.filterable = filterable;
        result.support = FormatSupport::Texture;
        if (format.blockSize > 1)
        {
            // RGTC 首发仅交付二维存储与 staging, 保留独立的 Shader/图能力门槛.
            result.sampleMask = 1u << 1;
            return true;
        }
        // 仅将有限 storage profile 与此 target 的实际 image 能力相交. 读与写的查询结果分别记录.
        if (glDispatchCompute && glMemoryBarrier && glBindImageTexture && glGetIntegeri_v &&
            (dimension == TextureDimension::Texture2D || dimension == TextureDimension::Texture2DArray) &&
            (mapping.format == Format::R32_FLOAT || mapping.format == Format::R32_UINT ||
             mapping.format == Format::RGBA8_UNORM || mapping.format == Format::RGBA16_FLOAT))
        {
            if (query(GL_SHADER_IMAGE_LOAD) == GL_FULL_SUPPORT)
            {
                result.support = result.support | FormatSupport::ShaderUavLoad;
            }
            if (query(GL_SHADER_IMAGE_STORE) == GL_FULL_SUPPORT)
            {
                result.support = result.support | FormatSupport::ShaderUavStore;
            }
            if (!device.CheckNative("CreateDevice.ImageFormats"))
            {
                return false;
            }
        }
        if (renderable == GL_FULL_SUPPORT)
        {
            if (color == GL_TRUE)
            {
                result.support = result.support | FormatSupport::RenderTarget;
                if (blend == GL_FULL_SUPPORT)
                {
                    result.support = result.support | FormatSupport::Blendable;
                }
            }
            if (depth == GL_TRUE && (!format.hasStencil || stencil == GL_TRUE))
            {
                result.support = result.support | FormatSupport::DepthStencil;
            }
        }
        const bool multisample =
            dimension == TextureDimension::Texture2DMS || dimension == TextureDimension::Texture2DMSArray;
        if (vertex == GL_FULL_SUPPORT && fragment == GL_FULL_SUPPORT)
        {
            // ShaderLoad 仍描述目标的实际读取能力. ShaderSample 对单 cube 仅交付可线性过滤的颜色视图.
            result.support = result.support | FormatSupport::ShaderLoad;
            if (!multisample && sampledCube && format.kind != FormatKind::Integer)
            {
                result.support = result.support | FormatSupport::ShaderSample;
            }
        }
        if (!multisample)
        {
            result.sampleMask = 1u << 1;
            const auto required = FormatSupport::DepthStencil | FormatSupport::ShaderSample;
            if (dimension == TextureDimension::Texture2D && (result.support & required) == required &&
                glTextureBarrier != nullptr)
            {
                // GL 4.6 22.3.1 为采样和测试的组合提供独立查询. Packed 格式还要求同时进行模板测试的能力.
                const bool depthTest = query(GL_SIMULTANEOUS_TEXTURE_AND_DEPTH_TEST) == GL_FULL_SUPPORT;
                const bool stencilTest = !format.hasStencil ||
                                         query(GL_SIMULTANEOUS_TEXTURE_AND_STENCIL_TEST) == GL_FULL_SUPPORT;
                if (!device.CheckNative("CreateDevice.ReadOnlyDepthSampling"))
                {
                    return false;
                }
                if (depthTest && stencilTest)
                {
                    result.support = result.support | FormatSupport::ReadOnlyDepthSampling;
                }
            }
            return true;
        }
        const GLint count = query(GL_NUM_SAMPLE_COUNTS);
        if (!device.CheckNative("CreateDevice.SampleCount") || count < 0 || count > caps.maxSamples)
        {
            device.Report("CreateDevice", "NativeFailure: invalid sample-set length");
            return false;
        }
        std::vector<GLint> samples(std::size_t(count), 0);
        if (count != 0)
        {
            glGetInternalformativ(target, mapping.internalFormat, GL_SAMPLES, count, samples.data());
            if (!device.CheckNative("CreateDevice.Samples"))
            {
                return false;
            }
        }
        for (GLint sample : samples)
        {
            if (sample <= 0 || sample > caps.maxSamples)
            {
                device.Report("CreateDevice", "NativeFailure: invalid sample-set element");
                return false;
            }
            if (sample == 2 || sample == 4 || sample == 8)
            {
                result.sampleMask |= 1u << sample;
            }
        }
        return true;
    }

    bool BuildCaps(OpenGLDevice &device, DeviceCaps &caps)
    {
        struct Limit
        {
            GLenum name;
            GLint *value;
        };
        const Limit limits[] = {{GL_MAX_TEXTURE_SIZE, &caps.maxTextureSize},
                                {GL_MAX_3D_TEXTURE_SIZE, &caps.max3DTextureSize},
                                {GL_MAX_CUBE_MAP_TEXTURE_SIZE, &caps.maxCubeTextureSize},
                                {GL_MAX_ARRAY_TEXTURE_LAYERS, &caps.maxArrayLayers},
                                {GL_MAX_COLOR_ATTACHMENTS, &caps.maxColorAttachments},
                                {GL_MAX_DRAW_BUFFERS, &caps.maxDrawBuffers},
                                {GL_MAX_SAMPLES, &caps.maxSamples}};
        for (const auto &limit : limits)
        {
            glGetIntegerv(limit.name, limit.value);
        }
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &caps.maxAnisotropy);
        glGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &caps.maxLodBias);
        if (!device.CheckNative("CreateDevice.Limits"))
        {
            return false;
        }
        for (const auto &limit : limits)
        {
            if (*limit.value <= 0)
            {
                device.Report("CreateDevice", "UnsupportedEnvironment: invalid native limit");
                return false;
            }
        }
        if (!std::isfinite(caps.maxAnisotropy) || caps.maxAnisotropy < 1 || !std::isfinite(caps.maxLodBias) ||
            caps.maxLodBias < 0)
        {
            device.Report("CreateDevice", "UnsupportedEnvironment: invalid sampler limits");
            return false;
        }
        for (const auto &mapping : FormatMappings)
        {
            for (std::size_t index = 1; index < TextureDimensionCount; ++index)
            {
                if (!ReadFormat(device, caps, mapping, TextureDimension(index),
                                caps.formats[std::size_t(mapping.format)][index]))
                {
                    return false;
                }
            }
        }
        // Device 发布前确认项目必需的 2D 基线. 其他 target 的独立结果仍留在 caps, 创建时继续按实际维度校验.
        for (const auto format :
             {Format::RGBA8_UNORM, Format::SRGBA8_UNORM, Format::RGBA16_FLOAT, Format::R32_UINT, Format::D32})
        {
            const auto &entry = caps.formats[std::size_t(format)][std::size_t(TextureDimension::Texture2D)];
            const auto required = FormatSupport::Texture |
                                  (format == Format::D32 ? FormatSupport::DepthStencil : FormatSupport::RenderTarget);
            if ((std::uint32_t(entry.support) & std::uint32_t(required)) != std::uint32_t(required) ||
                !(entry.support & (FormatSupport::ShaderLoad | FormatSupport::ShaderSample)))
            {
                device.Report("CreateDevice", "UnsupportedEnvironment: required baseline format usage",
                              GetFormatInfo(format).name);
                return false;
            }
        }
        return true;
    }

    // 可选传输入口由对应操作的能力控制. Resolve 同时需要源与匹配目标的真实颜色附件能力.
    TransferFormatSupport OpenGLDevice::GetTransferFormatSupport(Format format,
                                                                  TextureDimension dimension) const noexcept
    {
        const auto &caps = GetFormatCaps(format, dimension);
        TransferFormatSupport result;
        if (!(caps.support & FormatSupport::Texture))
        {
            return result;
        }
        const auto &info = GetFormatInfo(format);
        const bool colorAttachment = (caps.support & FormatSupport::RenderTarget) != FormatSupport::None;
        const bool resolveColor = format == Format::RGBA8_UNORM || format == Format::RGBA16_FLOAT;
        if (Transfer::Multisample(dimension))
        {
            const auto dest = dimension == TextureDimension::Texture2DMS ? TextureDimension::Texture2D
                                                                        : TextureDimension::Texture2DArray;
            if (resolveColor && colorAttachment &&
                (GetFormatCaps(format, dest).support & FormatSupport::RenderTarget) != FormatSupport::None)
            {
                result.resolveSourceSamples = caps.sampleMask;
                result.colorClearSamples = caps.sampleMask;
            }
            return result;
        }
        if (!Transfer::Planar(dimension))
        {
            return result;
        }
        result.colorClearSamples = colorAttachment ? caps.sampleMask : 0;
        result.depthStencilClear = (caps.support & FormatSupport::DepthStencil) != FormatSupport::None;
        // copy 表示普通纹理之间的复制. 压缩格式通过下方的 staging 标志单独公布整级块传输.
        result.copy = Transfer::OrdinaryCopyFormat(info);
        result.mipmaps = colorAttachment && caps.filterable && Transfer::ColorMipFormat(format);
        result.resolveDestination = resolveColor && colorAttachment &&
                                    (dimension == TextureDimension::Texture2D || dimension == TextureDimension::Texture2DArray);
        if (format == Format::BC5_UNORM)
        {
            result.stagingRead = glGetCompressedTextureImage != nullptr;
            result.stagingWrite = glCompressedTextureSubImage2D != nullptr;
        }
        else if (resolveColor || format == Format::SRGBA8_UNORM || format == Format::R8_UNORM ||
                 format == Format::R32_UINT)
        {
            result.stagingRead = glGetTextureSubImage != nullptr;
            result.stagingWrite = true; // 设备启动已核对对应的普通 DSA subimage 入口.
        }
        return result;
    }

    TransferFormatSupport QueryTransferFormatSupport(Device *device, Format format, TextureDimension dimension) noexcept
    {
        const auto *native = dynamic_cast<OpenGLDevice *>(device);
        return native ? native->GetTransferFormatSupport(format, dimension) : TransferFormatSupport{};
    }
}
