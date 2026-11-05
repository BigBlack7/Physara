#pragma once

// Shader 的 CPU 值合同. 包拥有字节码/反射/源映射, 错误复用 Core 的无分配有界文本.

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

    struct Failure
    {
        ErrorCode code = ErrorCode::InvalidInput;
        Core::ErrorContext context;
    };

    // 成功值和失败互斥. Failure 自持文本, 包括内存不足时也不需要分配诊断字符串.
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

    // 所有 offset/extent/stride 均为字节. type 使用版本 1 的规范类型名, 数组数量为正数.
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

    struct BufferLayout
    {
        std::string name;
        std::string layout;
        std::uint32_t alignmentBytes = 0;
        std::uint32_t sizeBytes = 0;
        std::uint32_t extentBytes = 0;
        std::vector<FieldLayout> fields;
    };

    // 逻辑身份与 canonical binding 同时保留, 不表达 GL 原生绑定单元.
    // structured buffer 的 recordStride/capacity 表达固定容量 record 数组, 其它资源为零.
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

    struct IoReflection
    {
        std::string name;
        std::string type;
        std::string interpolation;
        std::uint32_t location = 0;
    };

    // 声明可包含本程序未使用的资源/IO. stageMask 的 bit 0/1 分别表示 Vertex/Fragment.
    struct ResourceDeclaration
    {
        ResourceReflection resource;
        std::uint32_t stageMask = 0;
    };

    struct StageIoDeclaration
    {
        Stage stage = Stage::Vertex;
        bool input = false;
        IoReflection value;
    };

    // 完整 ABI 快照独立于活动反射. 包读取以它重算 ABI 并核对 IR, 不单独信任一个身份字符串.
    struct InterfaceDeclaration
    {
        std::string name;
        std::vector<BufferLayout> blocks;
        std::vector<ResourceDeclaration> bindings;
        std::vector<StageIoDeclaration> io;
    };

    struct SourceDependency
    {
        std::string logicalPath;
        std::string contentHash;
    };

    struct IncludeEdge
    {
        std::string includingPath;
        std::string includedPath;
        std::uint32_t line = 0;
        bool expanded = false;
    };

    // words 存储满足 uint32_t 对齐, 后续消费者可以在拥有者存活时借用其原始字节.
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

    // 与编译环境寿命独立的完整 CPU 包. 发布文件有单独格式版本和完整性检查.
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
