#include "Backend.hpp"

#include <cmath>
#include <memory>
#include <new>
#include <utility>

namespace Physara::RHI::OpenGL
{
    OpenGLSampler::OpenGLSampler(OpenGLDevice *device, const SamplerDesc &desc)
        : m_Device(device), m_Desc(desc), m_Deletion(std::make_unique<NativeDeletion>(NativeKind::Sampler))
    {
    }

    OpenGLSampler::~OpenGLSampler()
    {
        m_Device->ReleaseAllocation(std::move(m_Deletion), m_Published);
    }

    SamplerHandle OpenGLDevice::CreateSampler(const SamplerDesc &desc)
    {
        if (!RequireContext("CreateSampler"))
        {
            return nullptr;
        }
        if (desc.addressU > SamplerAddressMode::MirrorOnce || desc.addressV > SamplerAddressMode::MirrorOnce ||
            desc.addressW > SamplerAddressMode::MirrorOnce || !std::isfinite(desc.mipBias) ||
            std::abs(desc.mipBias) > m_Caps.maxLodBias || !std::isfinite(desc.maxAnisotropy) ||
            desc.maxAnisotropy < 1 || desc.maxAnisotropy > m_Caps.maxAnisotropy || !std::isfinite(desc.borderColor.r) ||
            !std::isfinite(desc.borderColor.g) || !std::isfinite(desc.borderColor.b) ||
            !std::isfinite(desc.borderColor.a))
        {
            Report("CreateSampler", "InvalidArgument: address, border, bias or anisotropy");
            return nullptr;
        }
        if (desc.reductionType != SamplerReductionType::Standard &&
            desc.reductionType != SamplerReductionType::Comparison)
        {
            Report("CreateSampler", "Unsupported: sampler reduction");
            return nullptr;
        }
        try
        {
            auto sampler = std::make_unique<OpenGLSampler>(this, desc);
            if (!BeginNative("CreateSampler"))
            {
                return nullptr;
            }
            auto &name = sampler->m_Deletion->name;
            glCreateSamplers(1, &name);
            if (!CheckNative("CreateSampler.Name", name))
            {
                return nullptr;
            }
            if (name == 0)
            {
                m_Phase = Phase::Faulted;
                Report("CreateSampler", "NativeFailure: zero object name");
                return nullptr;
            }
            constexpr GLint addresses[] = {GL_CLAMP_TO_EDGE, GL_REPEAT, GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT,
                                           GL_MIRROR_CLAMP_TO_EDGE};
            constexpr GLint minFilters[2][2] = {{GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR},
                                                {GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR}};
            struct IntegerParameter
            {
                GLenum parameter;
                GLint value;
            };
            const IntegerParameter parameters[] = {
                {GL_TEXTURE_MIN_FILTER, minFilters[desc.minFilter][desc.mipFilter]},
                {GL_TEXTURE_MAG_FILTER, desc.magFilter ? GL_LINEAR : GL_NEAREST},
                {GL_TEXTURE_WRAP_S, addresses[std::size_t(desc.addressU)]},
                {GL_TEXTURE_WRAP_T, addresses[std::size_t(desc.addressV)]},
                {GL_TEXTURE_WRAP_R, addresses[std::size_t(desc.addressW)]},
                {GL_TEXTURE_COMPARE_MODE,
                 desc.reductionType == SamplerReductionType::Comparison ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE},
                // ShadowDepthConvention 使用常规深度, 与主视图 Reverse-Z 无关.
                {GL_TEXTURE_COMPARE_FUNC, GL_LESS}};
            for (const auto &parameter : parameters)
            {
                glSamplerParameteri(name, parameter.parameter, parameter.value);
                if (!CheckNative("CreateSampler.IntegerParameter", name))
                {
                    return nullptr;
                }
            }
            glSamplerParameterf(name, GL_TEXTURE_MAX_ANISOTROPY, desc.maxAnisotropy);
            if (!CheckNative("CreateSampler.Anisotropy", name))
            {
                return nullptr;
            }
            glSamplerParameterf(name, GL_TEXTURE_LOD_BIAS, desc.mipBias);
            if (!CheckNative("CreateSampler.LodBias", name))
            {
                return nullptr;
            }
            const GLfloat border[] = {desc.borderColor.r, desc.borderColor.g, desc.borderColor.b, desc.borderColor.a};
            glSamplerParameterfv(name, GL_TEXTURE_BORDER_COLOR, border);
            if (!CheckNative("CreateSampler.Border", name))
            {
                return nullptr;
            }
            sampler->Publish();
            return SamplerHandle::Create(sampler.release());
        }
        catch (const std::bad_alloc &)
        {
            Report("CreateSampler", "CPU allocation failed");
            return nullptr;
        }
    }
}
