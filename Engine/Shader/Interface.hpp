#pragma once

/// @file
/// @brief 从权威声明生成 CPU/GLSL 接口与核对清单, 仅依赖 RHI 描述合同, 不依赖窗口或 Renderer.

#include <filesystem>
#include <string>
#include <vector>

#include "Types.hpp"

namespace Physara::Shader
{
    /// @brief 权威 schema 路径与输出目录, 路径仅在同步调用期间借用.
    struct InterfaceRequest
    {
        std::filesystem::path schemaPath;
        std::filesystem::path outputDirectory;
    };

    /// @brief 一代接口的自持身份和产物路径, outputs 包含最后发布的 Manifest.json.
    struct GeneratedInterface
    {
        std::string name;
        std::string abiHash;
        std::vector<std::filesystem::path> outputs;
    };

    /// @brief 同步生成接口, 相同内容不重写, 清单最后发布.
    /// @return 自持名称, ABI 与产物路径清单, 或 Failure.
    /// @note 逐文件替换不构成目录事务; 中途失败可留下部分新产物, 清单核对会拒绝混合代.
    [[nodiscard]] Result<GeneratedInterface> GenerateInterface(const InterfaceRequest &request) noexcept;
}
