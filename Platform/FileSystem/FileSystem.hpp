#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Physara::Platform
{
    class FileSystem final
    {
    public:
        // 初始化Assets根目录
        static void Init(std::string assetsRootPath);

        // 路径操作
        static std::string ResolvePath(std::string_view relativePath);
        static std::string GetExtension(std::string_view path);
        static std::string GetFileName(std::string_view path);
        static std::string GetParentDir(std::string_view path);

        // 文件读写
        static std::vector<std::uint8_t> ReadBinaryFile(std::string_view path);
        static std::string ReadTextFile(std::string_view path);
        static void WriteFile(std::string_view path, const std::vector<std::uint8_t> &data);

        // 查询
        static bool Exists(std::string_view path);
        static bool IsDirectory(std::string_view path);
        static std::vector<std::string> ListDirectory(std::string_view path);

    private:
        static std::string s_AssetsRootPath;
    };
}