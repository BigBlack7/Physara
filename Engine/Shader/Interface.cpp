#include "Interface.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>

#include "ShaderSchema.hpp"

namespace Physara::Shader
{
    static void EmitPadding(std::ostringstream &output, std::uint32_t size, std::uint32_t &index,
                            std::string_view indent)
    {
        if (size != 0)
        {
            output << indent << "std::array<std::byte, " << size << "> physaraPadding" << index++ << "{};\n";
        }
    }

    static std::string EmitFieldType(std::ostringstream &output, const FieldLayout &field)
    {
        const auto data = ParseDataType(field.type);
        std::string name = field.name;
        name.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())));
        auto type = data.components == 1
                        ? data.cppScalar
                        : "std::array<" + data.cppScalar + ", " + std::to_string(data.components) + ">";
        std::uint32_t elementBytes = data.components * 4;
        if (data.columns > 1)
        {
            // 列与数组元素以明确 padding 保持 stride, 外层 block 的 alignas 保证实际地址对齐.
            const auto column = name + "Column";
            output << "        struct " << column << "\n        {\n            " << type << " value{};\n";
            std::uint32_t padding = 0;
            EmitPadding(output, field.matrixStrideBytes - elementBytes, padding, "            ");
            output << "        };\n        static_assert(sizeof(" << column << ") == " << field.matrixStrideBytes
                   << ");\n";
            type = "std::array<" + column + ", " + std::to_string(data.columns) + ">";
            elementBytes = field.matrixStrideBytes * data.columns;
        }
        if (field.arrayStrideBytes != 0)
        {
            const auto element = name + "Element";
            output << "        struct " << element << "\n        {\n            " << type << " value{};\n";
            std::uint32_t padding = 0;
            EmitPadding(output, field.arrayStrideBytes - elementBytes, padding, "            ");
            output << "        };\n        static_assert(sizeof(" << element << ") == " << field.arrayStrideBytes
                   << ");\n";
            type = "std::array<" + element + ", " + std::to_string(field.arrayCount) + ">";
        }
        return type;
    }

    static std::string GenerateDataHeader(const InterfaceSchema &schema)
    {
        std::ostringstream output;
        output << "#pragma once\n\n// 自动生成的 CPU Shader 数据合同. 修改权威 schema 后重新生成.\n\n"
                  "#include <array>\n#include <cstddef>\n#include <cstdint>\n#include <limits>\n"
                  "#include <string_view>\n#include <type_traits>\n\n"
                  "namespace Physara::Shader::Generated::"
               << schema.name
               << "\n{\n"
                  "    inline constexpr std::string_view AbiHash = \""
               << schema.abiHash
               << "\";\n"
                  "    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);\n";
        for (const auto &block : schema.blocks)
        {
            output << "\n    // " << block.layout << ": 实际字段 extent 与分配尾 padding 分别表达.\n"
                   << "    struct alignas(" << block.alignmentBytes << ") " << block.name << "\n    {\n";
            std::vector<std::string> types;
            for (const auto &field : block.fields)
            {
                types.push_back(EmitFieldType(output, field));
            }
            std::uint32_t cursor = 0;
            std::uint32_t padding = 0;
            for (std::size_t index = 0; index < block.fields.size(); ++index)
            {
                const auto &field = block.fields[index];
                EmitPadding(output, field.offsetBytes - cursor, padding, "        ");
                output << "        " << types[index] << " " << field.name << "{};\n";
                cursor = field.offsetBytes + field.extentBytes;
            }
            EmitPadding(output, block.sizeBytes - cursor, padding, "        ");
            output << "    };\n    static_assert(std::is_standard_layout_v<" << block.name << "> &&\n"
                   << "                  std::is_trivially_copyable_v<" << block.name << ">);\n"
                   << "    static_assert(sizeof(" << block.name << ") == " << block.sizeBytes << ");\n"
                   << "    static_assert(alignof(" << block.name << ") == " << block.alignmentBytes << ");\n";
            for (const auto &field : block.fields)
            {
                output << "    static_assert(offsetof(" << block.name << ", " << field.name
                       << ") == " << field.offsetBytes << ");\n"
                       << "    static_assert(sizeof(" << block.name << "::" << field.name
                       << ") == " << field.extentBytes << ");\n";
            }
        }
        for (const auto &binding : schema.bindings)
        {
            if (binding.resource.kind == ResourceKind::StructuredBuffer)
            {
                auto name = binding.resource.name;
                name.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())));
                output << "\n    inline constexpr std::size_t " << name << "Capacity = " << binding.resource.capacity
                       << ";\n    using " << name << "Data = std::array<" << binding.resource.buffer.name << ", "
                       << name << "Capacity>;\n";
            }
        }
        output << "}\n";
        return output.str();
    }

    static std::string_view RhiKind(ResourceKind kind) noexcept
    {
        switch (kind)
        {
        case ResourceKind::ConstantBuffer:
            return "ConstantBuffer";
        case ResourceKind::StructuredBuffer:
            return "StructuredBuffer_SRV";
        case ResourceKind::Texture2D:
            return "Texture_SRV";
        case ResourceKind::Sampler:
            return "Sampler";
        }
        return "invalid";
    }

    static std::string GenerateBindingHeader(const InterfaceSchema &schema)
    {
        // RHI 的 visibility 属于整个集合. 此处取并集, 每项各自的可见阶段仍由 schema 和分阶段 GLSL 表达.
        std::map<std::uint32_t, std::uint32_t> sets;
        for (const auto &binding : schema.bindings)
        {
            sets[binding.resource.set] |= binding.stageMask;
        }
        std::ostringstream output;
        output << "#pragma once\n\n// 自动生成的逻辑集合描述. visibility 为集合内实际声明可见性的并集.\n\n"
                  "#include \"Data.hpp\"\n\n#include <array>\n\n#include <RHI.hpp>\n\n"
                  "namespace Physara::Shader::Generated::"
               << schema.name << "\n{\n"
               << "    inline std::array<::Physara::RHI::BindingLayoutDesc, " << sets.size()
               << "> MakeBindingLayouts()\n"
                  "    {\n        std::array<::Physara::RHI::BindingLayoutDesc, "
               << sets.size() << "> result{};\n";
        std::size_t index = 0;
        for (const auto &[set, visibility] : sets)
        {
            output << "        result[" << index << "].SetRegisterSpaceAndDescriptorSet(" << set << ");\n"
                   << "        result[" << index << "].SetVisibility(";
            if (visibility == 1)
            {
                output << "::Physara::RHI::ShaderType::Vertex";
            }
            else if (visibility == 2)
            {
                output << "::Physara::RHI::ShaderType::Pixel";
            }
            else
            {
                output << "::Physara::RHI::ShaderType::Vertex | ::Physara::RHI::ShaderType::Pixel";
            }
            output << ");\n";
            for (const auto &binding : schema.bindings)
            {
                if (binding.resource.set == set)
                {
                    output << "        result[" << index
                           << "].AddItem(::Physara::RHI::BindingLayoutItem::" << RhiKind(binding.resource.kind) << "("
                           << binding.resource.slot << "));\n";
                }
            }
            ++index;
        }
        output << "        return result;\n    }\n}\n";
        return output.str();
    }

    static void EmitGlslFields(std::ostringstream &output, const BufferLayout &block)
    {
        for (const auto &field : block.fields)
        {
            output << "    " << ParseDataType(field.type).glsl << " " << field.name;
            if (field.arrayStrideBytes != 0)
            {
                output << "[" << field.arrayCount << "]";
            }
            output << ";\n";
        }
    }

    static std::string GenerateGlsl(const InterfaceSchema &schema, Stage stage)
    {
        std::ostringstream output;
        output << "// 自动生成的 canonical Vulkan GLSL 接口, 算法由薄入口与共享函数实现.\n";
        for (const auto &block : schema.blocks)
        {
            if (block.layout == "std430")
            {
                output << "struct " << block.name << "\n{\n";
                EmitGlslFields(output, block);
                output << "};\n";
            }
        }
        for (const auto &binding : schema.bindings)
        {
            if ((binding.stageMask & (1u << static_cast<std::uint32_t>(stage))) == 0)
            {
                continue;
            }
            const auto &resource = binding.resource;
            output << "layout(set = " << resource.set << ", binding = " << resource.binding;
            if (resource.kind == ResourceKind::Texture2D || resource.kind == ResourceKind::Sampler)
            {
                output << ") uniform " << (resource.kind == ResourceKind::Texture2D ? "texture2D" : "sampler") << " "
                       << resource.name << ";\n";
                continue;
            }
            auto blockName = resource.name;
            blockName.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(blockName.front())));
            output << ", " << resource.buffer.layout << ") ";
            if (resource.kind == ResourceKind::StructuredBuffer)
            {
                output << "readonly buffer " << blockName << "Block\n{\n    " << resource.buffer.name << " records["
                       << resource.capacity << "];\n";
            }
            else
            {
                output << "uniform " << blockName << "Block\n{\n";
                EmitGlslFields(output, resource.buffer);
            }
            output << "} " << resource.name << ";\n";
        }
        for (const auto &io : schema.io)
        {
            if (io.stage != stage)
            {
                continue;
            }
            output << "layout(location = " << io.value.location << ") ";
            if ((stage == Stage::Vertex && !io.input) || (stage == Stage::Fragment && io.input))
            {
                output << io.value.interpolation << " ";
            }
            output << (io.input ? "in " : "out ") << ParseDataType(io.value.type).glsl << " " << io.value.name << ";\n";
        }
        return output.str();
    }

    Result<GeneratedInterface> GenerateInterface(const InterfaceRequest &request) noexcept
    {
        return GuardBuild<GeneratedInterface>(
            [&]
            {
                const auto source = ReadText(CanonicalInputPath(request.schemaPath), MaxDeclarationBytes);
                const auto document = ParseJson(source, PathText(request.schemaPath));
                const auto schema = ParseInterfaceSchema(document);
                Json bindings = Json::array();
                for (const auto &binding : schema.bindings)
                {
                    bindings.push_back({{"resource", binding.resource}, {"stages", binding.stageMask}});
                }
                const Json expected{{"schema", schema.canonical},
                                    {"abiHash", schema.abiHash},
                                    {"blocks", schema.blocks},
                                    {"bindings", bindings}};
                const std::map<std::string, std::string> artifacts{
                    {"Data.hpp", GenerateDataHeader(schema)},
                    {"Bindings.hpp", GenerateBindingHeader(schema)},
                    {"Vertex.glsl", GenerateGlsl(schema, Stage::Vertex)},
                    {"Fragment.glsl", GenerateGlsl(schema, Stage::Fragment)},
                    {"Expected.json", expected.dump(2) + "\n"}};
                Json hashes = Json::object();
                GeneratedInterface result{schema.name, schema.abiHash, {}};
                // 每份内容先完成计算. 文件逐一替换后最后发布 manifest, 旧 manifest 的摘要会拒绝混合代.
                for (const auto &[name, content] : artifacts)
                {
                    hashes[name] = HashText(content);
                    const auto path = request.outputDirectory / name;
                    (void)WriteTextAtomically(path, content);
                    result.outputs.push_back(path);
                }
                const Json manifest{{"version", 1},
                                    {"name", schema.name},
                                    {"abiHash", schema.abiHash},
                                    {"schemaHash", HashText(source)},
                                    {"artifacts", hashes}};
                const auto manifestPath = request.outputDirectory / "Manifest.json";
                (void)WriteTextAtomically(manifestPath, manifest.dump(2) + "\n");
                result.outputs.push_back(manifestPath);
                return result;
            });
    }
}
