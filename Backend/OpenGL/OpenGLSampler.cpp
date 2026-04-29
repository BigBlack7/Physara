#include "OpenGLSampler.hpp"

#include <algorithm>
#include <cassert>

#include <Backend/OpenGL/OpenGLTypeMapping.hpp>

namespace Physara::RHI
{
    namespace Internal
    {
        static GLenum ToGLMagFilter(FilterMode mode)
        {
            return (mode == FilterMode::Nearest) ? GL_NEAREST : GL_LINEAR;
        }

        static GLenum ToGLMinFilter(FilterMode minFilter, FilterMode mipFilter)
        {
            const bool minLinear = (minFilter != FilterMode::Nearest);
            const bool mipLinear = (mipFilter != FilterMode::Nearest);

            if (minLinear && mipLinear)
            {
                return GL_LINEAR_MIPMAP_LINEAR;
            }

            if (minLinear && !mipLinear)
            {
                return GL_LINEAR_MIPMAP_NEAREST;
            }

            if (!minLinear && mipLinear)
            {
                return GL_NEAREST_MIPMAP_LINEAR;
            }
            return GL_NEAREST_MIPMAP_NEAREST;
        }
    }

    OpenGLSampler::OpenGLSampler(const RHISamplerDesc &desc) : m_Desc(desc)
    {
        glCreateSamplers(1, &m_ID);

        // 过滤模式设置(Min/Mag Filter)
        glSamplerParameteri(m_ID, GL_TEXTURE_MIN_FILTER, Internal::ToGLMinFilter(m_Desc.minFilter, m_Desc.mipFilter));
        glSamplerParameteri(m_ID, GL_TEXTURE_MAG_FILTER, Internal::ToGLMagFilter(m_Desc.magFilter));

        // 纹理环绕模式设置UVW
        glSamplerParameteri(m_ID, GL_TEXTURE_WRAP_S, ToGLWrap(m_Desc.wrapU));
        glSamplerParameteri(m_ID, GL_TEXTURE_WRAP_T, ToGLWrap(m_Desc.wrapV));
        glSamplerParameteri(m_ID, GL_TEXTURE_WRAP_R, ToGLWrap(m_Desc.wrapW));

        // LOD(多级纹理)参数设置
        glSamplerParameterf(m_ID, GL_TEXTURE_LOD_BIAS, m_Desc.lodBias);
        glSamplerParameterf(m_ID, GL_TEXTURE_MIN_LOD, m_Desc.minLod);
        glSamplerParameterf(m_ID, GL_TEXTURE_MAX_LOD, m_Desc.maxLod);

        // 阴影比较模式设置
        if (m_Desc.compareOp != CompareOp::None)
        {
            glSamplerParameteri(m_ID, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
            glSamplerParameteri(m_ID, GL_TEXTURE_COMPARE_FUNC, ToGLCompareFunc(m_Desc.compareOp));
        }

        // 各向异性过滤设置
#ifdef GL_TEXTURE_MAX_ANISOTROPY
        const float aniso = std::max(1.f, m_Desc.anisotropy);
        if (aniso > 1.f)
        {
            glSamplerParameterf(m_ID, GL_TEXTURE_MAX_ANISOTROPY, aniso);
        }
#endif
    }

    OpenGLSampler::~OpenGLSampler()
    {
        if (m_ID != 0)
        {
            glDeleteSamplers(1, &m_ID);
            m_ID = 0;
        }
    }
}