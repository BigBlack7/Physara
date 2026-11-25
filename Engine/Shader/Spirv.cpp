#include "Spirv.hpp"

/// @file
/// @brief 用 SPIRV-Tools 验证最终字节码, 再由 SPIRV-Cross 对 profile, 布局和活动接口逐层核对.

#include <algorithm>
#include <cstdio>
#include <set>
#include <tuple>

#include <spirv-tools/libspirv.hpp>
#include <spirv_cross.hpp>

namespace Physara::Shader
{
    void ValidateSpirv(std::span<const std::uint32_t> words)
    {
        if (words.size() < 5 || words.size() > MaxPackageBytes / 4 || words[0] != 0x07230203 ||
            words[1] != 0x00010300 || words[4] != 0)
        {
            throw BuildError(ErrorCode::ValidationFailed, "Invalid canonical SPIR-V header/size/version.");
        }
        spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_1);
        Core::BoundedText diagnostic;
        validator.SetMessageConsumer(
            [&](spv_message_level_t, const char *, const spv_position_t &position, const char *message)
            {
                char prefix[64]{};
                std::snprintf(prefix, sizeof(prefix), "SPIR-V word %zu: ", position.index);
                diagnostic.Assign(prefix);
                // 失败诊断在有界存储中组合, OOM 时不为记录错误再次分配.
                diagnostic.Append(message ? message : "validation failed");
            });
        if (!validator.IsValid() || !validator.Validate(words.data(), words.size()))
        {
            throw BuildError(ErrorCode::ValidationFailed, diagnostic.View().empty()
                                                              ? std::string_view("Cannot initialize SPIR-V validator.")
                                                              : diagnostic.View());
        }
    }

    // 接口不一致统一保留为 ABI 失败, 调用方补充程序, 阶段与源路径上下文.
    static void RequireInterface(bool condition, std::string_view context)
    {
        if (!condition)
        {
            throw BuildError(ErrorCode::InterfaceMismatch, context);
        }
    }

    // 核对 32 位基类型, 向量/矩阵形状及数组维数, 不以 GLSL 名称相同替代实际 IR 形状.
    static void CheckType(const spirv_cross::SPIRType &actual, std::string_view expected, bool array,
                          std::string_view context)
    {
        const auto type = ParseDataType(expected);
        constexpr spirv_cross::SPIRType::BaseType Bases[]{spirv_cross::SPIRType::Float, spirv_cross::SPIRType::Int,
                                                          spirv_cross::SPIRType::UInt};
        RequireInterface(actual.basetype == Bases[type.scalarKind] && actual.width == 32 &&
                             actual.vecsize == type.components && actual.columns == type.columns &&
                             actual.array.size() == (array ? 1u : 0u),
                         "IR type mismatch in " + std::string(context) + ": expected " + std::string(expected) +
                             (array ? " array" : "") +
                             ", actual base/width/vector/columns/arrays=" + std::to_string(actual.basetype) + "/" +
                             std::to_string(actual.width) + "/" + std::to_string(actual.vecsize) + "/" +
                             std::to_string(actual.columns) + "/" + std::to_string(actual.array.size()));
    }

    // 从 IR 提取字段 offset/extent/stride 与矩阵方向, 和独立推导的完整块布局比较.
    static BufferLayout ReflectBuffer(const spirv_cross::Compiler &compiler, const spirv_cross::SPIRType &actual,
                                      const BufferLayout &expected)
    {
        RequireInterface(actual.basetype == spirv_cross::SPIRType::Struct &&
                             actual.member_types.size() == expected.fields.size(),
                         "IR block member count mismatch.");
        auto reflected = expected;
        for (std::uint32_t index = 0; index < expected.fields.size(); ++index)
        {
            const auto &field = expected.fields[index];
            auto &result = reflected.fields[index];
            const auto &type = compiler.get_type(actual.member_types[index]);
            CheckType(type, field.type, field.arrayStrideBytes != 0, expected.name + "." + field.name);
            RequireInterface(compiler.get_member_name(actual.self, index) == field.name,
                             "IR block member name mismatch.");
            result.offsetBytes = compiler.type_struct_member_offset(actual, index);
            result.extentBytes = static_cast<std::uint32_t>(compiler.get_declared_struct_member_size(actual, index));
            if (field.arrayStrideBytes != 0)
            {
                RequireInterface(type.array_size_literal[0] && type.array[0] == field.arrayCount,
                                 "IR field array capacity mismatch.");
                result.arrayStrideBytes = compiler.type_struct_member_array_stride(actual, index);
            }
            if (field.matrixStrideBytes != 0)
            {
                RequireInterface(compiler.has_member_decoration(actual.self, index, spv::DecorationColMajor) &&
                                     !compiler.has_member_decoration(actual.self, index, spv::DecorationRowMajor),
                                 "IR matrices must be column-major.");
                result.matrixStrideBytes = compiler.type_struct_member_matrix_stride(actual, index);
            }
            RequireInterface(Json(result) == Json(field), "IR field ABI mismatch: " + expected.name + "." + field.name +
                                                              ", expected=" + Json(field).dump() +
                                                              ", actual=" + Json(result).dump());
        }
        reflected.extentBytes = static_cast<std::uint32_t>(compiler.get_declared_struct_size(actual));
        // IR 的尾 extent 不含宿主 block 的尾 padding, 不能用任意库的 size 与 sizeof 直接比较.
        RequireInterface(Json(reflected) == Json(expected), "IR block extent mismatch: " + expected.name +
                                                                ", expected=" + std::to_string(expected.extentBytes) +
                                                                ", actual=" + std::to_string(reflected.extentBytes));
        return reflected;
    }

    // 全模块块类型须满足固定 record 形状与字节预算, 不因当前入口未活动而跳过 profile.
    static void CheckDataBlockProfile(const spirv_cross::Compiler &compiler, const spirv_cross::SPIRType &block)
    {
        if (block.basetype != spirv_cross::SPIRType::Struct || block.member_types.empty() ||
            compiler.get_declared_struct_size(block) > MaxBlockBytes)
        {
            throw BuildError(ErrorCode::UnsupportedProfile, "IR data block is outside the fixed record/byte budget.");
        }
        for (std::uint32_t index = 0; index < block.member_types.size(); ++index)
        {
            const auto &type = compiler.get_type(block.member_types[index]);
            const bool scalar = type.basetype == spirv_cross::SPIRType::Float ||
                                type.basetype == spirv_cross::SPIRType::Int ||
                                type.basetype == spirv_cross::SPIRType::UInt;
            const bool matrix =
                type.columns == 1 || (type.basetype == spirv_cross::SPIRType::Float && type.columns == type.vecsize &&
                                      type.columns >= 2 && type.columns <= 4);
            const bool array =
                type.array.empty() || (type.array.size() == 1 && type.array_size_literal[0] && type.array[0] > 0);
            if (!scalar || !matrix || !array || type.width != 32 || type.vecsize < 1 || type.vecsize > 4 ||
                compiler.has_member_decoration(block.self, index, spv::DecorationRowMajor))
            {
                throw BuildError(ErrorCode::UnsupportedProfile,
                                 "IR field type/array/matrix is outside the selected profile.");
            }
        }
    }

    // 以 decorations 核对访问合同, 没有单向证明时视为读写.
    static StorageAccess ReflectAccess(const spirv_cross::Bitset &flags)
    {
        const bool noWrite = flags.get(spv::DecorationNonWritable);
        const bool noRead = flags.get(spv::DecorationNonReadable);
        RequireInterface(!(noRead && noWrite), "Conflicting storage access decorations.");
        return noWrite ? StorageAccess::Read : (noRead ? StorageAccess::Write : StorageAccess::ReadWrite);
    }

    // 从 IR 的像素标量类型和显式格式确定 image 的规范拼写, 层类型另由 arrayed 位表达.
    static std::string_view StorageImageFormat(const spirv_cross::Compiler &compiler,
                                               const spirv_cross::SPIRType &type)
    {
        using spirv_cross::SPIRType;
        const auto &scalar = compiler.get_type(type.image.type);
        if (type.image.dim == spv::Dim2D && !type.image.ms && !type.image.depth && type.image.sampled == 2 &&
            scalar.width == 32)
        {
            if (type.image.format == spv::ImageFormatR32ui && scalar.basetype == SPIRType::UInt)
            {
                return "r32ui";
            }
            if (scalar.basetype == SPIRType::Float)
            {
                switch (type.image.format)
                {
                case spv::ImageFormatR32f: return "r32f";
                case spv::ImageFormatRgba8: return "rgba8";
                case spv::ImageFormatRgba16f: return "rgba16f";
                default: break;
                }
            }
        }
        throw BuildError(ErrorCode::UnsupportedProfile, "Unsupported storage image format or shape.");
    }

    // 先检查模块 capability, 执行模式和资源类别, 排除基线之外的扩展与数据访问形状.
    static void CheckProfile(spirv_cross::Compiler &compiler, Stage stage)
    {
        for (const auto capability : compiler.get_declared_capabilities())
        {
            if (capability != spv::CapabilityShader && capability != spv::CapabilityMatrix &&
                !(stage == Stage::Compute && capability == spv::CapabilityStorageImageExtendedFormats))
            {
                throw BuildError(ErrorCode::UnsupportedProfile, "SPIR-V capability is outside the selected profile.");
            }
        }
        if (!compiler.get_declared_extensions().empty() || !compiler.get_specialization_constants().empty())
        {
            throw BuildError(ErrorCode::UnsupportedProfile,
                             "SPIR-V extensions/specialization constants are unsupported.");
        }
        compiler.get_execution_mode_bitset().for_each_bit(
            [stage](std::uint32_t mode)
            {
                const bool fragment = stage == Stage::Fragment &&
                    (mode == spv::ExecutionModeOriginUpperLeft || mode == spv::ExecutionModeDepthReplacing);
                const bool compute = stage == Stage::Compute && mode == spv::ExecutionModeLocalSize;
                if (!fragment && !compute)
                {
                    throw BuildError(ErrorCode::UnsupportedProfile,
                                     "SPIR-V execution mode is outside the selected profile.");
                }
            });
        const auto all = compiler.get_shader_resources();
        if (!all.subpass_inputs.empty() || (stage != Stage::Compute && !all.storage_images.empty()) ||
            !all.sampled_images.empty() ||
            !all.atomic_counters.empty() || !all.acceleration_structures.empty() || !all.gl_plain_uniforms.empty() ||
            !all.tensors.empty() || !all.push_constant_buffers.empty() || !all.shader_record_buffers.empty())
        {
            throw BuildError(ErrorCode::UnsupportedProfile, "SPIR-V resource class is outside the selected profile.");
        }
        for (const auto &buffer : all.storage_buffers)
        {
            if (stage != Stage::Compute && ReflectAccess(compiler.get_buffer_block_flags(buffer.id)) != StorageAccess::Read)
            {
                throw BuildError(ErrorCode::UnsupportedProfile, "Storage buffers must be readonly in the graphics profile.");
            }
            const auto &block = compiler.get_type(buffer.base_type_id);
            if (block.member_types.size() != 1 || compiler.get_declared_struct_size(block) > MaxBlockBytes)
            {
                throw BuildError(ErrorCode::UnsupportedProfile, "Storage buffer requires one bounded record array.");
            }
            const auto &records = compiler.get_type(block.member_types[0]);
            if (records.basetype != spirv_cross::SPIRType::Struct || records.array.size() != 1 ||
                !records.array_size_literal[0] || records.array[0] == 0)
            {
                throw BuildError(ErrorCode::UnsupportedProfile,
                                 "Storage buffer requires a fixed positive record capacity.");
            }
            CheckDataBlockProfile(compiler, records);
        }
        for (const auto &buffer : all.uniform_buffers)
        {
            CheckDataBlockProfile(compiler, compiler.get_type(buffer.base_type_id));
        }
        for (const auto &items : {all.uniform_buffers, all.storage_buffers, all.separate_images, all.separate_samplers, all.storage_images})
        {
            for (const auto &resource : items)
            {
                if (!compiler.get_type(resource.type_id).array.empty())
                {
                    throw BuildError(ErrorCode::UnsupportedProfile,
                                     "Descriptor arrays are outside the selected profile.");
                }
            }
        }
        for (const auto &image : all.separate_images)
        {
            const auto &type = compiler.get_type(image.type_id);
            const bool cube = type.image.dim == spv::DimCube && !type.image.arrayed;
            const bool plane = type.image.dim == spv::Dim2D && (!type.image.arrayed || stage != Stage::Compute);
            if (!(cube || plane) ||
                type.image.ms || type.image.depth ||
                type.image.sampled != 1 || compiler.get_type(type.image.type).basetype != spirv_cross::SPIRType::Float)
            {
                throw BuildError(ErrorCode::UnsupportedProfile,
                                 "Unsupported separate float texture2D, texture2DArray or textureCube shape.");
            }
        }
        for (const auto &image : all.storage_images)
        {
            (void)StorageImageFormat(compiler, compiler.get_type(image.type_id));
        }
        compiler.update_active_builtins();
        for (const auto &resources : {all.builtin_inputs, all.builtin_outputs})
        {
            for (const auto &resource : resources)
            {
                const auto storage = compiler.get_type(resource.resource.type_id).storage;
                if (!compiler.has_active_builtin(resource.builtin, storage))
                {
                    continue;
                }
                const auto builtin = resource.builtin;
                const bool compute = stage == Stage::Compute &&
                    (builtin == spv::BuiltInNumWorkgroups || builtin == spv::BuiltInWorkgroupSize ||
                     builtin == spv::BuiltInWorkgroupId || builtin == spv::BuiltInLocalInvocationId ||
                     builtin == spv::BuiltInGlobalInvocationId || builtin == spv::BuiltInLocalInvocationIndex);
                const bool allowed = stage == Stage::Compute ? compute : stage == Stage::Vertex
                                         ? (builtin == spv::BuiltInPosition || builtin == spv::BuiltInPointSize ||
                                            builtin == spv::BuiltInVertexIndex || builtin == spv::BuiltInInstanceIndex)
                                         : (builtin == spv::BuiltInFragCoord || builtin == spv::BuiltInPointCoord ||
                                            builtin == spv::BuiltInFrontFacing || builtin == spv::BuiltInFragDepth);
                if (!allowed)
                {
                    throw BuildError(ErrorCode::UnsupportedProfile, "Active builtin is outside the selected profile.");
                }
            }
        }
    }

    // 以最终 set/binding 定位声明, 核对资源名, 种类, 可见阶段和具体 IR 布局/图像形状.
    static ResourceReflection ReflectResource(const spirv_cross::Compiler &compiler,
                                              const spirv_cross::Resource &actual, ResourceKind kind,
                                              const InterfaceSchema &schema, Stage stage)
    {
        RequireInterface(compiler.has_decoration(actual.id, spv::DecorationDescriptorSet) &&
                             compiler.has_decoration(actual.id, spv::DecorationBinding),
                         "IR resource lacks set/binding.");
        const auto set = compiler.get_decoration(actual.id, spv::DecorationDescriptorSet);
        const auto binding = compiler.get_decoration(actual.id, spv::DecorationBinding);
        const auto &name = compiler.get_name(actual.id);
        const auto named = std::ranges::find_if(schema.bindings,
                                                [&](const auto &value)
                                                {
                                                    return value.resource.name == name;
                                                });
        const auto found =
            std::ranges::find_if(schema.bindings,
                                 [&](const auto &value)
                                 {
                                     return value.resource.set == set && value.resource.binding == binding;
                                 });
        RequireInterface(found != schema.bindings.end(),
                         "IR resource has an undeclared set/binding: " + name + ", actual=" + std::to_string(set) +
                             "/" + std::to_string(binding) + ", expected=" +
                             (named == schema.bindings.end() ? std::string("undeclared")
                                                             : std::to_string(named->resource.set) + "/" +
                                                                   std::to_string(named->resource.binding)));
        const auto &expected = found->resource;
        RequireInterface(kind == expected.kind && (found->stageMask & (1u << static_cast<std::uint32_t>(stage))) != 0,
                         "IR resource class, dimension or stage mismatch: " + expected.name + ", expected kind=" +
                             std::string(ResourceKindName(expected.kind)) + ", actual kind=" +
                             std::string(ResourceKindName(kind)) + ", actual stage=" +
                             std::string(StageName(stage)) + ", expected mask=" + std::to_string(found->stageMask));
        RequireInterface(compiler.get_name(actual.id) == expected.name,
                         "IR resource name mismatch: expected=" + expected.name +
                             ", actual=" + compiler.get_name(actual.id));
        const auto &variable = compiler.get_type(actual.type_id);
        RequireInterface(variable.array.empty(), "Descriptor arrays are outside the selected profile.");
        auto result = expected;
        if (kind == ResourceKind::ConstantBuffer)
        {
            result.buffer = ReflectBuffer(compiler, compiler.get_type(actual.base_type_id), expected.buffer);
        }
        else if (IsStructured(kind))
        {
            const auto access = ReflectAccess(compiler.get_buffer_block_flags(actual.id));
            RequireInterface(access == expected.access, "IR buffer access mismatch: " + expected.name);
            result.access = access;
            const auto &block = compiler.get_type(actual.base_type_id);
            RequireInterface(block.member_types.size() == 1 && compiler.type_struct_member_offset(block, 0) == 0,
                             "IR structured buffer requires one record array at offset 0.");
            const auto &records = compiler.get_type(block.member_types[0]);
            RequireInterface(records.array.size() == 1 && records.array_size_literal[0] &&
                                 records.array[0] == expected.capacity,
                             "IR record capacity mismatch.");
            result.recordStrideBytes = compiler.type_struct_member_array_stride(block, 0);
            RequireInterface(result.recordStrideBytes == expected.recordStrideBytes, "IR record stride mismatch.");
            result.buffer = ReflectBuffer(compiler, records, expected.buffer);
        }
        else if (IsStorageImage(kind))
        {
            result.imageFormat = StorageImageFormat(compiler, variable);
            result.access = ReflectAccess(compiler.get_decoration_bitset(actual.id));
            RequireInterface(result.imageFormat == expected.imageFormat && result.access == expected.access,
                             "IR storage image format/access mismatch: " + expected.name);
        }
        else if (kind == ResourceKind::Sampler)
        {
            RequireInterface(variable.basetype == spirv_cross::SPIRType::Sampler, "IR sampler type mismatch.");
        }
        return result;
    }

    // 按最终 location 收集活动 IO, 类型/名称/装饰与声明独立比较, 最后按 location 排序.
    static std::vector<IoReflection> ReflectIo(const spirv_cross::Compiler &compiler,
                                               const spirv_cross::SmallVector<spirv_cross::Resource> &actual,
                                               const InterfaceSchema &schema, Stage stage, bool input)
    {
        std::vector<IoReflection> result;
        std::set<std::uint32_t> locations;
        for (const auto &resource : actual)
        {
            RequireInterface(compiler.has_decoration(resource.id, spv::DecorationLocation) &&
                                 !compiler.has_decoration(resource.id, spv::DecorationComponent) &&
                                 !compiler.has_decoration(resource.id, spv::DecorationIndex) &&
                                 !compiler.has_decoration(resource.id, spv::DecorationCentroid) &&
                                 !compiler.has_decoration(resource.id, spv::DecorationSample),
                             "Unsupported IR IO decorations.");
            const auto location = compiler.get_decoration(resource.id, spv::DecorationLocation);
            RequireInterface(locations.insert(location).second, "IR user IO location overlaps.");
            const auto found =
                std::ranges::find_if(schema.io,
                                     [&](const auto &io)
                                     {
                                         return io.stage == stage && io.input == input && io.value.location == location;
                                     });
            RequireInterface(found != schema.io.end(), "IR IO has an undeclared location.");
            auto value = found->value;
            CheckType(compiler.get_type(resource.type_id), value.type, false, value.name);
            RequireInterface(compiler.get_name(resource.id) == value.name, "IR IO name mismatch.");
            const bool interStage = (stage == Stage::Vertex && !input) || (stage == Stage::Fragment && input);
            const bool flat = compiler.has_decoration(resource.id, spv::DecorationFlat);
            const bool noPerspective = compiler.has_decoration(resource.id, spv::DecorationNoPerspective);
            RequireInterface(!(flat && noPerspective) && (interStage || (!flat && !noPerspective)),
                             "IR IO interpolation decorates an unsupported endpoint.");
            // 活动反射取最终 IR, 声明仅作独立预期, 不直接复制声明的插值限定.
            value.interpolation = flat ? "flat" : (noPerspective ? "noperspective" : "smooth");
            RequireInterface(value.interpolation == found->value.interpolation, "IR IO interpolation mismatch.");
            result.push_back(std::move(value));
        }
        std::ranges::sort(result, {}, &IoReflection::location);
        return result;
    }

    void ReflectModule(ShaderModule &module, const InterfaceSchema &schema)
    try
    {
        ValidateSpirv(module.words);
        spirv_cross::Compiler compiler(module.words);
        const auto entries = compiler.get_entry_points_and_stages();
        constexpr spv::ExecutionModel Models[]{spv::ExecutionModelVertex, spv::ExecutionModelFragment,
                                               spv::ExecutionModelGLCompute};
        const auto model = Models[static_cast<unsigned>(module.stage)];
        RequireInterface(module.entry == "main" && entries.size() == 1 && entries[0].name == module.entry &&
                             entries[0].execution_model == model,
                         "IR entry point/stage mismatch.");
        compiler.set_entry_point(module.entry, model);
        // 先对整个模块检查 capability 和资源类别, 再按入口的活动变量核对 ABI.
        // 合法但未被入口使用的资源仍保留在声明快照中.
        CheckProfile(compiler, module.stage);
        const auto active = compiler.get_shader_resources(compiler.get_active_interface_variables());
        std::vector<ResourceReflection> resources;
        const auto append = [&](const auto &items, ResourceKind kind)
        {
            for (const auto &item : items)
            {
                resources.push_back(ReflectResource(compiler, item, kind, schema, module.stage));
            }
        };
        append(active.uniform_buffers, ResourceKind::ConstantBuffer);
        // readonly 本身不能区分 t 和 u 槽. 先从 canonical binding 取逻辑类别, 再核对方向和记录布局.
        for (const auto &buffer : active.storage_buffers)
        {
            const auto binding = compiler.get_decoration(buffer.id, spv::DecorationBinding);
            const auto kind = binding >= CanonicalBinding(ResourceKind::StructuredUav, 0) &&
                binding <= CanonicalBinding(ResourceKind::StructuredUav, 127) ? ResourceKind::StructuredUav :
                                                                                ResourceKind::StructuredBuffer;
            resources.push_back(ReflectResource(compiler, buffer, kind, schema, module.stage));
        }
        for (const auto &image : active.storage_images)
        {
            const auto kind = compiler.get_type(image.type_id).image.arrayed ? ResourceKind::Image2DArray :
                                                                               ResourceKind::Image2D;
            resources.push_back(ReflectResource(compiler, image, kind, schema, module.stage));
        }
        std::array<std::uint32_t, 3> localSize{};
        if (module.stage == Stage::Compute)
        {
            RequireInterface(compiler.get_execution_mode_bitset().get(spv::ExecutionModeLocalSize),
                             "Compute requires a fixed local size.");
            for (std::uint32_t axis = 0; axis < localSize.size(); ++axis)
            {
                localSize[axis] = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, axis);
            }
            RequireInterface(localSize[0] != 0 && localSize == schema.localSize,
                             "IR workgroup size does not match the interface.");
        }
        // 从已通过 profile 校验的 IR 取实际层维度, 保留数组图像与 descriptor 数组的区别.
        for (const auto &image : active.separate_images)
        {
            const auto &shape = compiler.get_type(image.type_id).image;
            auto kind = ResourceKind::Texture2D;
            if (shape.arrayed)
            {
                kind = ResourceKind::Texture2DArray;
            }
            else if (shape.dim == spv::DimCube)
            {
                kind = ResourceKind::TextureCube;
            }
            resources.push_back(ReflectResource(compiler, image, kind, schema, module.stage));
        }
        append(active.separate_samplers, ResourceKind::Sampler);
        std::ranges::sort(resources,
                          [](const auto &left, const auto &right)
                          {
                              return std::tie(left.set, left.binding) < std::tie(right.set, right.binding);
                          });
        for (std::size_t index = 1; index < resources.size(); ++index)
        {
            RequireInterface(resources[index - 1].set != resources[index].set ||
                                 resources[index - 1].binding != resources[index].binding,
                             "IR resource binding overlaps.");
        }
        // 完整核对成功后才替换请求结果, 失败不能留下部分反射作为成功包消费.
        auto inputs = ReflectIo(compiler, active.stage_inputs, schema, module.stage, true);
        auto outputs = ReflectIo(compiler, active.stage_outputs, schema, module.stage, false);
        module.resources = std::move(resources);
        module.inputs = std::move(inputs);
        module.outputs = std::move(outputs);
        module.localSize = localSize;
    }
    catch (const BuildError &error)
    {
        const auto profile = module.stage == Stage::Compute ? ComputeProfile : GraphicsProfile;
        throw BuildError(error.GetFailure().code,
                         std::string(profile) + " / " + std::string(StageName(module.stage)) + ": " + error.what());
    }

    void CheckProgramInterfaces(const std::vector<ShaderModule> &modules, std::string_view profile)
    {
        if (profile == ComputeProfile)
        {
            RequireInterface(modules.size() == 1 && modules.front().stage == Stage::Compute,
                             "ComputeBaselineV1 requires one compute stage.");
            return;
        }
        RequireInterface(profile == GraphicsProfile, "Unsupported Shader package profile.");
        RequireInterface(modules.size() == 2 && modules[0].stage == Stage::Vertex &&
                             modules[1].stage == Stage::Fragment,
                         "GraphicsBaselineV1 requires one VS and one FS.");
        for (const auto &input : modules[1].inputs)
        {
            const auto found = std::ranges::find(modules[0].outputs, input.location, &IoReflection::location);
            RequireInterface(found != modules[0].outputs.end() && found->type == input.type &&
                                 found->interpolation == input.interpolation,
                             "FS input lacks a compatible VS output.");
        }
        for (const auto &vertex : modules[0].resources)
        {
            for (const auto &fragment : modules[1].resources)
            {
                if (vertex.set == fragment.set && vertex.binding == fragment.binding)
                {
                    RequireInterface(Json(vertex) == Json(fragment), "Cross-stage resource ABI mismatch.");
                }
            }
        }
    }
}
