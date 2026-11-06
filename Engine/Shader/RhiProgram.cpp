#include "RhiProgram.hpp"

/// @file
/// @brief 复用包读取边界, Shader 工厂自行复制模块, 本层不再保存或重算一份 native 反射.

#include "BuildSupport.hpp"
#include "Package.hpp"

namespace Physara::Shader
{
    Result<RhiProgram> LoadRhiProgram(RHI::Device &device, const std::filesystem::path &path,
                                      std::string_view expectedAbiHash) noexcept
    {
        return GuardBuild<RhiProgram>([&]() -> Result<RhiProgram> {
            // 文件完整性, profile, ABI 与双阶段一致性先由包边界确认, 失败时尚未创建 RHI 对象.
            auto loaded = ReadPackage(path, expectedAbiHash);
            if (const auto *failure = std::get_if<Failure>(&loaded))
            {
                return *failure;
            }
            const auto &package = std::get<ShaderPackage>(loaded);
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
