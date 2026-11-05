#pragma once

/// @file
/// @brief 私有 schema 模型与规范化规则, 供生成器, 包读取和最终反射共同消费.

#include <cstdint>
#include <string>
#include <vector>

#include "BuildSupport.hpp"

namespace Physara::Shader
{
    /// @brief 32 位标量/向量/方阵, components 为每列分量数, columns 为列数.
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

    /// @brief 已校验声明和派生布局, ABI 使用语义排序后的快照, canonical 保留原 JSON 声明.
    struct InterfaceSchema
    {
        std::string name;
        std::string abiHash;
        Json canonical;
        std::vector<BufferLayout> blocks;
        std::vector<BindingSchema> bindings;
        std::vector<IoSchema> io;
    };

    /// @brief 将规范类型名解码为 CPU/GLSL 形状, 不支持的名字抛 BuildError.
    [[nodiscard]] DataType ParseDataType(std::string_view name);
    /// @brief 按声明顺序推导 std140/std430 偏移, stride 和块尾对齐, 超预算时拒绝.
    [[nodiscard]] BufferLayout ParseBlockLayout(const Json &json);
    /// @brief 将资源类与 slot 映射到 RHI 固定 Vulkan 分区, 不生成 GL 原生绑定点.
    [[nodiscard]] std::uint32_t CanonicalBinding(ResourceKind kind, std::uint32_t slot);
    /// @brief 阶段/资源种类的严格解析及规范拼写查询, 解析拒绝超 profile 输入.
    [[nodiscard]] Stage ParseStage(std::string_view name);
    [[nodiscard]] std::string_view StageName(Stage stage) noexcept;
    [[nodiscard]] ResourceKind ParseResourceKind(std::string_view name);
    [[nodiscard]] std::string_view ResourceKindName(ResourceKind kind) noexcept;
    /// @brief 校验声明, 生成名称和语义身份冲突, 排序集合并推导完整 ABI.
    [[nodiscard]] InterfaceSchema ParseInterfaceSchema(const Json &document);
    /// @brief 借出指定块, 未声明名字抛 BuildError; 引用不跨 schema 修改或销毁.
    [[nodiscard]] const BufferLayout &FindBlock(const InterfaceSchema &schema, std::string_view name);
    /// @brief 拷贝完整声明为公开值, 包含入口未使用的资源与 IO.
    [[nodiscard]] InterfaceDeclaration SnapshotInterface(const InterfaceSchema &schema);
    /// @brief 从快照恢复规范声明 JSON, 重新解析时独立核对外来派生布局.
    [[nodiscard]] Json DeclarationSchema(const InterfaceDeclaration &declaration);
    /// @brief 对完整声明, 派生布局及固定 binding 分区计算 ABI, 不使用源文本格式或活动裁剪.
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
