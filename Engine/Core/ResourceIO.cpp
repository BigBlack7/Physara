#include "ResourceIO.hpp"

/// @file
/// @brief 用规范化的卷路径比较实际位置, 从同一个只读句柄取得后续解码使用的字节.

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace Physara::Core
{
    /// @brief 保持读取错误与路径和预算错误独立, 不在 worker 持有日志对象.
    static ResourceFailure Fail(ResourceError reason, std::string_view description) noexcept
    {
        return {reason, BoundedText(description)};
    }

    /// @brief 排除设备命名空间输入、NTFS 流及会被 Win32 改写结尾的普通组件.
    static bool IsFileComponent(std::wstring_view component)
    {
        return !component.empty() && component.back() != L'.' && component.back() != L' ' &&
            std::none_of(component.begin(), component.end(), [](wchar_t ch)
            {
                return ch < L' ' || std::wstring_view(L"<>:\"|?*").find(ch) != std::wstring_view::npos;
            });
    }

    ResourcePath::ResourcePath(std::filesystem::path relative, std::wstring directory)
        : m_Relative(std::move(relative)), m_Directory(std::move(directory))
    {
    }

    std::variant<ResourcePath, ResourceFailure>
    ResourcePath::Parse(const std::filesystem::path &reference, std::span<const std::wstring_view> allowedDirectories)
    {
        if (reference.empty() || reference.native().size() >= 32768 ||
            reference.has_root_path() || !reference.has_filename())
        {
            return Fail(ResourceError::InvalidPath, "Asset source must be a project-relative file path");
        }
        for (const auto &part : reference)
        {
            if (part != L"." && part != L".." && !IsFileComponent(part.native()))
            {
                return Fail(ResourceError::InvalidPath, "Asset source has a non-file path component");
            }
        }

        // 先验证原始组件, 再消去 . 和 .., 依据最终路径选择业务允许的目录.
        auto normal = reference.lexically_normal().make_preferred();
        auto part = normal.begin();
        if (part == normal.end() || *part != L"Assets" || ++part == normal.end())
        {
            return Fail(ResourceError::InvalidPath, "Asset source is outside Assets");
        }

        const auto allowed = std::ranges::find(allowedDirectories, std::wstring_view(part->native()));
        if (allowed == allowedDirectories.end() || ++part == normal.end() || !normal.has_filename())
        {
            return Fail(ResourceError::InvalidPath, "Asset source is outside the allowed asset directories");
        }
        return ResourcePath(std::move(normal), std::wstring(*allowed));
    }

    /// @brief 从 CreateFileW 获得的单个句柄, 自动在失败或取消时关闭.
    class FileHandle final
    {
    public:
        explicit FileHandle(HANDLE value) noexcept : m_Value(value) {}
        FileHandle(const FileHandle &) = delete;
        FileHandle &operator=(const FileHandle &) = delete;
        ~FileHandle()
        {
            if (m_Value != INVALID_HANDLE_VALUE)
            {
                CloseHandle(m_Value);
            }
        }

        [[nodiscard]] HANDLE Get() const noexcept { return m_Value; }

    private:
        HANDLE m_Value;
    };

    /// @brief 给本地绝对盘符路径保留宽字符与长路径语义. 空返回值表示根不在当前本地 profile 内.
    static std::wstring LocalRoot(const std::filesystem::path &path)
    {
        const auto preferred = path.lexically_normal().make_preferred();
        constexpr std::wstring_view Extended = L"\\\\?\\";
        std::wstring_view value = preferred.native();
        if (value.starts_with(Extended))
        {
            value.remove_prefix(Extended.size());
        }
        if (value.size() < 3 || value[1] != L':' || value[2] != L'\\' ||
            !((value[0] >= L'A' && value[0] <= L'Z') || (value[0] >= L'a' && value[0] <= L'z')) ||
            value.find(L'\0') != std::wstring_view::npos)
        {
            return {};
        }
        auto local = std::wstring(Extended);
        local.append(value);
        return local;
    }

    /// @brief 取得文件系统解析过的路径, 比较时使用相同卷标识和实际组件拼写.
    static std::wstring FinalPath(HANDLE file)
    {
        std::array<wchar_t, 32768> buffer;
        const auto size = GetFinalPathNameByHandleW(file, buffer.data(), static_cast<DWORD>(buffer.size()),
                                                   FILE_NAME_NORMALIZED | VOLUME_NAME_GUID);
        if (size == 0 || size >= buffer.size())
        {
            return {};
        }
        return std::wstring(buffer.data(), size);
    }

    /// @brief 读取前核对实际句柄位置与长度. 成功快照的最后一个字节仍来自原打开对象.
    static std::variant<std::vector<std::byte>, ResourceFailure>
    ReadBytes(const std::filesystem::path &root, const ResourcePath &source, std::size_t maxBytes,
              std::stop_token stop)
    {
        const auto rootName = LocalRoot(root);
        if (rootName.empty())
        {
            return Fail(ResourceError::InvalidPath, "Asset root must be an absolute local drive directory");
        }
        const FileHandle anchor(CreateFileW(rootName.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        BY_HANDLE_FILE_INFORMATION rootInfo{};
        if (anchor.Get() == INVALID_HANDLE_VALUE ||
            !GetFileInformationByHandle(anchor.Get(), &rootInfo) ||
            (rootInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            return Fail(ResourceError::CannotOpen, "Cannot open the project resource directory");
        }
        auto openedRoot = FinalPath(anchor.Get());
        if (openedRoot.empty())
        {
            return Fail(ResourceError::InvalidPath, "Cannot resolve the project resource directory");
        }
        if (openedRoot.back() != L'\\')
        {
            openedRoot += L'\\';
        }

        // 使用已解析的根. 源经重解析后仍需位于完整允许目录的组件边界内.
        const auto fileName = openedRoot + source.Relative().native();
        const FileHandle file(CreateFileW(fileName.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        if (file.Get() == INVALID_HANDLE_VALUE)
        {
            return Fail(ResourceError::CannotOpen, "Cannot open the asset source for a read-only snapshot");
        }
        BY_HANDLE_FILE_INFORMATION info{};
        const auto allowed = openedRoot + L"Assets\\" + source.Directory() + L"\\";
        if (GetFileType(file.Get()) != FILE_TYPE_DISK || !GetFileInformationByHandle(file.Get(), &info) ||
            (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE)) != 0)
        {
            return Fail(ResourceError::InvalidPath, "Opened asset source is not a regular file");
        }
        // 词法校验不能证明 reparse point 的落点. 以下判断针对实际将被 ReadFile 使用的句柄.
        const auto resolved = FinalPath(file.Get());
        if (!resolved.starts_with(allowed) || resolved.find(L':', allowed.size()) != std::wstring::npos)
        {
            return Fail(ResourceError::InvalidPath, "Opened source is not a regular file in the allowed directory");
        }

        const auto size = (static_cast<std::uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
        if (size > maxBytes || size > std::numeric_limits<ULONG>::max())
        {
            return Fail(ResourceError::ByteLimit, "Asset source exceeds the byte budget");
        }
        if (stop.stop_requested())
        {
            return Fail(ResourceError::Cancelled, "Asset source reading was cancelled");
        }

        // 小块读取使取消可以在底层调用之间生效. 用完整长度拒绝短读.
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        constexpr std::size_t ChunkSize = 256 * 1024;
        for (std::size_t offset = 0; offset < bytes.size();)
        {
            if (stop.stop_requested())
            {
                return Fail(ResourceError::Cancelled, "Asset source reading was cancelled");
            }
            const auto chunk = static_cast<DWORD>(std::min(ChunkSize, bytes.size() - offset));
            DWORD read = 0;
            if (!ReadFile(file.Get(), bytes.data() + offset, chunk, &read, nullptr) || read != chunk)
            {
                return Fail(ResourceError::IncompleteRead, "Cannot read all bytes of the asset source");
            }
            offset += read;
        }
        return bytes;
    }

    std::variant<ResourceBytes, ResourceFailure>
    ReadResourceFile(const std::filesystem::path &root, const ResourcePath &source, std::size_t maxBytes,
                     std::stop_token stop)
    {
        if (stop.stop_requested())
        {
            return Fail(ResourceError::Cancelled, "Asset source reading was cancelled");
        }
        auto read = ReadBytes(root, source, maxBytes, stop);
        if (const auto *error = std::get_if<ResourceFailure>(&read))
        {
            return *error;
        }
        if (stop.stop_requested())
        {
            return Fail(ResourceError::Cancelled, "Asset source hashing was cancelled");
        }

        // hash 和随后交付的载荷复用这一份已完整读取的缓冲区, 不按路径再次读取源文件.
        auto bytes = std::move(std::get<std::vector<std::byte>>(read));
        const auto hash = ComputeSha256(bytes);
        if (const auto *digest = std::get_if<Sha256Digest>(&hash))
        {
            return ResourceBytes{std::move(bytes), *digest};
        }
        return Fail(ResourceError::HashFailed, "Cannot fingerprint the asset source");
    }
}
