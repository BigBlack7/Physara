#pragma once

// 同步 CPU 编译环境拥有 glslang 的进程级初始化权. 单个环境串行处理请求, 不接触图形 context.

#include <atomic>
#include <filesystem>
#include <memory>
#include <vector>

#include "Types.hpp"

namespace Physara::Shader
{
    struct BuildRequest
    {
        std::filesystem::path programPath;
        std::filesystem::path sourceRoot;
        std::filesystem::path interfaceRoot;
    };

    struct BuildOutput
    {
        ShaderPackage package;
        // 仅供当前构建的 DEPFILE, 实际路径不进入可重建的包身份.
        std::vector<std::filesystem::path> fileDependencies;
    };

    class Compiler final
    {
      public:
        // 同时至多一个成功环境. 分配/初始化失败不会泄漏进程初始化权.
        [[nodiscard]] static Result<std::unique_ptr<Compiler>> Create() noexcept;
        ~Compiler();
        Compiler(const Compiler &) = delete;
        Compiler &operator=(const Compiler &) = delete;
        Compiler(Compiler &&) = delete;
        Compiler &operator=(Compiler &&) = delete;

        // 输入只在调用期间借用, 返回值拥有全部 CPU 数据. 失败不返回部分成功包.
        // 可传播的分配/依赖异常转为 Failure; STL/第三方 noexcept 内致命 OOM 不保证同进程恢复.
        [[nodiscard]] Result<BuildOutput> Build(const BuildRequest &request) noexcept;

      private:
        Compiler() = default;
        bool m_Initialized = false;
        static std::atomic_flag s_Claim;
    };
}
