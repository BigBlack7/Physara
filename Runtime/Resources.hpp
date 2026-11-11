#pragma once

/// @file
/// @brief 从实际产品模块目录解析同配置 UI 包, 路径不依赖当前工作目录.

#include <filesystem>
#include <variant>

#include <Core/Error.hpp>

namespace Physara::Runtime
{
    /// @brief 从主模块目录拼接当前配置的生成路径. 包打开及 ABI 校验由 Editor 装配完成.
    /// @return 规范化的路径, 或主模块路径无法取得时的有界原因.
    [[nodiscard]] std::variant<std::filesystem::path, Core::BoundedText> LocateEditorUiPackage();
}
