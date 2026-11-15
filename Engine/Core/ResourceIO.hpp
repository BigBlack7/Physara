#pragma once

/// @file
/// @brief 规范化项目相对资源路径, 在实际文件句柄上校验并取得有界字节快照.

#include <filesystem>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <Core/ContentHash.hpp>
#include <Core/Error.hpp>

namespace Physara::Core
{
    enum class ResourceError
    {
        InvalidPath,
        CannotOpen,
        IncompleteRead,
        ByteLimit,
        HashFailed,
        Cancelled
    };

    /// @brief 文件边界的自持失败原因, 不携带打开句柄或对外部路径的借用.
    struct ResourceFailure
    {
        ResourceError reason;
        BoundedText description;
    };

    /// @brief 已完成词法校验的 Assets 相对路径, 持有规范拼写与允许的目录前缀.
    /// @note I/O 时仍须校验打开句柄的实际位置.
    class ResourcePath final
    {
    public:
        /// @brief 规范化相对路径. allowedDirectories 是受调用模块控制的 Assets 下一级目录名.
        /// @return 成功值拥有规范路径; 拒绝绝对路径、流及规范化后不在允许目录内的引用.
        /// @note 这里只做词法校验, 不打开文件; 分配失败交调用方边界处理.
        [[nodiscard]] static std::variant<ResourcePath, ResourceFailure>
        Parse(const std::filesystem::path &reference, std::span<const std::wstring_view> allowedDirectories);

        [[nodiscard]] const std::filesystem::path &Relative() const noexcept { return m_Relative; }
        [[nodiscard]] const std::wstring &Directory() const noexcept { return m_Directory; }

    private:
        ResourcePath(std::filesystem::path relative, std::wstring directory);

        std::filesystem::path m_Relative;
        std::wstring m_Directory;
    };

    /// @brief 同一打开对象的完整字节及其 SHA-256, 解码和内容身份共同使用此快照.
    struct ResourceBytes
    {
        std::vector<std::byte> bytes;
        Sha256Digest fingerprint;
    };

    /// @brief 从绝对的本地盘符资源根打开普通文件, 校验打开对象后再读正文.
    /// @return 完整字节和其摘要, 或有界错误. 返回前关闭文件, 不交付部分字节.
    /// @note 同步 I/O 由 CPU worker 调用, 支持读块与摘要前的合作取消. 分配失败可传播.
    /// 文件只共享读取, 不允许并发写入或删除; 共享冲突作为 CannotOpen 交回, 此入口不重试.
    [[nodiscard]] std::variant<ResourceBytes, ResourceFailure>
    ReadResourceFile(const std::filesystem::path &root, const ResourcePath &source, std::size_t maxBytes,
                     std::stop_token stop = {});
}
