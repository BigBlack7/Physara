#include "FileSystem.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <cctype>

namespace Physara::Platform
{
    namespace Internal
    {
        std::string ToLower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        std::string ToGenericString(const std::filesystem::path &path)
        {
            return path.lexically_normal().generic_string();
        }

        std::string NormalizePathString(std::string_view path)
        {
            return ToGenericString(std::filesystem::path(path));
        }

        // 将路径规范化为统一的比较格式。
        std::string NormalizeForCompareString(std::string_view path)
        {
            std::string s = NormalizePathString(path);

            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        std::string NormalizeDirectoryForCompare(std::string_view path)
        {
            std::string s = NormalizeForCompareString(path);
            if (!s.empty() && s.back() != '/')
            {
                s.push_back('/');
            }

            return s;
        }

        // 判断target路径是否是root路径的子路径
        bool IsSubPathString(std::string_view root, std::string_view target)
        {
            const std::string rootNorm = NormalizeDirectoryForCompare(root);
            const std::string targetNorm = NormalizeDirectoryForCompare(target);

            return targetNorm.rfind(rootNorm, 0) == 0; // 检查target是否以root开头
        }

        // 将任意路径转换为绝对路径, 并确保该路径位于指定的资源根目录内
        std::filesystem::path ToAbsolutePath(std::string_view path, const std::string &assetsRoot)
        {
            if (assetsRoot.empty())
            {
                throw std::runtime_error("FileSystem is not initialized. Call FileSystem::Init first.");
            }

            // 规范化根路径, 消除.和..
            const std::filesystem::path rootPath = std::filesystem::absolute(std::filesystem::path(assetsRoot)).lexically_normal();
            const std::filesystem::path inPath(path);

            std::filesystem::path absPath;
            if (inPath.is_absolute())
            {
                absPath = inPath.lexically_normal();
            }
            else
            {
                absPath = (rootPath / inPath).lexically_normal();
            }

            if (!IsSubPathString(rootPath.generic_string(), absPath.generic_string())) // 限制所有路径必须位于Assets根目录内
            {
                throw std::runtime_error("Path escapes assets root: " + absPath.string());
            }

            return absPath;
        }
    }

    std::string FileSystem::s_AssetsRootPath;

    void FileSystem::Init(std::string assetsRootPath)
    {
        if (assetsRootPath.empty())
        {
            throw std::invalid_argument("FileSystem::Init received empty assets root path.");
        }

        std::filesystem::path root(assetsRootPath);
        root = std::filesystem::absolute(root).lexically_normal();

        if (!std::filesystem::exists(root))
        {
            throw std::runtime_error("Assets root does not exist: " + root.string());
        }

        if (!std::filesystem::is_directory(root))
        {
            throw std::runtime_error("Assets root is not a directory: " + root.string());
        }

        s_AssetsRootPath = Internal::ToGenericString(root);
    }

    std::string FileSystem::ResolvePath(std::string_view relativePath)
    {
        const std::filesystem::path absPath = Internal::ToAbsolutePath(relativePath, s_AssetsRootPath);
        return Internal::ToGenericString(absPath);
    }

    std::string FileSystem::NormalizePath(std::string_view path)
    {
        return Internal::NormalizePathString(path);
    }

    std::string FileSystem::NormalizeForCompare(std::string_view path)
    {
        return Internal::NormalizeForCompareString(path);
    }

    bool FileSystem::IsSamePath(std::string_view lhs, std::string_view rhs)
    {
        return NormalizeForCompare(lhs) == NormalizeForCompare(rhs);
    }

    bool FileSystem::IsSubPath(std::string_view root, std::string_view target)
    {
        return Internal::IsSubPathString(root, target);
    }

    std::string FileSystem::ClampToRoot(std::string_view path, std::string_view root)
    {
        const std::string normalizedPath = NormalizePath(path);
        const std::string normalizedRoot = NormalizePath(root);
        return IsSubPath(normalizedRoot, normalizedPath) ? normalizedPath : normalizedRoot;
    }

    std::string FileSystem::ToAssetsRelativePath(std::string_view path)
    {
        std::filesystem::path input(path);
        if (input.empty())
        {
            return {};
        }

        input = input.lexically_normal();
        if (!input.is_absolute())
        {
            const auto firstPart = input.begin();
            if (firstPart != input.end() && Internal::ToLower(firstPart->generic_string()) == "assets")
            {
                std::filesystem::path stripped;
                bool skipFirst = true;
                for (const std::filesystem::path &part : input)
                {
                    if (skipFirst)
                    {
                        skipFirst = false;
                        continue;
                    }
                    stripped /= part;
                }
                input = stripped.lexically_normal();
            }
        }

        if (!s_AssetsRootPath.empty())
        {
            const std::filesystem::path assetsRoot = std::filesystem::absolute(std::filesystem::path(s_AssetsRootPath)).lexically_normal();
            const std::filesystem::path candidate = input.is_absolute()
                                                        ? input
                                                        : (assetsRoot / input).lexically_normal();

            if (Internal::IsSubPathString(assetsRoot.generic_string(), candidate.generic_string()))
            {
                std::error_code error{};
                const std::filesystem::path relative = std::filesystem::relative(candidate, assetsRoot, error);
                if (!error && !relative.empty() && relative != ".")
                {
                    return Internal::ToGenericString(relative);
                }
            }
        }

        return Internal::ToGenericString(input);
    }

    std::string FileSystem::GetExtension(std::string_view path)
    {
        return std::filesystem::path(path).extension().string();
    }

    std::string FileSystem::GetExtensionLower(std::string_view path)
    {
        return Internal::ToLower(GetExtension(path));
    }

    std::string FileSystem::GetFileName(std::string_view path)
    {
        return std::filesystem::path(path).filename().string();
    }

    std::string FileSystem::GetParentDir(std::string_view path)
    {
        const std::filesystem::path absPath = Internal::ToAbsolutePath(path, s_AssetsRootPath);
        return Internal::ToGenericString(absPath.parent_path());
    }

    std::string FileSystem::GetAssetsRootPath()
    {
        return s_AssetsRootPath;
    }

    std::vector<std::uint8_t> FileSystem::ReadBinaryFile(std::string_view path)
    {
        const std::filesystem::path absPath = Internal::ToAbsolutePath(path, s_AssetsRootPath);

        std::ifstream file(absPath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open binary file: " + absPath.string());
        }

        const std::streamsize size = file.tellg();
        if (size < 0)
        {
            throw std::runtime_error("Failed to get file size: " + absPath.string());
        }

        std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size));
        file.seekg(0, std::ios::beg);

        if (size > 0 && !file.read(reinterpret_cast<char *>(buffer.data()), size))
        {
            throw std::runtime_error("Failed to read binary file: " + absPath.string());
        }

        return buffer;
    }

    std::string FileSystem::ReadTextFile(std::string_view path)
    {
        const std::filesystem::path absPath = Internal::ToAbsolutePath(path, s_AssetsRootPath);

        std::ifstream file(absPath, std::ios::in);

        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open text file: " + absPath.string());
        }

        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    void FileSystem::WriteFile(std::string_view path, const std::vector<std::uint8_t> &data)
    {
        const std::filesystem::path absPath = Internal::ToAbsolutePath(path, s_AssetsRootPath);

        if (!absPath.parent_path().empty()) // 创建父目录
        {
            std::filesystem::create_directories(absPath.parent_path());
        }

        std::ofstream file(absPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file for write: " + absPath.string());
        }

        if (!data.empty())
        {
            file.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
            if (!file.good())
            {
                throw std::runtime_error("Failed to write file: " + absPath.string());
            }
        }
    }

    bool FileSystem::Exists(std::string_view path)
    {
        const std::filesystem::path absPath = Internal::ToAbsolutePath(path, s_AssetsRootPath);
        return std::filesystem::exists(absPath);
    }

    bool FileSystem::IsDirectory(std::string_view path)
    {
        const std::filesystem::path absPath = Internal::ToAbsolutePath(path, s_AssetsRootPath);
        return std::filesystem::is_directory(absPath);
    }

    std::vector<std::string> FileSystem::ListDirectory(std::string_view path)
    {
        const std::filesystem::path absPath = Internal::ToAbsolutePath(path, s_AssetsRootPath);

        std::vector<std::string> entries{};
        if (!std::filesystem::exists(absPath) || !std::filesystem::is_directory(absPath))
        {
            return entries;
        }

        // 遍历获取目录中的所有条目
        for (const auto &entry : std::filesystem::directory_iterator(absPath))
        {
            entries.push_back(entry.path().filename().string());
        }

        std::sort(entries.begin(), entries.end());
        return entries;
    }
}