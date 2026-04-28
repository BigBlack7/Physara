#pragma once

#include <cassert>
#include <cstdint>

#include <glad/glad.h>

#include <Engine/RHI/RHIDefinitions.hpp>

namespace Physara::RHI
{
    struct GLTextureFormat
    {
        GLenum internalFormat;
        GLenum baseFormat;
        GLenum type;
    };

    [[nodiscard]] constexpr bool IsCompressedTextureFormat(TextureFormat format)
    {
        return format == TextureFormat::BC6H_UF16 || format == TextureFormat::BC7_UNORM;
    }

    [[nodiscard]] constexpr GLTextureFormat ToGLTextureFormat(TextureFormat format)
    {
        if (format == TextureFormat::RGBA8)
        {
            return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
        }
        if (format == TextureFormat::RGBA16F)
        {
            return {GL_RGBA16F, GL_RGBA, GL_FLOAT};
        }
        if (format == TextureFormat::RGBA32F)
        {
            return {GL_RGBA32F, GL_RGBA, GL_FLOAT};
        }
        if (format == TextureFormat::RG16F)
        {
            return {GL_RG16F, GL_RG, GL_FLOAT};
        }
        if (format == TextureFormat::R8)
        {
            return {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
        }
        if (format == TextureFormat::Depth24Stencil8)
        {
            return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};
        }
        if (format == TextureFormat::Depth32F)
        {
            return {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT};
        }

        if (format == TextureFormat::BC6H_UF16)
        {
            return {GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT, 0u, 0u};
        }
        if (format == TextureFormat::BC7_UNORM)
        {
            return {GL_COMPRESSED_RGBA_BPTC_UNORM, 0u, 0u};
        }

        assert(false && "Invalid TextureFormat");
        return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    }

    [[nodiscard]] constexpr GLenum ToGLFilter(FilterMode mode)
    {
        if (mode == FilterMode::Nearest)
        {
            return GL_NEAREST;
        }
        if (mode == FilterMode::Linear)
        {
            return GL_LINEAR;
        }
        if (mode == FilterMode::LinearMipmapLinear)
        {
            return GL_LINEAR_MIPMAP_LINEAR;
        }

        assert(false && "Invalid FilterMode");
        return GL_LINEAR;
    }

    [[nodiscard]] constexpr GLenum ToGLWrap(WrapMode mode)
    {
        if (mode == WrapMode::Repeat)
        {
            return GL_REPEAT;
        }
        if (mode == WrapMode::ClampToEdge)
        {
            return GL_CLAMP_TO_EDGE;
        }
        if (mode == WrapMode::ClampToBorder)
        {
            return GL_CLAMP_TO_BORDER;
        }
        if (mode == WrapMode::MirroredRepeat)
        {
            return GL_MIRRORED_REPEAT;
        }

        assert(false && "Invalid WrapMode");
        return GL_REPEAT;
    }

    [[nodiscard]] constexpr GLenum ToGLCompareFunc(CompareOp op)
    {
        if (op == CompareOp::None)
        {
            return GL_LESS;
        }
        if (op == CompareOp::Less)
        {
            return GL_LESS;
        }
        if (op == CompareOp::LessEqual)
        {
            return GL_LEQUAL;
        }
        if (op == CompareOp::Greater)
        {
            return GL_GREATER;
        }

        assert(false && "Invalid CompareOp");
        return GL_LESS;
    }

    [[nodiscard]] constexpr GLenum ToGLBufferUsage(bool dynamic)
    {
        return dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    }

    [[nodiscard]] constexpr GLenum ToGLCullMode(CullMode mode)
    {
        if (mode == CullMode::Front)
        {
            return GL_FRONT;
        }
        if (mode == CullMode::Back)
        {
            return GL_BACK;
        }
        if (mode == CullMode::None)
        {
            return GL_BACK;
        }

        assert(false && "Invalid CullMode");
        return GL_BACK;
    }

    [[nodiscard]] constexpr GLenum ToGLPolygonMode(PolygonMode mode)
    {
        if (mode == PolygonMode::Fill)
        {
            return GL_FILL;
        }
        if (mode == PolygonMode::Line)
        {
            return GL_LINE;
        }

        assert(false && "Invalid PolygonMode");
        return GL_FILL;
    }

    [[nodiscard]] constexpr GLenum ToGLDepthFunc(DepthCompareOp op)
    {
        if (op == DepthCompareOp::Less)
        {
            return GL_LESS;
        }
        if (op == DepthCompareOp::LessEqual)
        {
            return GL_LEQUAL;
        }
        if (op == DepthCompareOp::Equal)
        {
            return GL_EQUAL;
        }
        if (op == DepthCompareOp::Always)
        {
            return GL_ALWAYS;
        }

        assert(false && "Invalid DepthCompareOp");
        return GL_LESS;
    }

    [[nodiscard]] constexpr GLenum ToGLBlendFactor(BlendFactor factor)
    {
        if (factor == BlendFactor::Zero)
        {
            return GL_ZERO;
        }
        if (factor == BlendFactor::One)
        {
            return GL_ONE;
        }
        if (factor == BlendFactor::SrcColor)
        {
            return GL_SRC_COLOR;
        }
        if (factor == BlendFactor::OneMinusSrcColor)
        {
            return GL_ONE_MINUS_SRC_COLOR;
        }
        if (factor == BlendFactor::DstColor)
        {
            return GL_DST_COLOR;
        }
        if (factor == BlendFactor::OneMinusDstColor)
        {
            return GL_ONE_MINUS_DST_COLOR;
        }
        if (factor == BlendFactor::SrcAlpha)
        {
            return GL_SRC_ALPHA;
        }
        if (factor == BlendFactor::OneMinusSrcAlpha)
        {
            return GL_ONE_MINUS_SRC_ALPHA;
        }
        if (factor == BlendFactor::DstAlpha)
        {
            return GL_DST_ALPHA;
        }
        if (factor == BlendFactor::OneMinusDstAlpha)
        {
            return GL_ONE_MINUS_DST_ALPHA;
        }

        assert(false && "Invalid BlendFactor");
        return GL_ONE;
    }

    [[nodiscard]] constexpr GLenum ToGLBlendOp(BlendOp op)
    {
        if (op == BlendOp::Add)
        {
            return GL_FUNC_ADD;
        }
        if (op == BlendOp::Subtract)
        {
            return GL_FUNC_SUBTRACT;
        }
        if (op == BlendOp::ReverseSubtract)
        {
            return GL_FUNC_REVERSE_SUBTRACT;
        }

        assert(false && "Invalid BlendOp");
        return GL_FUNC_ADD;
    }

    [[nodiscard]] constexpr GLenum ToGLTopology(PrimitiveTopology topo)
    {
        if (topo == PrimitiveTopology::Triangles)
        {
            return GL_TRIANGLES;
        }
        if (topo == PrimitiveTopology::Lines)
        {
            return GL_LINES;
        }
        if (topo == PrimitiveTopology::Points)
        {
            return GL_POINTS;
        }

        assert(false && "Invalid PrimitiveTopology");
        return GL_TRIANGLES;
    }

    [[nodiscard]] constexpr GLenum ToGLShaderStage(ShaderStage stage)
    {
        if (stage == ShaderStage::Vertex)
        {
            return GL_VERTEX_SHADER;
        }
        if (stage == ShaderStage::Fragment)
        {
            return GL_FRAGMENT_SHADER;
        }
        if (stage == ShaderStage::Compute)
        {
            return GL_COMPUTE_SHADER;
        }

        assert(false && "Invalid ShaderStage");
        return GL_VERTEX_SHADER;
    }

    [[nodiscard]] constexpr GLenum ToGLAttachmentPoint(std::uint32_t colorIndex)
    {
        if (colorIndex >= 32u)
        {
            assert(false && "Color attachment index out of range");
            return GL_COLOR_ATTACHMENT0;
        }

        return static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + colorIndex);
    }
}