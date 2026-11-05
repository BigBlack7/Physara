#pragma once

/// @file
/// @brief Shader 的 CPU 值合同, 包拥有字节码, 反射和源映射, 不依赖图形设备.

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include <Core/Error.hpp>

namespace Physara::Shader
{
    enum class ErrorCode : std::uint8_t
    {
        InvalidInput,
        UnsupportedProfile,
        CompilerUnavailable,
        CompilationFailed,
        ValidationFailed,
        InterfaceMismatch,
        CorruptPackage,
        IoFailure,
        SystemFailure,
        OutOfMemory
    };

    /// @brief Shader 公共边界的自持失败值, code 分类, context 保留有界诊断.
    struct Failure
    {
        ErrorCode code = ErrorCode::InvalidInput;
        Core::ErrorContext context;
    };

    /// @brief 成功值与失败互斥, Failure 自持有界文本, 报告 OOM 无需再次分配正文.
    template <typename T> using Result = std::variant<T, Failure>;

    enum class Stage : std::uint8_t
    {
        Vertex,
        Fragment
    };

    enum class ResourceKind : std::uint8_t
    {
        ConstantBuffer,
        StructuredBuffer,
        Texture2D,
        Sampler
    };

    /// @brief 字段的规范布局, offset/extent/stride 均为字节, type 使用版本 1 规范类型名.
    /// @note extent 包含数组元素或矩阵列的 stride 占位; 非数组/非矩阵的对应 stride 为零.
    struct FieldLayout
    {
        std::string name;
        std::string type;
        std::uint32_t offsetBytes = 0;
        std::uint32_t extentBytes = 0;
        std::uint32_t arrayCount = 1;
        std::uint32_t arrayStrideBytes = 0;
        std::uint32_t matrixStrideBytes = 0;
    };

    /// @brief std140/std430 块布局, fields 保持声明顺序.
    /// sizeBytes 是 extentBytes 向块对齐上取整的分配大小, 也是 record stride; extentBytes 不含块尾 padding.
    struct BufferLayout
    {
        std::string name;
        std::string layout;
        std::uint32_t alignmentBytes = 0;
        std::uint32_t sizeBytes = 0;
        std::uint32_t extentBytes = 0;
        std::vector<FieldLayout> fields;
    };

    /// @brief 活动资源信息, 逻辑 set/slot 与 canonical binding 同时保留, binding 不等同 GL 原生单元.
    /// @note recordStride/capacity 仅表达固定只读 record 数组, 其他资源为零; buffer 仅对缓冲资源有效.
    struct ResourceReflection
    {
        std::string name;
        ResourceKind kind = ResourceKind::ConstantBuffer;
        std::uint32_t set = 0;
        std::uint32_t slot = 0;
        std::uint32_t binding = 0;
        std::uint32_t recordStrideBytes = 0;
        std::uint32_t capacity = 0;
        BufferLayout buffer;
    };

    /// @brief 用户 IO 的 location, 规范类型与最终 IR 插值, 不包含内建变量.
    struct IoReflection
    {
        std::string name;
        std::string type;
        std::string interpolation;
        std::uint32_t location = 0;
    };

    /// @brief 完整资源声明, 可包含入口未使用的资源; stageMask 的 bit 0/1 表示 Vertex/Fragment.
    struct ResourceDeclaration
    {
        ResourceReflection resource;
        std::uint32_t stageMask = 0;
    };

    /// @brief 带阶段与输入方向的完整 IO 声明, 与模块活动反射分开保存.
    struct StageIoDeclaration
    {
        Stage stage = Stage::Vertex;
        bool input = false;
        IoReflection value;
    };

    /// @brief 自持完整 ABI 声明, 与最终活动集合分开保存.
    /// @note 消费者重算声明 ABI 并核对 IR, 不只比较两份外来哈希字符串.
    struct InterfaceDeclaration
    {
        std::string name;
        std::vector<BufferLayout> blocks;
        std::vector<ResourceDeclaration> bindings;
        std::vector<StageIoDeclaration> io;
    };

    /// @brief 受控逻辑路径与原文件字节摘要, 不含机器绝对路径, 用于可重建内容身份.
    struct SourceDependency
    {
        std::string logicalPath;
        std::string contentHash;
    };

    /// @brief 静态 include 声明, line 为原文件一基行号, 同目标不同行分别保存.
    /// expanded 仅表示编译器实际成功处理过此边, 未活动条件分支保持 false.
    struct IncludeEdge
    {
        std::string includingPath;
        std::string includedPath;
        std::uint32_t line = 0;
        bool expanded = false;
    };

    /// @brief 一阶段的最终 SPIR-V 与活动反射, entry/stage 必须和 IR 入口一致.
    /// @note words 满足 uint32_t 对齐, 字节借用仅在拥有者存活且容器未失效期间有效.
    struct ShaderModule
    {
        Stage stage = Stage::Vertex;
        std::string entry;
        std::string sourcePath;
        std::vector<std::uint32_t> words;
        std::vector<ResourceReflection> resources;
        std::vector<IoReflection> inputs;
        std::vector<IoReflection> outputs;
    };

    /// @brief 与编译环境寿命独立的 CPU 包, ABI 标识完整声明及布局, contentHash 标识完整构建输入.
    /// compilerFingerprint 标识工具输入, 配置与选项; 文件另有格式版本和完整性摘要.
    struct ShaderPackage
    {
        std::string profile;
        std::string abiHash;
        std::string contentHash;
        std::string compilerFingerprint;
        InterfaceDeclaration declaration;
        std::vector<ShaderModule> modules;
        std::vector<SourceDependency> dependencies;
        std::vector<IncludeEdge> sourceMap;
    };
}
