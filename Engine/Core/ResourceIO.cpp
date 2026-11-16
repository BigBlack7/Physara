#include "ResourceIO.hpp"

/// @file
/// @brief 用规范化的卷路径比较实际位置, 从同一个只读句柄取得后续解码使用的字节.
/// 写入固定物理目录及临时文件对象, 在最后可取消点之后执行同卷重命名.

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
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
        FileHandle(FileHandle &&other) noexcept : m_Value(std::exchange(other.m_Value, INVALID_HANDLE_VALUE)) {}
        ~FileHandle() { static_cast<void>(Close()); }

        [[nodiscard]] bool Close() noexcept
        {
            return m_Value == INVALID_HANDLE_VALUE ||
                CloseHandle(std::exchange(m_Value, INVALID_HANDLE_VALUE)) != FALSE;
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

    bool ResourceCommit::Cancel() noexcept
    {
        auto expected = Phase::Pending;
        return m_Phase.compare_exchange_strong(expected, Phase::Cancelled) || expected == Phase::Cancelled;
    }

    bool ResourceCommit::IsCancelled() const noexcept
    {
        return m_Phase.load() == Phase::Cancelled;
    }

    bool ResourceCommit::Begin() noexcept
    {
        auto expected = Phase::Pending;
        return m_Phase.compare_exchange_strong(expected, Phase::Committing);
    }

    /// @brief 取得实际目录. 固定阶段禁止沿替换后的组件再次解析到不同目录.
    static FileHandle OpenDirectory(const std::wstring &name, bool follow, bool shareWrite) noexcept
    {
        const auto flags = FILE_FLAG_BACKUP_SEMANTICS | (follow ? DWORD{0} : FILE_FLAG_OPEN_REPARSE_POINT);
        const auto share = FILE_SHARE_READ | (shareWrite ? FILE_SHARE_WRITE : DWORD{0});
        return FileHandle(CreateFileW(name.c_str(), FILE_READ_ATTRIBUTES | FILE_TRAVERSE,
            share, nullptr, OPEN_EXISTING, flags, nullptr));
    }

    /// @brief 核对磁盘目录及当前阶段是否允许重解析点, 不将名字存在视为位置已固定.
    static bool IsDirectory(HANDLE directory, bool follow) noexcept
    {
        BY_HANDLE_FILE_INFORMATION info{};
        return directory != INVALID_HANDLE_VALUE && GetFileType(directory) == FILE_TYPE_DISK &&
            GetFileInformationByHandle(directory, &info) && (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            (follow || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0);
    }

    /// @brief 已解析的目标父目录及各级固定句柄, 阻止写入期间祖先目录被替换或改为重解析点.
    struct WriteDirectory
    {
        std::wstring path;
        std::vector<FileHandle> pins;
    };

    /// @brief 校验物理位置后逐级固定卷内目录, 避免同目录临时文件被路径重定向.
    static std::variant<WriteDirectory, ResourceFailure>
    PinWriteDirectory(const std::filesystem::path &root, const ResourcePath &target)
    {
        const auto name = LocalRoot(root);
        if (name.empty())
        {
            return Fail(ResourceError::InvalidPath, "Invalid project root for writing");
        }
        const auto anchor = OpenDirectory(name, true, true);
        if (!IsDirectory(anchor.Get(), true))
        {
            return Fail(ResourceError::CannotOpen, "Cannot open the project directory for writing");
        }
        auto openedRoot = FinalPath(anchor.Get());
        if (openedRoot.empty())
        {
            return Fail(ResourceError::InvalidPath, "Cannot resolve the project directory for writing");
        }
        if (openedRoot.back() != L'\\')
        {
            openedRoot += L'\\';
        }
        const auto found = OpenDirectory(openedRoot + target.Relative().parent_path().native(), true, true);
        if (!IsDirectory(found.Get(), true))
        {
            return Fail(ResourceError::CannotOpen, "Cannot open the document destination directory");
        }
        auto actual = FinalPath(found.Get());
        const auto allowed = openedRoot + L"Assets\\" + target.Directory();
        if (actual != allowed && !actual.starts_with(allowed + L'\\'))
        {
            return Fail(ResourceError::InvalidPath, "Document destination resolves outside its project directory");
        }

        WriteDirectory result{std::move(actual), {}};
        constexpr std::wstring_view VolumePrefix = L"\\\\?\\Volume{";
        const auto volumeEnd = result.path.find(L'\\', VolumePrefix.size());
        if (!result.path.starts_with(VolumePrefix) || volumeEnd == std::wstring::npos)
        {
            return Fail(ResourceError::InvalidPath, "Document destination requires a local volume");
        }
        auto part = volumeEnd;
        do
        {
            part = result.path.find(L'\\', part + 1);
            auto pin = OpenDirectory(result.path.substr(0, part), false, false);
            if (!IsDirectory(pin.Get(), false))
            {
                return Fail(ResourceError::InvalidPath, "Cannot pin the resolved destination directory");
            }
            result.pins.push_back(std::move(pin));
        } while (part != std::wstring::npos);
        if (FinalPath(result.pins.back().Get()) != result.path || FinalPath(found.Get()) != result.path)
        {
            return Fail(ResourceError::InvalidPath, "Document destination directory changed");
        }
        return result;
    }

    /// @brief 现有文件须为普通文件. 以重解析点本身查询, 替换只操作其所在目录的叶子名字.
    static bool CanReplaceFile(const std::wstring &name)
    {
        const FileHandle check(CreateFileW(name.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (check.Get() == INVALID_HANDLE_VALUE)
        {
            return GetLastError() == ERROR_FILE_NOT_FOUND;
        }
        BY_HANDLE_FILE_INFORMATION info{};
        return GetFileType(check.Get()) == FILE_TYPE_DISK && GetFileInformationByHandle(check.Get(), &info) &&
            (info.dwFileAttributes &
             (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_REPARSE_POINT)) == 0;
    }

    /// @brief 持有临时文件的删除权. 清理使用句柄, 无需通过文件名找回临时文件.
    class TemporaryFile final
    {
    public:
        explicit TemporaryFile(FileHandle handle) : file(std::move(handle)) {}
        ~TemporaryFile()
        {
            if (!published && file.Get() != INVALID_HANDLE_VALUE)
            {
                FILE_DISPOSITION_INFO discard{TRUE};
                SetFileInformationByHandle(file.Get(), FileDispositionInfo, &discard, sizeof(discard));
            }
        }
        FileHandle file;
        bool published{};
    };

    /// @brief 在固定目录独占创建本进程临时文件, 名称冲突只作有限尝试, 不复用已有文件.
    static FileHandle CreateTemporary(const std::wstring &directory)
    {
        static std::atomic<std::uint64_t> serial{0};
        const auto prefix = directory + L"\\.physara-" + std::to_wstring(GetCurrentProcessId()) + L"-";
        for (unsigned attempt = 0; attempt != 8; ++attempt)
        {
            const auto name = prefix + std::to_wstring(serial.fetch_add(1, std::memory_order_relaxed)) + L".tmp";
            const auto file = CreateFileW(name.c_str(), GENERIC_WRITE | DELETE, 0, nullptr,
                                          CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE || GetLastError() != ERROR_FILE_EXISTS)
            {
                return FileHandle(file);
            }
        }
        return FileHandle(INVALID_HANDLE_VALUE);
    }

    /// @brief 句柄重复后保留删除权, 关闭写入权限并核对结果后才进入替换.
    static FileHandle RetainForRename(HANDLE writer) noexcept
    {
        auto process = GetCurrentProcess();
        HANDLE result = INVALID_HANDLE_VALUE;
        if (!DuplicateHandle(process, writer, process, &result, DELETE, FALSE, 0))
        {
            result = INVALID_HANDLE_VALUE;
        }
        return FileHandle(result);
    }

    std::optional<ResourceFailure>
    WriteResourceFile(const std::filesystem::path &root, const ResourcePath &target, std::span<const std::byte> data,
                      ResourceCommit &commit, std::stop_token stop)
    {
        const auto cancelled = [&] { return stop.stop_requested() || commit.IsCancelled(); };
        const auto cancelledFailure = Fail(ResourceError::Cancelled, "Document writing was cancelled");
        if (cancelled())
        {
            return cancelledFailure;
        }
        auto opened = PinWriteDirectory(root, target);
        if (const auto *error = std::get_if<ResourceFailure>(&opened))
        {
            return *error;
        }
        auto &directory = std::get<WriteDirectory>(opened);
        const auto destination = directory.path + L'\\' + target.Relative().filename().native();
        if (destination.size() >= 32768 || !CanReplaceFile(destination))
        {
            return Fail(ResourceError::CannotOpen, "Document destination is not a writable file entry");
        }
        // 使用已固定物理目录下的绝对目标. 不让 Win32 对相对重命名参数使用当前工作目录.
        // 所有分配发生在取得提交资格之前.
        std::vector<std::byte> storage(sizeof(FILE_RENAME_INFO) + destination.size() * sizeof(wchar_t));
        auto *rename = new (storage.data()) FILE_RENAME_INFO{};
        rename->ReplaceIfExists = TRUE;
        rename->FileNameLength = static_cast<DWORD>(destination.size() * sizeof(wchar_t));
        std::memcpy(storage.data() + offsetof(FILE_RENAME_INFO, FileName), destination.data(), rename->FileNameLength);

        TemporaryFile writer(CreateTemporary(directory.path));
        if (writer.file.Get() == INVALID_HANDLE_VALUE)
        {
            return Fail(ResourceError::CannotOpen, "Cannot create an exclusive document temporary file");
        }
        constexpr std::size_t Chunk = 256 * 1024;
        while (!data.empty())
        {
            if (cancelled())
            {
                return cancelledFailure;
            }
            const auto bytes = static_cast<DWORD>(std::min(data.size(), Chunk));
            DWORD written = 0;
            if (!WriteFile(writer.file.Get(), data.data(), bytes, &written, nullptr) || written != bytes)
            {
                return Fail(ResourceError::IncompleteWrite, "Incomplete document temporary write");
            }
            data = data.subspan(bytes);
        }
        if (!FlushFileBuffers(writer.file.Get()))
        {
            return Fail(ResourceError::IncompleteWrite, "Cannot flush the document temporary file");
        }
        TemporaryFile pending(RetainForRename(writer.file.Get()));
        if (pending.file.Get() == INVALID_HANDLE_VALUE)
        {
            return Fail(ResourceError::IncompleteWrite, "Cannot retain the completed temporary file");
        }
        writer.published = true; // 临时文件清理权交给 pending.
        if (!writer.file.Close())
        {
            return Fail(ResourceError::IncompleteWrite, "Cannot close the document writer");
        }
        // 重命名需要在目标父目录取得写权限. 先持有一个允许该写入的目录句柄,
        // 再释放创建临时文件时的排他写限制. 临时文件仍由同一目录内的原句柄独占.
        const auto commitDirectory = OpenDirectory(directory.path, false, true);
        if (!IsDirectory(commitDirectory.Get(), false) || !directory.pins.back().Close())
        {
            return Fail(ResourceError::IncompleteWrite, "Cannot retain the document directory for replacement");
        }
        if (cancelled() || !commit.Begin())
        {
            return cancelledFailure;
        }
        if (!SetFileInformationByHandle(pending.file.Get(), FileRenameInfo, rename,
                                        static_cast<DWORD>(storage.size())))
        {
            const auto native = GetLastError();
            auto failure = Fail(ResourceError::IncompleteWrite, "Cannot publish the document replacement, Win32 ");
            std::array<char, 16> number;
            const auto text = std::to_chars(number.data(), number.data() + number.size(), native);
            failure.description.Append(std::string_view(number.data(), text.ptr));
            return failure;
        }
        pending.published = true;
        return std::nullopt;
    }
}
