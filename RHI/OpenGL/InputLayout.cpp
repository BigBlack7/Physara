#include "InputLayout.hpp"

/// @file
/// @brief 从有限 GL core 格式与真实顶点 limits 准备属性, 名称只保留诊断意义.

#include <algorithm>
#include <exception>
#include <limits>

namespace Physara::RHI::OpenGL
{
    bool VertexFormat(Format format, VertexInput &input) noexcept
    {
        GLenum storage = 0;
        switch (format)
        {
        case Format::R8_UINT:
        case Format::RG8_UINT:
        case Format::RGBA8_UINT:
        case Format::R8_UNORM:
        case Format::RG8_UNORM:
        case Format::RGBA8_UNORM:
            storage = GL_UNSIGNED_BYTE;
            break;
        case Format::R8_SINT:
        case Format::RG8_SINT:
        case Format::RGBA8_SINT:
        case Format::R8_SNORM:
        case Format::RG8_SNORM:
        case Format::RGBA8_SNORM:
            storage = GL_BYTE;
            break;
        case Format::R16_UINT:
        case Format::RG16_UINT:
        case Format::RGBA16_UINT:
        case Format::R16_UNORM:
        case Format::RG16_UNORM:
        case Format::RGBA16_UNORM:
            storage = GL_UNSIGNED_SHORT;
            break;
        case Format::R16_SINT:
        case Format::RG16_SINT:
        case Format::RGBA16_SINT:
        case Format::R16_SNORM:
        case Format::RG16_SNORM:
        case Format::RGBA16_SNORM:
            storage = GL_SHORT;
            break;
        case Format::R16_FLOAT:
        case Format::RG16_FLOAT:
        case Format::RGBA16_FLOAT:
            storage = GL_HALF_FLOAT;
            break;
        case Format::R32_UINT:
        case Format::RG32_UINT:
        case Format::RGB32_UINT:
        case Format::RGBA32_UINT:
            storage = GL_UNSIGNED_INT;
            break;
        case Format::R32_SINT:
        case Format::RG32_SINT:
        case Format::RGB32_SINT:
        case Format::RGBA32_SINT:
            storage = GL_INT;
            break;
        case Format::R32_FLOAT:
        case Format::RG32_FLOAT:
        case Format::RGB32_FLOAT:
        case Format::RGBA32_FLOAT:
            storage = GL_FLOAT;
            break;
        default:
            return false;
        }
        const auto &info = GetFormatInfo(format);
        input.scalar = info.kind == FormatKind::Integer ? (info.isSigned ? GL_INT : GL_UNSIGNED_INT) : GL_FLOAT;
        input.storage = storage;
        input.normalized = info.kind == FormatKind::Normalized;
        input.components = std::uint32_t(info.hasRed) + info.hasGreen + info.hasBlue + info.hasAlpha;
        // Draw 的 CPU 范围检查按存储宽度计算, 不能用转换后 Shader 标量的四字节宽度替代.
        input.byteSize = info.bytesPerBlock;
        return true;
    }

    OpenGLInputLayout::OpenGLInputLayout(OpenGLDevice *device, std::span<const VertexAttributeDesc> attributes)
        : m_Device(device), m_Attributes(attributes.begin(), attributes.end())
    {
        const auto &caps = device->GetCaps().shader;
        // 一个描述可展开多个连续 location, 但同一 bufferIndex 只能有一份 stride 和 divisor 语义.
        for (const auto &attribute : m_Attributes)
        {
            VertexInput input;
            RequirePreparation(VertexFormat(attribute.format, input), "Unsupported: vertex attribute format");
            RequirePreparation(attribute.arraySize > 0 &&
                                   attribute.arraySize <= MaxVertexAttributes - m_Inputs.size() &&
                                   m_Inputs.size() + attribute.arraySize <= std::size_t(caps.vertexAttributes) &&
                                   attribute.bufferIndex < MaxVertexAttributes &&
                                   attribute.bufferIndex < std::uint32_t(caps.vertexBindings) &&
                                   attribute.elementStride <= std::uint32_t(caps.vertexStride),
                               "InvalidArgument: vertex count/slot/stride");
            const auto bytes = GetFormatInfo(attribute.format).bytesPerBlock;
            // 先以宽类型证明末项 offset 可表示, 再在展开循环中执行受该证明保护的窄算术.
            const auto end = std::uint64_t(attribute.offset) + std::uint64_t(attribute.arraySize) * bytes;
            RequirePreparation(end <= std::numeric_limits<std::uint32_t>::max() &&
                                   end - bytes <= std::uint64_t(caps.vertexOffset),
                               "InvalidArgument: vertex attribute offset");
            for (const auto &prior : m_Inputs)
            {
                RequirePreparation(
                    prior.bufferIndex != attribute.bufferIndex ||
                        (prior.stride == attribute.elementStride && prior.instanced == attribute.isInstanced),
                    "InvalidArgument: shared vertex buffer stride/frequency mismatch");
            }
            input.bufferIndex = attribute.bufferIndex;
            input.stride = attribute.elementStride;
            input.instanced = attribute.isInstanced;
            for (std::uint32_t index = 0; index < attribute.arraySize; ++index)
            {
                input.location = static_cast<std::uint32_t>(m_Inputs.size());
                input.offset = attribute.offset + index * bytes;
                m_Inputs.push_back(input);
            }
        }
    }

    std::uint32_t OpenGLInputLayout::GetNumAttributes() const
    {
        return static_cast<std::uint32_t>(m_Attributes.size());
    }

    const VertexAttributeDesc *OpenGLInputLayout::GetAttributeDesc(std::uint32_t index) const
    {
        return index < m_Attributes.size() ? &m_Attributes[index] : nullptr;
    }

    InputLayoutHandle OpenGLDevice::CreateInputLayout(const VertexAttributeDesc *attributes, std::uint32_t count,
                                                      Shader *vertexShader)
    {
        if (!RequireContext("CreateInputLayout"))
        {
            return nullptr;
        }
        try
        {
            RequirePreparation((count == 0 || attributes) && count <= MaxVertexAttributes,
                               "InvalidArgument: vertex attribute storage/count");
            if (vertexShader)
            {
                // 公共参数在本后端不参与 location 分配; 只核对归属/阶段, 实际输入匹配留在 Pipeline.
                const auto *shader = dynamic_cast<OpenGLShader *>(vertexShader);
                RequirePreparation(shader && shader->Owner() == this &&
                                       shader->GetDesc().shaderType == ShaderType::Vertex,
                                   "InvalidResource: input-layout vertex shader");
            }
            return InputLayoutHandle::Create(new OpenGLInputLayout(this, {attributes, count}));
        }
        catch (const std::exception &error)
        {
            Report("CreateInputLayout", error.what());
            return nullptr;
        }
    }
} // namespace Physara::RHI::OpenGL
