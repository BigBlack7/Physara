#pragma once

// 接口生成只消费声明与 RHI 常量合同, 不依赖任何生成头或业务 Renderer.

#include <filesystem>
#include <string>
#include <vector>

#include "Types.hpp"

namespace Physara::Shader
{
    struct InterfaceRequest
    {
        std::filesystem::path schemaPath;
        std::filesystem::path outputDirectory;
    };

    struct GeneratedInterface
    {
        std::string name;
        std::string abiHash;
        std::vector<std::filesystem::path> outputs;
    };

    // 同步生成. 路径只在调用期间借用, 成功值拥有名称/ABI/输出清单.
    // 不变内容不重写; 完成描述最后发布, 失败返回 Failure 且不能识别为完整新代.
    [[nodiscard]] Result<GeneratedInterface> GenerateInterface(const InterfaceRequest &request) noexcept;
}
