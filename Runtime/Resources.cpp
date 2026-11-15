#include "Resources.hpp"

/// @file
/// @brief Windows 主模块路径的冷启动查询, 为 CPU 资产和生成包提供一致的位置基准.

#include <array>
#include <string_view>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "RuntimeResources.hpp"

namespace Physara::Runtime
{
    std::variant<ResourceLocations, Core::BoundedText> LocateResources()
    {
        // 用宽字符保留 Windows 主模块路径. 缓冲区填满按截断处理, 防止从不完整目录解析包.
        std::array<wchar_t, 32768> module{};
        const auto length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
        if (length == 0 || length >= module.size())
        {
            return Core::BoundedText("Cannot locate the product executable directory");
        }
        const std::filesystem::path executable(std::wstring_view(module.data(), length));
        // Assets 相对 exe 目录, UI 包相对同一目录按生成布局定位; 两者都不退回当前工作目录查找.
        const auto directory = executable.parent_path();
        return ResourceLocations{directory, (directory / EditorUiPackageRelative).lexically_normal()};
    }
}
