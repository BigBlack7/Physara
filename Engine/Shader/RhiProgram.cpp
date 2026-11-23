#include "RhiProgram.hpp"

/// @file
/// @brief 为文件包和内存包检查消费者的 ABI, 源入口及指定的活动资源. RHI 工厂在返回前复制模块字节.

#include <algorithm>

#include "BuildSupport.hpp"
#include "Package.hpp"

namespace Physara::Shader
{
    /// @brief 包已验证活动 IR 和唯一资源身份. 按名核对实际活动集合, 这些名称也唯一确定其槽和维度.
    static bool MatchesActiveResources(const ShaderPackage &package, const ExpectedActiveResources &expected)
    {
        const auto module = std::ranges::find(package.modules, expected.stage, &ShaderModule::stage);
        if (module == package.modules.end())
        {
            return false;
        }
        std::size_t count = 0;
        for (const auto &resource : module->resources)
        {
            if (resource.set == expected.set)
            {
                if (std::ranges::find(expected.names, std::string_view(resource.name)) == expected.names.end())
                {
                    return false;
                }
                ++count;
            }
        }
        return count == expected.names.size();
    }

    Result<RhiProgram> LoadRhiProgram(RHI::Device &device, const std::filesystem::path &path,
                                      std::string_view expectedAbiHash,
                                      std::optional<ExpectedActiveResources> expectedActive,
                                      std::string_view expectedFragmentSource) noexcept
    {
        const auto loaded = ReadPackage(path, expectedAbiHash);
        if (const auto *failure = std::get_if<Failure>(&loaded))
        {
            return *failure;
        }
        return CreateRhiProgram(device, std::get<ShaderPackage>(loaded), expectedAbiHash, expectedActive,
                                expectedFragmentSource);
    }

    Result<RhiProgram> CreateRhiProgram(RHI::Device &device, const ShaderPackage &package,
        std::string_view expectedAbiHash, std::optional<ExpectedActiveResources> expectedActive,
        std::string_view expectedFragmentSource, std::string_view expectedVertexSource) noexcept
    {
        return GuardBuild<RhiProgram>([&]() -> Result<RhiProgram> {
            // 两个上游均验证包. 到设备边界时确认当前调用方的阶段和消费合同.
            if (package.profile != GraphicsProfile || package.modules.size() != 2 ||
                package.modules[0].stage != Stage::Vertex || package.modules[1].stage != Stage::Fragment)
            {
                return MakeFailure(ErrorCode::UnsupportedProfile, "The consumer requires a graphics Shader package.");
            }
            if (package.abiHash != expectedAbiHash)
            {
                return MakeFailure(ErrorCode::InterfaceMismatch, "Program ABI does not match the running consumer.");
            }
            if (!expectedVertexSource.empty() && package.modules[0].sourcePath != expectedVertexSource)
            {
                return MakeFailure(ErrorCode::InterfaceMismatch, "Package vertex source does not match the role.");
            }
            if (!expectedFragmentSource.empty() && package.modules[1].sourcePath != expectedFragmentSource)
            {
                Core::BoundedText issue("Package fragment source mismatch. Expected ");
                issue.Append(expectedFragmentSource);
                issue.Append(", found ");
                issue.Append(package.modules[1].sourcePath);
                return MakeFailure(ErrorCode::InterfaceMismatch, issue.View());
            }
            if (expectedActive && !MatchesActiveResources(package, *expectedActive))
            {
                return MakeFailure(ErrorCode::InterfaceMismatch, "Package active resources do not match the required role");
            }
            RhiProgram result;
            result.abiHash = package.abiHash;
            result.contentHash = package.contentHash;
            // Shader 工厂仅借用模块字节到返回. 本地结果先保活已成功的阶段, 全部成功后才交付.
            for (const auto &module : package.modules)
            {
                RHI::ShaderDesc desc;
                desc.shaderType = module.stage == Stage::Vertex ? RHI::ShaderType::Vertex : RHI::ShaderType::Pixel;
                desc.entryName = module.entry;
                desc.debugName = module.sourcePath;
                auto shader =
                    device.CreateShader(desc, module.words.data(), module.words.size() * sizeof(std::uint32_t));
                if (!shader)
                {
                    // 结果析构释放本次先前成功的阶段, 具体原生原因仍由 Device 回调报告.
                    return MakeFailure(ErrorCode::CompilationFailed,
                                       "RHI Shader creation failed; see device diagnostic.");
                }
                (module.stage == Stage::Vertex ? result.vertex : result.fragment) = std::move(shader);
            }
            return result;
        });
    }
    Result<RhiComputeProgram> LoadRhiComputeProgram(RHI::Device &device, const std::filesystem::path &path,
                                                     std::string_view expectedAbiHash) noexcept
    {
        return GuardBuild<RhiComputeProgram>([&]() -> Result<RhiComputeProgram> {
            auto loaded = ReadPackage(path, expectedAbiHash);
            if (const auto *failure = std::get_if<Failure>(&loaded))
            {
                return *failure;
            }
            const auto &package = std::get<ShaderPackage>(loaded);
            if (package.profile != ComputeProfile)
            {
                return MakeFailure(ErrorCode::UnsupportedProfile, "The consumer requires a compute Shader package.");
            }
            // 单阶段形状和 localSize 已由包读取器核对. 设备在本次调用期间取得字节码, 返回值携带 CS 和包身份.
            const auto &module = package.modules.front();
            RHI::ShaderDesc desc;
            desc.shaderType = RHI::ShaderType::Compute;
            desc.entryName = module.entry;
            desc.debugName = module.sourcePath;
            auto shader = device.CreateShader(desc, module.words.data(), module.words.size() * sizeof(std::uint32_t));
            if (!shader)
            {
                return MakeFailure(ErrorCode::CompilationFailed, "RHI Compute Shader creation failed.");
            }
            return RhiComputeProgram{std::move(shader), module.localSize, package.abiHash, package.contentHash};
        });
    }
} // namespace Physara::Shader
