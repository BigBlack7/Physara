#include "Shader.hpp"

/// @file
/// @brief 在原始模块边界解析 profile/资源/IO, 不编译源文件, 不向 RHI 引入包格式或 schema.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

#include <spirv_cross.hpp>

namespace Physara::RHI::OpenGL
{
    void RequirePreparation(bool condition, const std::string &reason)
    {
        if (!condition)
        {
            throw std::runtime_error(reason);
        }
    }

    /// @brief 将当前 profile 的 32 位数值类型转为不依赖 SPIRV-Cross 的标量身份.
    static GLenum Scalar(const spirv_cross::SPIRType &type)
    {
        RequirePreparation(type.width == 32, "Unsupported: shader scalar width");
        switch (type.basetype)
        {
        case spirv_cross::SPIRType::Float:
            return GL_FLOAT;
        case spirv_cross::SPIRType::Int:
            return GL_INT;
        case spirv_cross::SPIRType::UInt:
            return GL_UNSIGNED_INT;
        default:
            throw std::runtime_error("Unsupported: shader scalar type");
        }
    }

    /// @brief 抽取字段布局签名, 只读 record 允许一层 struct 数组, 数据字段不允许递归嵌套.
    static void BlockLayout(const spirv_cross::Compiler &compiler, const spirv_cross::SPIRType &type,
                            std::vector<std::uint32_t> &signature)
    {
        RequirePreparation(type.basetype == spirv_cross::SPIRType::Struct && !type.member_types.empty() &&
                               compiler.get_declared_struct_size(type) <= 1024 * 1024,
                           "Unsupported: shader block shape or size");
        signature.push_back(static_cast<std::uint32_t>(type.member_types.size()));
        for (std::uint32_t index = 0; index < type.member_types.size(); ++index)
        {
            const auto &field = compiler.get_type(type.member_types[index]);
            const auto scalar = Scalar(field);
            RequirePreparation(field.vecsize >= 1 && field.vecsize <= 4 &&
                                   (field.columns == 1 || (scalar == GL_FLOAT && field.columns == field.vecsize &&
                                                           field.columns >= 2 && field.columns <= 4)) &&
                                   (field.array.empty() ||
                                    (field.array.size() == 1 && field.array_size_literal[0] && field.array[0] > 0)) &&
                                   !compiler.has_member_decoration(type.self, index, spv::DecorationRowMajor),
                               "Unsupported: shader block field shape");
            signature.insert(signature.end(),
                             {scalar, field.vecsize, field.columns, field.array.empty() ? 0u : field.array[0],
                              compiler.type_struct_member_offset(type, index),
                              static_cast<std::uint32_t>(compiler.get_declared_struct_member_size(type, index)),
                              field.array.empty() ? 0u : compiler.type_struct_member_array_stride(type, index),
                              field.columns == 1 ? 0u : compiler.type_struct_member_matrix_stride(type, index)});
        }
    }

    /// @brief 先拒绝模块级扩展, 再检查基线允许的内建变量和资源类别.
    static void CheckProfile(spirv_cross::Compiler &compiler, ShaderType stage)
    {
        for (auto capability : compiler.get_declared_capabilities())
        {
            RequirePreparation(capability == spv::CapabilityShader || capability == spv::CapabilityMatrix,
                               "Unsupported: canonical capability");
        }
        RequirePreparation(compiler.get_declared_extensions().empty() &&
                               compiler.get_specialization_constants().empty(),
                           "Unsupported: shader extension");
        compiler.get_execution_mode_bitset().for_each_bit([stage](std::uint32_t mode) {
            RequirePreparation(stage == ShaderType::Pixel && (mode == spv::ExecutionModeOriginUpperLeft ||
                                                              mode == spv::ExecutionModeDepthReplacing),
                               "Unsupported: shader execution mode");
        });
        const auto all = compiler.get_shader_resources();
        RequirePreparation(all.subpass_inputs.empty() && all.storage_images.empty() && all.sampled_images.empty() &&
                               all.atomic_counters.empty() && all.acceleration_structures.empty() &&
                               all.gl_plain_uniforms.empty() && all.push_constant_buffers.empty() &&
                               all.shader_record_buffers.empty() && all.tensors.empty(),
                           "Unsupported: resource class outside GraphicsBaselineV1");
        compiler.update_active_builtins();
        for (const auto &builtins : {all.builtin_inputs, all.builtin_outputs})
        {
            for (const auto &item : builtins)
            {
                if (!compiler.has_active_builtin(item.builtin, compiler.get_type(item.resource.type_id).storage))
                {
                    continue;
                }
                const auto value = item.builtin;
                const bool allowed = stage == ShaderType::Vertex
                                         ? value == spv::BuiltInPosition || value == spv::BuiltInPointSize ||
                                               value == spv::BuiltInVertexIndex || value == spv::BuiltInInstanceIndex
                                         : value == spv::BuiltInFragCoord || value == spv::BuiltInPointCoord ||
                                               value == spv::BuiltInFrontFacing || value == spv::BuiltInFragDepth;
                RequirePreparation(allowed, "Unsupported: shader builtin");
            }
        }
    }

    /// @brief 反射一项资源的数值布局, 逻辑 binding 不在此展开为 GL 单元.
    static ShaderResource ReflectResource(const spirv_cross::Compiler &compiler, const spirv_cross::Resource &item,
                                          ResourceType kind)
    {
        RequirePreparation(compiler.has_decoration(item.id, spv::DecorationDescriptorSet) &&
                               compiler.has_decoration(item.id, spv::DecorationBinding) &&
                               compiler.get_type(item.type_id).array.empty(),
                           "InvalidArgument: missing set/binding or descriptor array");
        ShaderResource result;
        result.id = item.id;
        result.type = kind;
        result.key = {compiler.get_decoration(item.id, spv::DecorationDescriptorSet),
                      compiler.get_decoration(item.id, spv::DecorationBinding)};
        RequirePreparation(result.key.set < 4, "Unsupported: logical set outside GraphicsBaselineV1");
        const auto &type = compiler.get_type(item.base_type_id);
        if (kind == ResourceType::ConstantBuffer)
        {
            BlockLayout(compiler, type, result.layout);
            result.extent = static_cast<std::uint32_t>(compiler.get_declared_struct_size(type));
        }
        else if (kind == ResourceType::StructuredBuffer_SRV)
        {
            RequirePreparation(compiler.get_buffer_block_flags(item.id).get(spv::DecorationNonWritable) &&
                                   type.member_types.size() == 1 && compiler.type_struct_member_offset(type, 0) == 0,
                               "Unsupported: storage block must be one readonly record array");
            const auto &records = compiler.get_type(type.member_types[0]);
            RequirePreparation(records.array.size() == 1 && records.array_size_literal[0] && records.array[0] > 0,
                               "Unsupported: storage records must have fixed capacity");
            BlockLayout(compiler, records, result.layout);
            const auto extent = compiler.get_declared_struct_size(type);
            RequirePreparation(extent <= 1024 * 1024, "Unsupported: storage block exceeds profile budget");
            result.extent = static_cast<std::uint32_t>(extent);
            result.recordStride = compiler.type_struct_member_array_stride(type, 0);
            result.layout.insert(result.layout.begin(), {records.array[0], result.recordStride});
        }
        else if (kind == ResourceType::Texture_SRV)
        {
            const auto &image = compiler.get_type(item.type_id);
            RequirePreparation(image.image.dim == spv::Dim2D && !image.image.arrayed && !image.image.ms &&
                                   !image.image.depth && image.image.sampled == 1 &&
                                   Scalar(compiler.get_type(image.image.type)) == GL_FLOAT,
                               "Unsupported: only separate float texture2D is accepted");
        }
        return result;
    }

    /// @brief 提取活动 IO 的 location/类型/插值, 不保留指向编译器临时结构的借用.
    static std::vector<ShaderIo> ReflectIo(const spirv_cross::Compiler &compiler,
                                           const spirv_cross::SmallVector<spirv_cross::Resource> &items,
                                           bool interStage)
    {
        std::vector<ShaderIo> result;
        for (const auto &item : items)
        {
            const auto &type = compiler.get_type(item.type_id);
            RequirePreparation(type.columns == 1 && type.array.empty() && type.vecsize >= 1 && type.vecsize <= 4 &&
                                   compiler.has_decoration(item.id, spv::DecorationLocation) &&
                                   !compiler.has_decoration(item.id, spv::DecorationComponent) &&
                                   !compiler.has_decoration(item.id, spv::DecorationIndex) &&
                                   !compiler.has_decoration(item.id, spv::DecorationCentroid) &&
                                   !compiler.has_decoration(item.id, spv::DecorationSample),
                               "Unsupported: shader IO shape/decoration");
            const auto interpolation = (compiler.has_decoration(item.id, spv::DecorationFlat) ? 1u : 0u) |
                                       (compiler.has_decoration(item.id, spv::DecorationNoPerspective) ? 2u : 0u);
            const auto scalar = Scalar(type);
            RequirePreparation(interpolation != 3 && (interStage || interpolation == 0) &&
                                   (!interStage || scalar == GL_FLOAT || interpolation == 1),
                               "InvalidArgument: IO interpolation");
            result.push_back(
                {compiler.get_decoration(item.id, spv::DecorationLocation), scalar, type.vecsize, interpolation});
        }
        std::ranges::sort(result, {}, &ShaderIo::location);
        RequirePreparation(std::adjacent_find(result.begin(), result.end(),
                                              [](auto &a, auto &b) { return a.location == b.location; }) ==
                               result.end(),
                           "InvalidArgument: duplicate IO location");
        return result;
    }

    OpenGLShader::OpenGLShader(OpenGLDevice *device, const ShaderDesc &desc, const void *bytes, std::size_t size)
        : m_Device(device), m_Desc(desc), m_Words(size / 4)
    {
        std::memcpy(m_Words.data(), bytes, size);
        RequirePreparation(m_Words[0] == 0x07230203 && m_Words[1] == 0x00010300 && m_Words[4] == 0,
                           "InvalidArgument: canonical SPIR-V 1.3 header");
        spirv_cross::Compiler compiler(m_Words);
        const auto entries = compiler.get_entry_points_and_stages();
        const auto model =
            desc.shaderType == ShaderType::Vertex ? spv::ExecutionModelVertex : spv::ExecutionModelFragment;
        RequirePreparation(entries.size() == 1 && entries[0].name == desc.entryName &&
                               entries[0].execution_model == model,
                           "InvalidArgument: shader entry/stage mismatch");
        compiler.set_entry_point(desc.entryName, model);
        CheckProfile(compiler, desc.shaderType);
        // 只记录活动 builtin 的事实, 原生点大小开关由实际执行该 VS 的 ContextState 建立.
        m_WritesPointSize = compiler.has_active_builtin(spv::BuiltInPointSize, spv::StorageClassOutput);
        // 全部声明都须满足 profile, 但只有当前入口活动的资源进入 Pipeline 的限额预算.
        const auto activeVariables = compiler.get_active_interface_variables();
        const auto active = compiler.get_shader_resources(activeVariables);
        const auto all = compiler.get_shader_resources();
        const auto append = [&](const auto &items, ResourceType kind) {
            for (const auto &item : items)
            {
                auto resource = ReflectResource(compiler, item, kind);
                if (activeVariables.contains(item.id))
                {
                    m_Resources.push_back(std::move(resource));
                }
            }
        };
        append(all.uniform_buffers, ResourceType::ConstantBuffer);
        append(all.storage_buffers, ResourceType::StructuredBuffer_SRV);
        append(all.separate_images, ResourceType::Texture_SRV);
        append(all.separate_samplers, ResourceType::Sampler);
        std::ranges::sort(m_Resources, {}, &ShaderResource::key);
        RequirePreparation(std::adjacent_find(m_Resources.begin(), m_Resources.end(),
                                              [](const auto &a, const auto &b) { return a.key == b.key; }) ==
                               m_Resources.end(),
                           "InvalidArgument: duplicate descriptor binding");
        // 顶点输入/片元输出只表达数值 location, 插值限定属于 VS 输出到 FS 输入这一条边.
        m_Inputs = ReflectIo(compiler, active.stage_inputs, desc.shaderType == ShaderType::Pixel);
        m_Outputs = ReflectIo(compiler, active.stage_outputs, desc.shaderType == ShaderType::Vertex);
        // dummy 分析会改变活动集合, 故资源和 IO 先保存, pair 的对象身份仍来自原始 IR.
        const auto dummy = compiler.build_dummy_sampler_for_combined_images();
        compiler.build_combined_image_samplers();
        const auto key = [&](std::uint32_t id) -> ShaderBindingKey {
            return {compiler.get_decoration(id, spv::DecorationDescriptorSet),
                    compiler.get_decoration(id, spv::DecorationBinding)};
        };
        for (const auto &pair : compiler.get_combined_image_samplers())
        {
            m_Pairs.push_back({key(pair.image_id), pair.sampler_id == dummy ? ShaderBindingKey{} : key(pair.sampler_id),
                               pair.sampler_id == dummy});
        }
        std::ranges::sort(m_Pairs);
        m_Pairs.erase(std::unique(m_Pairs.begin(), m_Pairs.end()), m_Pairs.end());
    }

    void OpenGLShader::GetBytecode(const void **bytes, std::size_t *size) const
    {
        if (bytes)
        {
            *bytes = m_Words.data();
        }
        if (size)
        {
            *size = m_Words.size() * sizeof(std::uint32_t);
        }
    }

    ShaderHandle OpenGLDevice::CreateShader(const ShaderDesc &desc, const void *bytes, std::size_t size)
    {
        if (!RequireContext("CreateShader"))
        {
            return nullptr;
        }
        char context[256]{};
        std::snprintf(context, sizeof(context), "stage=%u entry=%.32s source=%.160s",
                      static_cast<unsigned>(desc.shaderType), desc.entryName.c_str(), desc.debugName.c_str());
        if (!bytes || size < 20 || size % 4 != 0 || size > 64 * 1024 * 1024 || desc.entryName != "main" ||
            (desc.shaderType != ShaderType::Vertex && desc.shaderType != ShaderType::Pixel) ||
            desc.hlslExtensionsUAV >= 0 || desc.useSpecificShaderExt || desc.numCustomSemantics != 0 ||
            desc.pCustomSemantics || desc.fastGSFlags != FastGeometryShaderFlags(0) || desc.pCoordinateSwizzling)
        {
            Report("CreateShader", "InvalidArgument or Unsupported: GraphicsBaselineV1 module/descriptor", context);
            return nullptr;
        }
        // 先以字节长度保护头部读取, 再复制到对齐的 words; 调用方地址无需具有 uint32_t 对齐.
        try
        {
            return ShaderHandle::Create(new OpenGLShader(this, desc, bytes, size));
        }
        catch (const std::exception &error)
        {
            Report("CreateShader", error.what(), context);
            return nullptr;
        }
    }
} // namespace Physara::RHI::OpenGL
