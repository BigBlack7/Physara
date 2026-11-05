#include "Compiler.hpp"
#include "Interface.hpp"
#include "Package.hpp"

/// @file
/// @brief Shader 构建命令入口, 协调接口生成, 编译, 包检查与辅助文件的完成时间.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <variant>

#include "BuildSupport.hpp"

namespace Physara::Shader
{
    static const char *FatalOperation = "startup";

    // 为第三方 noexcept 致命失败提供无需分配的进程诊断, 不尝试继续使用编译环境.
    static void FailTerminatedBuild() noexcept
    {
        // 只在此构建进程安装. noexcept 内的 OOM 无法返回 Failure, 诊断不构造 string 或日志后端.
        std::fputs("Shader fatal OOM/noexcept failure during ", stderr);
        std::fputs(FatalOperation, stderr);
        std::fputs("; this process cannot recover.\n", stderr);
        std::_Exit(1);
    }

    // 在私有驱动中将显式失败转成 BuildError, 统一由最外层退出码边界消费.
    template <typename T> static T TakeResult(Result<T> result)
    {
        if (const auto *failure = std::get_if<Failure>(&result))
        {
            throw BuildError(failure->code, failure->context.description.View());
        }
        return std::move(std::get<T>(result));
    }

    // 将路径转为 depfile 的绝对路径拼写, 转义 Make 语法中的特殊字符.
    static std::string DepfilePath(const std::filesystem::path &path)
    {
        std::string escaped;
        for (const auto character : PathText(std::filesystem::absolute(path)))
        {
            if (character == '$')
            {
                escaped += "$$";
            }
            else
            {
                if (character == ' ' || character == '#' || character == ':')
                {
                    escaped += '\\';
                }
                escaped += character;
            }
        }
        return escaped;
    }

    // 仅供接口生成规则标记已求值状态, 不替代 manifest 的完整产物核对.
    static void WriteStamp(const std::filesystem::path &path, std::string_view identity)
    {
        // stamp 只表示规则已评估当前输入, 每次成功运行都刷新. 数据产物由各自的原子发布保护.
        std::ofstream stamp(path, std::ios::binary | std::ios::trunc);
        if (!stamp || !stamp.write(identity.data(), static_cast<std::streamsize>(identity.size())))
        {
            throw BuildError(ErrorCode::IoFailure, "Cannot write Shader completion stamp.");
        }
        stamp.close();
        if (!stamp)
        {
            throw BuildError(ErrorCode::IoFailure, "Cannot close Shader completion stamp.");
        }
    }

    // generate 发布接口与完成标记; build 先辅助文件后包; check 仅完整读取校验, 用法错误返回 2.
    static int RunBuildDriver(int count, wchar_t **arguments)
    {
        if (count == 5 && std::wstring_view(arguments[1]) == L"generate")
        {
            const auto result = TakeResult(GenerateInterface({arguments[2], arguments[3]}));
            WriteStamp(arguments[4], result.abiHash);
            std::printf("Generated Shader interface %s, ABI %s\n", result.name.c_str(), result.abiHash.c_str());
            return 0;
        }
        if (count == 8 && std::wstring_view(arguments[1]) == L"build")
        {
            auto temporary = std::filesystem::path(arguments[5]);
            temporary += L".tmp";
            std::error_code cleanup;
            std::filesystem::remove(temporary, cleanup);
            auto compiler = TakeResult(Compiler::Create());
            const auto output = TakeResult(compiler->Build({arguments[2], arguments[3], arguments[4]}));
            const auto previousTime = std::filesystem::exists(arguments[5])
                                          ? std::filesystem::last_write_time(arguments[5])
                                          : std::filesystem::file_time_type::clock::now();
            // depfile 左侧对应最后发布的包, 与 CMake 首个输出一致, 右侧列出实际读取的输入.
            std::string dependencies = DepfilePath(arguments[5]) + ":";
            for (const auto &dependency : output.fileDependencies)
            {
                dependencies += " \\\n  " + DepfilePath(dependency);
            }
            dependencies += "\n";
            // 辅助输出不推进完成时间. 即使 stamp 清理受阻, 旧包仍比变化后的输入更旧.
            (void)WriteTextAtomically(arguments[6], dependencies, previousTime);
            (void)WriteTextAtomically(arguments[7], output.package.contentHash, previousTime);
            // 包是最后的发布点. depfile/stamp 的必要写入先完成, 任一失败均保留上一完整包.
            auto publication = WritePackage(arguments[5], output.package);
            if (std::holds_alternative<Failure>(publication))
            {
                // 清理失败也会报错. 辅助输出时间不越过旧包, 重新求值不依赖此 stamp 能否删除.
                std::filesystem::remove(arguments[7]);
            }
            if (!TakeResult(std::move(publication)))
            {
                // 相同字节不替换, 只刷新成功求值时间, 避免仅输入 mtime 改变后持续重编译.
                std::filesystem::last_write_time(arguments[5], std::filesystem::file_time_type::clock::now());
            }
            std::printf("Compiled Shader package %s\n", output.package.contentHash.c_str());
            return 0;
        }
        if (count == 4 && std::wstring_view(arguments[1]) == L"check")
        {
            const auto abi = PathText(std::filesystem::path(arguments[3]));
            const auto package = TakeResult(ReadPackage(arguments[2], abi));
            std::printf("Checked %zu stages, ABI %s, content %s\n", package.modules.size(), package.abiHash.c_str(),
                        package.contentHash.c_str());
            return 0;
        }
        std::fputs(
            "Usage: PhysaraShaderBuild generate <schema> <directory> <stamp>\n"
            "       PhysaraShaderBuild build <program> <source-root> <interface-root> <package> <depfile> <stamp>\n"
            "       PhysaraShaderBuild check <package> <expected-abi>\n",
            stderr);
        return 2;
    }
}

int wmain(int count, wchar_t **arguments)
{
    std::set_terminate(Physara::Shader::FailTerminatedBuild);
    if (count > 1)
    {
        const auto command = std::wstring_view(arguments[1]);
        Physara::Shader::FatalOperation =
            command == L"generate" ? "generate"
                                   : (command == L"build" ? "build" : (command == L"check" ? "check" : "arguments"));
    }
    const auto result = Physara::Shader::GuardBuild<int>(
        [&]
        {
            return Physara::Shader::RunBuildDriver(count, arguments);
        });
    if (const auto *failure = std::get_if<Physara::Shader::Failure>(&result))
    {
        std::fprintf(stderr, "Shader error %u: %s\n", static_cast<unsigned>(failure->code),
                     failure->context.description.View().data());
        return 1;
    }
    return std::get<int>(result);
}
