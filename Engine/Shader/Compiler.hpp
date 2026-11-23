#pragma once

/// @file
/// @brief 同步 CPU 编译接口, 返回独立可消费的包与本次构建的文件依赖.

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "ProductVariants.hpp"

namespace Physara::Shader
{
    /// @brief 程序声明及两个受控根, 源引用限制在 sourceRoot 与生成的 interfaceRoot 内.
    struct BuildRequest
    {
        std::filesystem::path programPath;
        std::filesystem::path sourceRoot;
        std::filesystem::path interfaceRoot;
        std::optional<VariantKey> variant = std::nullopt; ///< 产品入口固定角色和结构值, 独立编译入口可留空.
    };

    /// @brief 本机复验用路径及实际读取的原字节摘要. 路径不进入可移植包.
    struct BuildInput
    {
        std::filesystem::path path;
        std::string hash;
    };

    /// @brief 成功构建的自持包和 depfile 路径, 不自动向磁盘发布包.
    struct BuildOutput
    {
        ShaderPackage package;
        std::vector<BuildInput> inputs; ///< 供依赖生成及开发会话复验, 对应构建时实际使用的字节.
    };

    /// @brief 实际进入 glslang 的阶段编译次数, 以及阶段编译, 链接和 SPIR-V 生成的累计秒数.
    struct CompilationStatistics
    {
        std::uint64_t stages{};
        double seconds{};
    };

    /// @brief 拥有 glslang 进程初始化权的同步环境, 单个环境串行处理请求, 不接触图形 context.
    /// @note 包不依赖环境继续存活; 析构前须结束全部使用者.
    class Compiler final
    {
      public:
        /// @brief 创建唯一环境, 分配或初始化失败归还进程 claim.
        [[nodiscard]] static Result<std::unique_ptr<Compiler>> Create() noexcept;
        ~Compiler();
        Compiler(const Compiler &) = delete;
        Compiler &operator=(const Compiler &) = delete;
        Compiler(Compiler &&) = delete;
        Compiler &operator=(Compiler &&) = delete;

        /// @brief 校验生成接口与请求, 编译/链接 VS 和 FS, 核对最终 IR 后形成内容身份.
        /// @return 自持 BuildOutput 或 Failure, 不返回部分成功包, 输入仅在调用期间借用.
        /// @param statistics 可选累计计数, 含实际开始后失败的阶段, 声明预检不计为 GLSL 编译.
        /// @note 可传播异常转为 Failure; STL/第三方 noexcept 内致命 OOM 不保证同进程恢复.
        [[nodiscard]] Result<BuildOutput> Build(const BuildRequest &request,
                                                CompilationStatistics *statistics = nullptr) noexcept;

      private:
        Compiler() = default;
        bool m_Initialized = false;
        static std::atomic_flag s_Claim;
    };
}
