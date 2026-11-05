#pragma once

// Shader 私有构建协议: 统一有界文件 I/O、JSON 信任边界和公共结果的异常转换.
// 只由本模块的生成/编译/包实现消费, 不向业务公开 JSON 或原生系统类型.

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

    [[nodiscard]] Failure MakeFailure(ErrorCode code, std::string_view message) noexcept;

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

    // 所有可分配工作在进入 lambda 后执行, 可传播的 OOM 转为有界结果.
    // STL/第三方 noexcept 内的 terminate 无法在这里捕获; 构建驱动提供单独的进程失败边界.
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

    [[nodiscard]] std::string PathText(const std::filesystem::path &path);
    // 构建输入不接受可重定向的 reparse point, 防止 depfile 固定旧物理路径后漏掉目标切换.
    [[nodiscard]] std::filesystem::path CanonicalInputPath(const std::filesystem::path &path);
    [[nodiscard]] std::vector<std::byte> ReadBytes(const std::filesystem::path &path, std::size_t limit);
    [[nodiscard]] std::string ReadText(const std::filesystem::path &path, std::size_t limit);
    [[nodiscard]] Json ParseJson(std::string_view text, std::string_view origin);
    void CheckFields(const Json &object, std::initializer_list<std::string_view> allowed, std::string_view origin);
    [[nodiscard]] std::uint32_t UInt(const Json &value, std::uint32_t maximum, std::string_view origin);
    [[nodiscard]] std::string Identifier(const Json &value, std::string_view origin);
    [[nodiscard]] std::string Sha256(std::span<const std::byte> bytes);
    [[nodiscard]] std::string HashText(std::string_view text);
    // 辅助构建输出可指定发布时间, 在临时文件上设置后才替换, 不让其时间越过最后成功包.
    [[nodiscard]] bool WriteAtomically(const std::filesystem::path &path, std::span<const std::byte> bytes,
                                       std::optional<std::filesystem::file_time_type> outputTime = std::nullopt);
    [[nodiscard]] bool WriteTextAtomically(const std::filesystem::path &path, std::string_view text,
                                           std::optional<std::filesystem::file_time_type> outputTime = std::nullopt);
    [[nodiscard]] bool IsWithin(const std::filesystem::path &root, const std::filesystem::path &file);
}
