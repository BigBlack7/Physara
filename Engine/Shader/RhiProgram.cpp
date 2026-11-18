#include "RhiProgram.hpp"

/// @file
/// @brief 复用包读取边界, Shader 工厂自行复制模块, 本层不再保存或重算一份 native 反射.

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
                                      std::optional<ExpectedActiveResources> expectedActive) noexcept
    {
        return GuardBuild<RhiProgram>([&]() -> Result<RhiProgram> {
            // 文件完整性, profile, ABI 与双阶段一致性先由包边界确认, 失败时尚未创建 RHI 对象.
            auto loaded = ReadPackage(path, expectedAbiHash);
            if (const auto *failure = std::get_if<Failure>(&loaded))
            {
                return *failure;
            }
            const auto &package = std::get<ShaderPackage>(loaded);
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
} // namespace Physara::Shader
