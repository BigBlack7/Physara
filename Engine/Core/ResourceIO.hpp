#pragma once

/// @file
/// @brief 规范化项目相对资源路径, 在实际文件句柄上校验读取, 并在受控目录安全替换文件.

#include <atomic>
#include <filesystem>
#include <optional>
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
        IncompleteWrite,
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

    /// @brief 文件替换与取消的一次性裁决. 取得提交资格后只能报告实际文件结果.
    class ResourceCommit final
    {
    public:
        /// @brief 撤销尚未取得的提交资格. 已取消时仍返回 true, 已进入提交时返回 false.
        [[nodiscard]] bool Cancel() noexcept;
        [[nodiscard]] bool IsCancelled() const noexcept;
        /// @brief writer 在最后可取消点竞争一次提交资格, 失败时不得替换目标.
        [[nodiscard]] bool Begin() noexcept;

    private:
        enum class Phase { Pending, Cancelled, Committing };
        std::atomic<Phase> m_Phase{Phase::Pending};
    };

    /// @brief 写入已校验目录的独占临时文件, 刷新并关闭写入句柄后按同目录替换.
    /// @note 同步 worker 入口. 失败或提交前取消保留目标, 清理只作用于本次取得的临时句柄.
    /// 目标父目录须已存在; data 仅在调用中借用, 编码与大小预算由调用方确定.
    /// @return 空值表示实际替换成功. commit 只用于这一次写入, 取得资格后不再接受取消.
    [[nodiscard]] std::optional<ResourceFailure>
    WriteResourceFile(const std::filesystem::path &root, const ResourcePath &target, std::span<const std::byte> data,
                      ResourceCommit &commit, std::stop_token stop = {});
}
