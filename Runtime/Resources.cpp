#include "Resources.hpp"

/// @file
/// @brief Windows 主模块路径的冷启动查询, 生成的相对资源路径由 Runtime CMake 提供.

#include <array>
#include <string_view>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "RuntimeResources.hpp"

namespace Physara::Runtime
{
    std::variant<std::filesystem::path, Core::BoundedText> LocateEditorUiPackage()
    {
        // 用宽字符保留 Windows 主模块路径. 缓冲区填满按截断处理, 防止从不完整目录解析包.
        std::array<wchar_t, 32768> module{};
        const auto length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
        if (length == 0 || length >= module.size())
        {
            return Core::BoundedText("Cannot locate the product executable directory");
        }
        const std::filesystem::path executable(std::wstring_view(module.data(), length));
        return (executable.parent_path() / EditorUiPackageRelative).lexically_normal();
    }
}
