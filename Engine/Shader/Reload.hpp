#pragma once

/// @file
/// @brief 开发会话的文件通知与 CPU 包候选. 负责人串行观察状态, 编译和包缓存由唯一 worker 持有.

#include <filesystem>
#include <memory>
#include <optional>

#include "Compiler.hpp"

namespace Physara::Shader
{
    /// @brief 按 ProductShader 下标持有不可变 CPU 包, 相同包可同时供缓存和不同请求使用.
    using ProductPackages = std::array<std::shared_ptr<const ShaderPackage>, ProductShaderCount>;

    /// @brief 单个 worker 的条目和逻辑字节上限. 每个包按完整编码大小计费.
    struct ReloadLimits
    {
        std::size_t entries = 2 * ProductShaderCount;
        /// @brief 默认容纳当前活动代与共享接口重编所得的完整候选, 按实际大小计费.
        std::size_t bytes = 20 * 1024 * 1024;
    };

    /// @brief worker 最近一次交付的占用, 以及累计包命中, 构建尝试和实际编译.
    struct ReloadStatistics
    {
        ReloadLimits limit;
        std::size_t entries{}, bytes{};
        std::uint64_t hits{}, builds{}, discarded{}; ///< builds 包含进入编译器前被拒绝的包构建.
        double buildSeconds{}; ///< 包构建秒数, 含声明预检, 编译, 反射及身份生成.
        CompilationStatistics compilation;
    };

    /// @brief 整份候选或首个失败. 包是自持 CPU 数据, 不接触设备或帧内存.
    struct ReloadResult
    {
        std::uint64_t request{};
        ProductPackages packages;
        std::optional<Failure> failure;
        ProductShader failedRole = ProductShader::Count;
    };

    /// @brief 由 Runtime owner 轮询的开发会话. 析构时取消并等待唯一 worker, 再释放缓存和私有生成目录.
    class SourceReload final
    {
    public:
        /// @brief 观察绝对 checkout 下的 Shaders 和接口目录, 在已存在的可写 scratchParent 内建立本会话私有目录.
        [[nodiscard]] static Result<std::unique_ptr<SourceReload>> Create(const std::filesystem::path &checkout,
            const std::filesystem::path &scratchParent, ReloadLimits limits = {}) noexcept;
        ~SourceReload();
        SourceReload(const SourceReload &) = delete;
        SourceReload &operator=(const SourceReload &) = delete;

        /// @brief 先读取目录通知, 再取已完成结果. 无可交付的当前结果时返回空, 仍可能更新 Requested().
        /// @note 请求号标识本会话已观察的修订. 连续保存共享静默窗口, worker 仍在编译时可跨轮推进.
        [[nodiscard]] std::optional<ReloadResult> Poll() noexcept;
        [[nodiscard]] std::uint64_t Requested() const noexcept;
        [[nodiscard]] ReloadStatistics Statistics() const noexcept;

    private:
        struct State;
        explicit SourceReload(std::unique_ptr<State> state) noexcept;
        std::unique_ptr<State> m_State;
    };
}
