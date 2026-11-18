#pragma once

/// @file
/// @brief 从实际产品模块目录解析 CPU 资产根和同配置 Shader 包.

#include <filesystem>
#include <variant>

#include <Core/Error.hpp>
#include <Renderer/Paths/Forward.hpp>

namespace Physara::Runtime
{
    /// @brief 基于同一个 exe 目录的位置快照. 项目资产和构建生成包使用各自的相对布局.
    struct ResourceLocations
    {
        std::filesystem::path projectRoot; ///< Assets/... 相对该目录.
        std::filesystem::path editorUiPackage; ///< 由 CMake 为当前配置生成的 UI 包位置, 不属于 Assets 输入.
        Renderer::ForwardPackagePaths forward; ///< 同配置的场景包. 路径定位不代表已经准备.
    };

    /// @brief 定位资源根并拼接当前配置的生成包路径. 此步骤不打开任何资产.
    /// @return 规范化的位置, 或主模块路径无法取得时的有界原因.
    /// @note 与进程当前工作目录无关. 路径分配异常由 Runtime 冷启动边界处理.
    [[nodiscard]] std::variant<ResourceLocations, Core::BoundedText> LocateResources();
}
