#pragma once

// 模块私有接口模型. 解析结果拥有声明和派生布局, 供生成器与最终反射核对共同消费.

#include <cstdint>
#include <string>
#include <vector>

#include "BuildSupport.hpp"

namespace Physara::Shader
{
    struct DataType
    {
        std::string glsl;
        std::string cppScalar;
        std::uint32_t components = 1;
        std::uint32_t columns = 1;
        std::uint32_t scalarKind = 0; // float/int/uint 分别为 0/1/2, 均固定 32 位.
    };

    using BindingSchema = ResourceDeclaration;
    using IoSchema = StageIoDeclaration;

    struct InterfaceSchema
    {
        std::string name;
        std::string abiHash;
        Json canonical;
        std::vector<BufferLayout> blocks;
        std::vector<BindingSchema> bindings;
        std::vector<IoSchema> io;
    };

    [[nodiscard]] DataType ParseDataType(std::string_view name);
    [[nodiscard]] BufferLayout ParseBlockLayout(const Json &json);
    [[nodiscard]] std::uint32_t CanonicalBinding(ResourceKind kind, std::uint32_t slot);
    [[nodiscard]] Stage ParseStage(std::string_view name);
    [[nodiscard]] std::string_view StageName(Stage stage) noexcept;
    [[nodiscard]] ResourceKind ParseResourceKind(std::string_view name);
    [[nodiscard]] std::string_view ResourceKindName(ResourceKind kind) noexcept;
    [[nodiscard]] InterfaceSchema ParseInterfaceSchema(const Json &document);
    [[nodiscard]] const BufferLayout &FindBlock(const InterfaceSchema &schema, std::string_view name);
    [[nodiscard]] InterfaceDeclaration SnapshotInterface(const InterfaceSchema &schema);
    [[nodiscard]] Json DeclarationSchema(const InterfaceDeclaration &declaration);
    [[nodiscard]] std::string ComputeInterfaceAbi(const InterfaceDeclaration &declaration);

    // JSON 只穿越本模块的文件边界. 这些声明让生成预期和包格式复用同一字段编码.
    // to_json/from_json 是 nlohmann_json 通过 ADL 查找的固定钩子名, 改名会使隐式序列化失效.
    // 此处保留第三方协议拼写, 模块自己的函数仍使用 PascalCase.
    void to_json(Json &json, const FieldLayout &value);
    void from_json(const Json &json, FieldLayout &value);
    void to_json(Json &json, const BufferLayout &value);
    void from_json(const Json &json, BufferLayout &value);
    void to_json(Json &json, const ResourceReflection &value);
    void from_json(const Json &json, ResourceReflection &value);
    void to_json(Json &json, const IoReflection &value);
    void from_json(const Json &json, IoReflection &value);
}
