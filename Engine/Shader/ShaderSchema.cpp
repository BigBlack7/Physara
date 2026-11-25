#include "ShaderSchema.hpp"

/// @file
/// @brief 严格解析有限 Shader 声明, 计算 std140/std430 布局, 逻辑绑定和完整接口 ABI.

#include <algorithm>
#include <array>
#include <set>
#include <tuple>

#include <RHI.hpp>

namespace Physara::Shader
{
    DataType ParseDataType(std::string_view name)
    {
        DataType type;
        if (name == "float32" || name == "int32" || name == "uint32")
        {
            type.scalarKind = name == "float32" ? 0u : (name == "int32" ? 1u : 2u);
        }
        else if (name.size() == 8 && name.starts_with("float") && name[6] == 'x' && name[5] == name[7] &&
                 name[5] >= '2' && name[5] <= '4')
        {
            type.columns = static_cast<std::uint32_t>(name[5] - '0');
            type.components = type.columns;
        }
        else
        {
            if (name.empty() || name.back() < '2' || name.back() > '4')
            {
                throw BuildError(ErrorCode::InvalidInput, "Unsupported data type: " + std::string(name));
            }
            const auto base = name.substr(0, name.size() - 1);
            if (base != "float" && base != "int" && base != "uint")
            {
                throw BuildError(ErrorCode::InvalidInput, "Unsupported data type: " + std::string(name));
            }
            type.components = static_cast<std::uint32_t>(name.back() - '0');
            type.scalarKind = base == "float" ? 0u : (base == "int" ? 1u : 2u);
        }
        constexpr std::array Scalars{"float", "std::int32_t", "std::uint32_t"};
        constexpr std::array GlslScalars{"float", "int", "uint"};
        constexpr std::array GlslVectors{"vec", "ivec", "uvec"};
        type.cppScalar = Scalars[type.scalarKind];
        type.glsl = type.columns > 1
                        ? "mat" + std::to_string(type.columns)
                        : (type.components > 1 ? GlslVectors[type.scalarKind] + std::to_string(type.components)
                                               : std::string(GlslScalars[type.scalarKind]));
        return type;
    }

    Stage ParseStage(std::string_view name)
    {
        if (name == "vertex")
        {
            return Stage::Vertex;
        }
        if (name == "fragment")
        {
            return Stage::Fragment;
        }
        if (name == "compute")
        {
            return Stage::Compute;
        }
        throw BuildError(ErrorCode::UnsupportedProfile, "Unsupported stage: " + std::string(name));
    }

    std::string_view StageName(Stage stage) noexcept
    {
        switch (stage)
        {
        case Stage::Vertex: return "vertex";
        case Stage::Fragment: return "fragment";
        case Stage::Compute: return "compute";
        }
        return "invalid";
    }

    ResourceKind ParseResourceKind(std::string_view name)
    {
        if (name == "constant-buffer")
        {
            return ResourceKind::ConstantBuffer;
        }
        if (name == "structured-buffer")
        {
            return ResourceKind::StructuredBuffer;
        }
        if (name == "texture2d")
        {
            return ResourceKind::Texture2D;
        }
        if (name == "texturecube")
        {
            return ResourceKind::TextureCube;
        }
        if (name == "texture2d-array")
        {
            return ResourceKind::Texture2DArray;
        }
        if (name == "sampler")
        {
            return ResourceKind::Sampler;
        }
        if (name == "structured-uav")
        {
            return ResourceKind::StructuredUav;
        }
        if (name == "image2d")
        {
            return ResourceKind::Image2D;
        }
        if (name == "image2d-array")
        {
            return ResourceKind::Image2DArray;
        }
        throw BuildError(ErrorCode::UnsupportedProfile, "Unsupported resource: " + std::string(name));
    }

    std::string_view ResourceKindName(ResourceKind kind) noexcept
    {
        switch (kind)
        {
        case ResourceKind::ConstantBuffer:
            return "constant-buffer";
        case ResourceKind::StructuredBuffer:
            return "structured-buffer";
        case ResourceKind::Texture2D:
            return "texture2d";
        case ResourceKind::TextureCube:
            return "texturecube";
        case ResourceKind::Texture2DArray:
            return "texture2d-array";
        case ResourceKind::Sampler:
            return "sampler";
        case ResourceKind::StructuredUav:
            return "structured-uav";
        case ResourceKind::Image2D:
            return "image2d";
        case ResourceKind::Image2DArray:
            return "image2d-array";
        }
        return "invalid";
    }

    StorageAccess ParseStorageAccess(std::string_view name)
    {
        if (name == "read")
        {
            return StorageAccess::Read;
        }
        if (name == "write")
        {
            return StorageAccess::Write;
        }
        if (name == "read-write")
        {
            return StorageAccess::ReadWrite;
        }
        throw BuildError(ErrorCode::InvalidInput, "Unsupported storage access.");
    }

    std::string_view StorageAccessName(StorageAccess access) noexcept
    {
        switch (access)
        {
        case StorageAccess::Read: return "read";
        case StorageAccess::Write: return "write";
        case StorageAccess::ReadWrite: return "read-write";
        }
        return "invalid";
    }

    std::string ParseStorageFormat(const Json &value)
    {
        auto format = value.get<std::string>();
        if (format != "r32f" && format != "r32ui" && format != "rgba8" && format != "rgba16f")
        {
            throw BuildError(ErrorCode::UnsupportedProfile, "Unsupported storage image format.");
        }
        return format;
    }

    std::array<std::uint32_t, 3> ParseLocalSize(const Json &value)
    {
        if (!value.is_array() || value.size() != 3)
        {
            throw BuildError(ErrorCode::InvalidInput, "localSize requires three positive axes.");
        }
        std::array<std::uint32_t, 3> size{};
        for (std::size_t axis = 0; axis < size.size(); ++axis)
        {
            size[axis] = UInt(value.at(axis), UINT32_MAX, "localSize");
            if (size[axis] == 0)
            {
                throw BuildError(ErrorCode::InvalidInput, "localSize axes must be positive.");
            }
        }
        return size;
    }

    std::uint32_t CanonicalBinding(ResourceKind kind, std::uint32_t slot)
    {
        const RHI::VulkanBindingOffsets offsets;
        const auto offset = kind == ResourceKind::ConstantBuffer
                                ? offsets.constantBuffer
                                : (kind == ResourceKind::Sampler ? offsets.sampler :
                                   (IsUnordered(kind) ? offsets.unorderedAccess : offsets.shaderResource));
        const auto value = std::uint64_t(offset) + slot;
        if (slot > 127 || value > 511)
        {
            throw BuildError(ErrorCode::InvalidInput, "Canonical binding exceeds GraphicsBaselineV1 range.");
        }
        return static_cast<std::uint32_t>(value);
    }

    // 已知类型规则提供正对齐, 用宽中间值向上取整并在返回前执行块预算约束.
    static std::uint32_t AlignData(std::uint32_t size, std::uint32_t alignment)
    {
        // alignment 仅来自固定 32 位类型规则, 数学中间值使用宽类型, 再按 block 预算收窄.
        const auto aligned = (std::uint64_t(size) + alignment - 1) / alignment * alignment;
        if (aligned > MaxBlockBytes)
        {
            throw BuildError(ErrorCode::InvalidInput, "Data layout exceeds block byte budget.");
        }
        return static_cast<std::uint32_t>(aligned);
    }

    BufferLayout ParseBlockLayout(const Json &json)
    {
        CheckFields(json, {"name", "layout", "fields"}, "/blocks");
        BufferLayout block;
        block.name = Identifier(json.at("name"), "/blocks/name");
        if (block.name.front() < 'A' || block.name.front() > 'Z')
        {
            throw BuildError(ErrorCode::InvalidInput, "/blocks/name requires PascalCase.");
        }
        block.layout = json.at("layout").get<std::string>();
        if (block.layout != "std140" && block.layout != "std430")
        {
            throw BuildError(ErrorCode::InvalidInput, "/blocks/layout requires std140 or std430.");
        }
        const auto &fields = json.at("fields");
        if (!fields.is_array() || fields.empty())
        {
            throw BuildError(ErrorCode::InvalidInput, "/blocks/fields requires nonempty array.");
        }
        std::set<std::string> names;
        block.alignmentBytes = block.layout == "std140" ? 16u : 4u;
        for (const auto &item : fields)
        {
            CheckFields(item, {"name", "type", "array"}, "/blocks/fields");
            FieldLayout field;
            field.name = Identifier(item.at("name"), "/blocks/" + block.name + "/fields/name");
            if (field.name.front() < 'a' || field.name.front() > 'z' || field.name.starts_with("physara"))
            {
                throw BuildError(ErrorCode::InvalidInput, "Field requires camelCase; physara prefix is reserved.");
            }
            if (!names.insert(field.name).second)
            {
                throw BuildError(ErrorCode::InvalidInput, "Duplicate field: " + block.name + "." + field.name);
            }
            field.type = item.at("type").get<std::string>();
            DataType type;
            try
            {
                type = ParseDataType(field.type);
            }
            catch (const BuildError &error)
            {
                throw BuildError(error.GetFailure().code,
                                 "/blocks/" + block.name + "/fields/" + field.name + "/type: " + error.what());
            }
            auto helperName = field.name;
            helperName.front() = static_cast<char>(helperName.front() - 'a' + 'A');
            if ((type.columns > 1 && helperName + "Column" == block.name) ||
                (item.contains("array") && helperName + "Element" == block.name))
            {
                throw BuildError(ErrorCode::InvalidInput, "Generated helper name conflicts with block: " + block.name);
            }
            // vec3 按 16 字节对齐, 字段本身只占 12 字节, 后接的标量可用第四个分量的位置.
            // 矩阵按列 stride 推进, 数组按元素 stride 推进. 整个 block 的尾部在所有字段排完后对齐.
            const auto vectorAlign = type.components == 3 ? 16u : type.components * 4;
            auto alignment = vectorAlign;
            std::uint64_t extent = type.components * 4;
            if (type.columns > 1)
            {
                alignment = block.layout == "std140" ? std::max(16u, alignment) : alignment;
                field.matrixStrideBytes = AlignData(static_cast<std::uint32_t>(extent), alignment);
                extent = std::uint64_t(field.matrixStrideBytes) * type.columns;
            }
            if (item.contains("array"))
            {
                field.arrayCount = UInt(item.at("array"), MaxBlockBytes, "/blocks/fields/array");
                if (field.arrayCount == 0)
                {
                    throw BuildError(ErrorCode::InvalidInput, "Zero array size: " + block.name + "." + field.name);
                }
                alignment = block.layout == "std140" ? std::max(16u, alignment) : alignment;
                field.arrayStrideBytes = AlignData(static_cast<std::uint32_t>(extent), alignment);
                extent = std::uint64_t(field.arrayStrideBytes) * field.arrayCount;
            }
            field.offsetBytes = AlignData(block.extentBytes, alignment);
            if (extent + field.offsetBytes > MaxBlockBytes)
            {
                throw BuildError(ErrorCode::InvalidInput, "Block size overflow/budget: " + block.name);
            }
            field.extentBytes = static_cast<std::uint32_t>(extent);
            block.extentBytes = field.offsetBytes + field.extentBytes;
            block.alignmentBytes = std::max(block.alignmentBytes, alignment);
            block.fields.push_back(std::move(field));
        }
        block.sizeBytes = AlignData(block.extentBytes, block.alignmentBytes);
        return block;
    }

    const BufferLayout &FindBlock(const InterfaceSchema &schema, std::string_view name)
    {
        const auto block = std::ranges::find(schema.blocks, name, &BufferLayout::name);
        if (block == schema.blocks.end())
        {
            throw BuildError(ErrorCode::InvalidInput, "Unknown block: " + std::string(name));
        }
        return *block;
    }

    // 从类别及可见阶段推导逻辑槽并核对布局. 固定记录数组按容量和 stride 计预算, image 单独校验格式与方向.
    static BindingSchema ParseBinding(const Json &json, const InterfaceSchema &schema)
    {
        CheckFields(json, {"name", "kind", "set", "slot", "block", "capacity", "stages", "access", "format"}, "/resources");
        BindingSchema binding;
        auto &resource = binding.resource;
        resource.name = Identifier(json.at("name"), "/resources/name");
        if (resource.name.front() < 'a' || resource.name.front() > 'z')
        {
            throw BuildError(ErrorCode::InvalidInput, "Resource name requires camelCase.");
        }
        resource.kind = ParseResourceKind(json.at("kind").get<std::string>());
        resource.set = UInt(json.at("set"), 3, "/resources/set");
        resource.slot = UInt(json.at("slot"), 127, "/resources/slot");
        const auto &stages = json.at("stages");
        if (!stages.is_array() || stages.empty())
        {
            throw BuildError(ErrorCode::InvalidInput, "/resources/stages requires nonempty array.");
        }
        for (const auto &stage : stages)
        {
            const auto mask = 1u << static_cast<std::uint32_t>(ParseStage(stage.get<std::string>()));
            if ((binding.stageMask & mask) != 0)
            {
                throw BuildError(ErrorCode::InvalidInput, "Duplicate resource visibility: " + resource.name);
            }
            binding.stageMask |= mask;
        }
        constexpr auto ComputeMask = 1u << static_cast<unsigned>(Stage::Compute);
        if (resource.kind == ResourceKind::Texture2DArray && (binding.stageMask & ComputeMask) != 0)
        {
            throw BuildError(ErrorCode::InvalidInput,
                             "Resource " + resource.name + ": texture2d-array is not supported in the compute stage.");
        }
        if ((schema.localSize[0] != 0 && binding.stageMask != ComputeMask) ||
            (schema.localSize[0] == 0 && (binding.stageMask & ComputeMask) != 0) ||
            (IsUnordered(resource.kind) && binding.stageMask != ComputeMask))
        {
            throw BuildError(ErrorCode::InvalidInput, "Resource visibility does not match the interface profile.");
        }
        if (IsUnordered(resource.kind))
        {
            resource.access = ParseStorageAccess(json.at("access").get<std::string>());
        }
        else if (json.contains("access"))
        {
            throw BuildError(ErrorCode::InvalidInput, "Only storage resources declare access.");
        }
        if (IsStorageImage(resource.kind))
        {
            resource.imageFormat = ParseStorageFormat(json.at("format"));
        }
        else if (json.contains("format"))
        {
            throw BuildError(ErrorCode::InvalidInput, "Only storage images declare a format.");
        }
        resource.binding = CanonicalBinding(resource.kind, resource.slot);
        const bool buffer =
            resource.kind == ResourceKind::ConstantBuffer || IsStructured(resource.kind);
        if (buffer)
        {
            resource.buffer = FindBlock(schema, json.at("block").get<std::string>());
            const bool structured = IsStructured(resource.kind);
            if (resource.buffer.layout != (structured ? "std430" : "std140"))
            {
                throw BuildError(ErrorCode::InvalidInput, "Resource/block layout mismatch: " + resource.name);
            }
            if (structured)
            {
                resource.capacity = UInt(json.at("capacity"), MaxBlockBytes, "/resources/capacity");
                resource.recordStrideBytes = resource.buffer.sizeBytes;
                if (resource.capacity == 0 ||
                    std::uint64_t(resource.capacity) * resource.recordStrideBytes > MaxBlockBytes)
                {
                    throw BuildError(ErrorCode::InvalidInput,
                                     "Structured buffer exceeds byte budget: " + resource.name);
                }
            }
            else if (json.contains("capacity"))
            {
                throw BuildError(ErrorCode::InvalidInput, "Constant buffer cannot declare capacity.");
            }
        }
        else if (json.contains("block") || json.contains("capacity"))
        {
            throw BuildError(ErrorCode::InvalidInput, "Texture/sampler cannot declare buffer fields.");
        }
        return binding;
    }

    // 用户 IO 限定为单 location 标量/向量, 阶段端点与跨阶段插值边分别校验.
    static IoSchema ParseIo(const Json &json)
    {
        CheckFields(json, {"stage", "direction", "name", "location", "type", "interpolation"}, "/io");
        IoSchema io;
        io.stage = ParseStage(json.at("stage").get<std::string>());
        if (io.stage == Stage::Compute)
        {
            throw BuildError(ErrorCode::InvalidInput, "Compute interfaces have no user stage IO.");
        }
        const auto direction = json.at("direction").get<std::string>();
        if (direction != "in" && direction != "out")
        {
            throw BuildError(ErrorCode::InvalidInput, "/io/direction requires in or out.");
        }
        io.input = direction == "in";
        io.value.name = Identifier(json.at("name"), "/io/name");
        io.value.location = UInt(json.at("location"), 31, "/io/location");
        io.value.type = json.at("type").get<std::string>();
        const auto type = ParseDataType(io.value.type);
        io.value.interpolation = json.value("interpolation", std::string("smooth"));
        if (type.columns != 1 || (io.value.interpolation != "smooth" && io.value.interpolation != "flat" &&
                                  io.value.interpolation != "noperspective"))
        {
            throw BuildError(ErrorCode::UnsupportedProfile, "Unsupported stage IO shape/interpolation.");
        }
        const bool interStage = (io.stage == Stage::Vertex && !io.input) || (io.stage == Stage::Fragment && io.input);
        // 插值只描述 VS 输出到 FS 输入的边. 端点不能把未写入 IR 的值带入 ABI/活动反射.
        if (!interStage && io.value.interpolation != "smooth")
        {
            throw BuildError(ErrorCode::InvalidInput,
                             "/io/" + io.value.name +
                                 "/interpolation applies only to vertex outputs and fragment inputs.");
        }
        if (interStage && type.scalarKind != 0 && io.value.interpolation != "flat")
        {
            throw BuildError(ErrorCode::InvalidInput, "Integer inter-stage IO requires flat interpolation.");
        }
        return io;
    }

    InterfaceSchema ParseInterfaceSchema(const Json &document)
    {
        CheckFields(document, {"version", "name", "blocks", "resources", "io", "localSize"}, "/");
        if (UInt(document.at("version"), 1, "/version") != 1)
        {
            throw BuildError(ErrorCode::InvalidInput, "Unsupported interface version.");
        }
        InterfaceSchema schema;
        if (document.contains("localSize"))
        {
            schema.localSize = ParseLocalSize(document.at("localSize"));
            if (!document.at("io").empty())
            {
                throw BuildError(ErrorCode::InvalidInput, "Compute interfaces have no user stage IO.");
            }
        }
        schema.name = Identifier(document.at("name"), "/name");
        if (schema.name.front() < 'A' || schema.name.front() > 'Z')
        {
            throw BuildError(ErrorCode::InvalidInput, "/name requires PascalCase.");
        }
        for (const auto *name : {"blocks", "resources", "io"})
        {
            if (!document.at(name).is_array())
            {
                throw BuildError(ErrorCode::InvalidInput, std::string("/") + name + " requires array.");
            }
        }
        std::set<std::string> blockNames;
        std::set<std::string> generatedNames{"AbiHash", "MakeBindingLayouts"};
        for (const auto &block : document.at("blocks"))
        {
            auto value = ParseBlockLayout(block);
            if (!blockNames.insert(value.name).second || !generatedNames.insert(value.name).second)
            {
                throw BuildError(ErrorCode::InvalidInput, "Duplicate block: " + value.name);
            }
            schema.blocks.push_back(std::move(value));
        }
        std::set<std::string> resourceNames;
        std::set<std::pair<std::uint32_t, std::uint32_t>> bindings;
        for (const auto &resource : document.at("resources"))
        {
            auto binding = ParseBinding(resource, schema);
            if (!resourceNames.insert(binding.resource.name).second ||
                !bindings.emplace(binding.resource.set, binding.resource.binding).second)
            {
                throw BuildError(ErrorCode::InvalidInput, "Duplicate resource identity: " + binding.resource.name);
            }
            schema.bindings.push_back(std::move(binding));
        }
        std::set<std::string> glslTypes;
        for (const auto &block : schema.blocks)
        {
            if (block.layout == "std430")
            {
                glslTypes.insert(block.name);
            }
        }
        for (const auto &binding : schema.bindings)
        {
            auto name = binding.resource.name;
            name.front() = static_cast<char>(name.front() - 'a' + 'A');
            if (IsStructured(binding.resource.kind) &&
                (!generatedNames.insert(name + "Capacity").second || !generatedNames.insert(name + "Data").second))
            {
                throw BuildError(ErrorCode::InvalidInput, "Generated record name conflicts: " + name);
            }
            if ((binding.resource.kind == ResourceKind::ConstantBuffer ||
                 IsStructured(binding.resource.kind)) &&
                !glslTypes.insert(name + "Block").second)
            {
                throw BuildError(ErrorCode::InvalidInput, "Generated GLSL block name conflicts: " + name);
            }
        }
        std::set<std::tuple<Stage, bool, std::uint32_t>> locations;
        std::set<std::pair<Stage, std::string>> ioNames;
        for (const auto &item : document.at("io"))
        {
            auto io = ParseIo(item);
            if (!locations.emplace(io.stage, io.input, io.value.location).second ||
                !ioNames.emplace(io.stage, io.value.name).second || resourceNames.contains(io.value.name))
            {
                throw BuildError(ErrorCode::InvalidInput, "Duplicate IO location/name: " + io.value.name);
            }
            schema.io.push_back(std::move(io));
        }
        std::ranges::sort(schema.blocks, {}, &BufferLayout::name);
        std::ranges::sort(schema.bindings,
                          [](const auto &left, const auto &right)
                          {
                              return std::tie(left.resource.set, left.resource.binding) <
                                     std::tie(right.resource.set, right.resource.binding);
                          });
        std::ranges::sort(schema.io,
                          [](const auto &left, const auto &right)
                          {
                              return std::tie(left.stage, left.input, left.value.location) <
                                     std::tie(right.stage, right.input, right.value.location);
                          });
        // 规范化声明保留字段顺序, 集合按语义身份排序. 派生布局也进入 ABI, 避免规则实现变化漏失效.
        schema.canonical = document;
        schema.abiHash = ComputeInterfaceAbi(SnapshotInterface(schema));
        return schema;
    }
    InterfaceDeclaration SnapshotInterface(const InterfaceSchema &schema)
    {
        return {schema.name, schema.blocks, schema.bindings, schema.io, schema.localSize};
    }

    Json DeclarationSchema(const InterfaceDeclaration &declaration)
    {
        Json blocks = Json::array();
        for (const auto &block : declaration.blocks)
        {
            Json fields = Json::array();
            for (const auto &field : block.fields)
            {
                Json value{{"name", field.name}, {"type", field.type}};
                if (field.arrayStrideBytes != 0)
                {
                    value["array"] = field.arrayCount;
                }
                fields.push_back(std::move(value));
            }
            blocks.push_back({{"name", block.name}, {"layout", block.layout}, {"fields", fields}});
        }
        Json resources = Json::array();
        for (const auto &binding : declaration.bindings)
        {
            if (binding.stageMask == 0 || (binding.stageMask > 4))
            {
                throw BuildError(ErrorCode::InvalidInput, "Interface snapshot has invalid stage visibility.");
            }
            const auto &resource = binding.resource;
            Json stages = Json::array();
            if ((binding.stageMask & 1u) != 0)
            {
                stages.push_back("vertex");
            }
            if ((binding.stageMask & 2u) != 0)
            {
                stages.push_back("fragment");
            }
            if ((binding.stageMask & 4u) != 0)
            {
                stages.push_back("compute");
            }
            Json value{{"name", resource.name},
                       {"kind", ResourceKindName(resource.kind)},
                       {"set", resource.set},
                       {"slot", resource.slot},
                       {"stages", stages}};
            if (resource.kind == ResourceKind::ConstantBuffer || IsStructured(resource.kind))
            {
                value["block"] = resource.buffer.name;
            }
            if (IsStructured(resource.kind))
            {
                value["capacity"] = resource.capacity;
            }
            if (IsUnordered(resource.kind))
            {
                value["access"] = StorageAccessName(resource.access);
            }
            if (IsStorageImage(resource.kind))
            {
                value["format"] = resource.imageFormat;
            }
            resources.push_back(std::move(value));
        }
        Json ioValues = Json::array();
        for (const auto &io : declaration.io)
        {
            if (io.stage != Stage::Vertex && io.stage != Stage::Fragment)
            {
                throw BuildError(ErrorCode::InvalidInput, "Interface snapshot has an invalid stage.");
            }
            Json value{{"stage", StageName(io.stage)},
                       {"direction", io.input ? "in" : "out"},
                       {"name", io.value.name},
                       {"type", io.value.type},
                       {"location", io.value.location}};
            // 默认项保持省略, 规范快照不会因补写默认值超过原 schema 的字节预算.
            if (io.value.interpolation != "smooth")
            {
                value["interpolation"] = io.value.interpolation;
            }
            ioValues.push_back(std::move(value));
        }
        Json schema{
            {"version", 1}, {"name", declaration.name}, {"blocks", blocks}, {"resources", resources}, {"io", ioValues}};
        if (declaration.localSize[0] != 0)
        {
            schema["localSize"] = declaration.localSize;
        }
        return schema;
    }

    std::string ComputeInterfaceAbi(const InterfaceDeclaration &declaration)
    {
        // 固定字段编码同时服务生成与包边界; 派生量参加身份, 外来快照还须与重新解析的规范值相等.
        Json bindings = Json::array();
        for (const auto &binding : declaration.bindings)
        {
            bindings.push_back({{"resource", binding.resource}, {"stages", binding.stageMask}});
        }
        Json ioValues = Json::array();
        for (const auto &io : declaration.io)
        {
            ioValues.push_back({{"stage", StageName(io.stage)}, {"input", io.input}, {"value", io.value}});
        }
        const RHI::VulkanBindingOffsets offsets;
        Json identity{
            {"version", 1},
            {"name", declaration.name},
            {"blocks", declaration.blocks},
            {"bindings", bindings},
            {"io", ioValues},
            {"offsets", {offsets.shaderResource, offsets.sampler, offsets.constantBuffer, offsets.unorderedAccess}}};
        if (declaration.localSize[0] != 0)
        {
            identity["localSize"] = declaration.localSize;
        }
        return HashText(identity.dump());
    }
}
