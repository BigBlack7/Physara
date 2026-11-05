#pragma once

/// @file
/// @brief Shader 私有构建支持: 有界 I/O, JSON 信任边界, 摘要和发布, 不向业务公开 JSON/系统类型.

#include <cstddef>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "Types.hpp"

namespace Physara::Shader
{
    using Json = nlohmann::json;
    inline constexpr std::size_t MaxDeclarationBytes = 1024 * 1024;
    inline constexpr std::size_t MaxSourceBytes = 8 * 1024 * 1024;
    inline constexpr std::size_t MaxPackageBytes = 64 * 1024 * 1024;
    inline constexpr std::uint32_t MaxBlockBytes = 1024 * 1024;

    /// @brief 无动态分配地形成自持错误, 超长正文由 Core 字节预算截断.
    [[nodiscard]] Failure MakeFailure(ErrorCode code, std::string_view message) noexcept;

    /// @brief 模块内部失败异常, 公共入口由 GuardBuild 转回 Failure.
    class BuildError final : public std::exception
    {
      public:
        BuildError(ErrorCode code, std::string_view message) noexcept : m_Failure(MakeFailure(code, message))
        {
        }
        [[nodiscard]] const char *what() const noexcept override
        {
            return m_Failure.context.description.View().data();
        }
        [[nodiscard]] const Failure &GetFailure() const noexcept
        {
            return m_Failure;
        }

      private:
        Failure m_Failure;
    };

    /// @brief 将可传播异常统一转成自持 Failure, 不重试或伪造成功值.
    /// @pre 可分配工作在 function 内执行, 参数构造时的异常不在此捕获范围.
    /// @note STL/第三方 noexcept 内的 terminate 无法捕获, 由构建进程有界报告并失败退出.
    template <typename T, typename Function> Result<T> GuardBuild(Function &&function) noexcept
    {
        try
        {
            return function();
        }
        catch (const BuildError &error)
        {
            return error.GetFailure();
        }
        catch (const std::bad_alloc &)
        {
            return MakeFailure(ErrorCode::OutOfMemory, "Shader CPU allocation failed.");
        }
        catch (const std::exception &error)
        {
            return MakeFailure(ErrorCode::InvalidInput, error.what());
        }
        catch (...)
        {
            return MakeFailure(ErrorCode::SystemFailure, "Unexpected Shader dependency failure.");
        }
    }

    /// @brief 将原生路径编码为通用 UTF-8 文本, 供身份与诊断使用.
    [[nodiscard]] std::string PathText(const std::filesystem::path &path);
    /// @brief 逐组件拒绝 reparse point 后取得规范路径, 防止 depfile 固定旧目标而漏掉重定向.
    [[nodiscard]] std::filesystem::path CanonicalInputPath(const std::filesystem::path &path);
    /// @brief 按字节预算读取完整文件, 不可读, 超限或短读时抛 BuildError.
    [[nodiscard]] std::vector<std::byte> ReadBytes(const std::filesystem::path &path, std::size_t limit);
    /// @brief 保留原文件字节为字符串, 不转换编码或规范化换行, 使用同一读取预算.
    [[nodiscard]] std::string ReadText(const std::filesystem::path &path, std::size_t limit);
    /// @brief 完整解析 JSON, 拒绝原始 NUL, 重复 key 与超深嵌套, origin 用于错误定位.
    [[nodiscard]] Json ParseJson(std::string_view text, std::string_view origin);
    /// @brief 拒绝非对象或未声明字段; 必填字段由消费者的 at 访问另行要求.
    void CheckFields(const Json &object, std::initializer_list<std::string_view> allowed, std::string_view origin);
    /// @brief 提取闭区间 [0, maximum] 内的整数, 拒绝负数与浮点数.
    [[nodiscard]] std::uint32_t UInt(const Json &value, std::uint32_t maximum, std::string_view origin);
    /// @brief 校验共享 C++/GLSL ASCII 标识符及保留字, 名称冲突由 schema 层处理.
    [[nodiscard]] std::string Identifier(const Json &value, std::string_view origin);
    /// @brief 对原始字节计算小写十六进制 SHA-256, HashText 不额外规范化文本.
    [[nodiscard]] std::string Sha256(std::span<const std::byte> bytes);
    [[nodiscard]] std::string HashText(std::string_view text);
    /// @brief 原子替换文件, 字节相同则保留文件; 失败抛异常并保留旧文件内容.
    /// @param outputTime 替换前在临时文件设置的时间, 相同字节也会修正旧文件时间.
    /// @return true 表示替换内容, false 表示字节相同, 不表示时间一定未变化.
    [[nodiscard]] bool WriteAtomically(const std::filesystem::path &path, std::span<const std::byte> bytes,
                                       std::optional<std::filesystem::file_time_type> outputTime = std::nullopt);
    /// @brief 对文本原字节应用 WriteAtomically 的发布与时间合同, 不转换编码或换行.
    [[nodiscard]] bool WriteTextAtomically(const std::filesystem::path &path, std::string_view text,
                                           std::optional<std::filesystem::file_time_type> outputTime = std::nullopt);
    /// @brief 按 Windows 不区分大小写的路径组件判断包含关系, 调用方先规范化实际路径.
    [[nodiscard]] bool IsWithin(const std::filesystem::path &root, const std::filesystem::path &file);
}
