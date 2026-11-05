#include "Compiler.hpp"

#include <new>

#include <glslang/Public/ShaderLang.h>

#include "BuildSupport.hpp"

namespace Physara::Shader
{
    std::atomic_flag Compiler::s_Claim = ATOMIC_FLAG_INIT;

    Result<std::unique_ptr<Compiler>> Compiler::Create() noexcept
    {
        return GuardBuild<std::unique_ptr<Compiler>>(
            []() -> std::unique_ptr<Compiler>
            {
                // glslang 初始化是进程级事实. 在真正构造之前取得唯一 claim, 拒绝并发环境而不隐藏全局锁.
                if (s_Claim.test_and_set(std::memory_order_acquire))
                {
                    throw BuildError(ErrorCode::CompilerUnavailable, "A Shader compiler environment already exists.");
                }
                std::unique_ptr<Compiler> compiler(new (std::nothrow) Compiler);
                if (!compiler)
                {
                    s_Claim.clear(std::memory_order_release);
                    throw std::bad_alloc();
                }
                bool initialized = false;
                try
                {
                    initialized = glslang::InitializeProcess();
                }
                catch (const std::bad_alloc &)
                {
                    // 固定版本在分配进程 pool 之前递增 client 计数; 分配失败也必须配对归还该计数.
                    glslang::FinalizeProcess();
                    throw;
                }
                if (!initialized)
                {
                    throw BuildError(ErrorCode::CompilerUnavailable, "Cannot initialize glslang.");
                }
                compiler->m_Initialized = true;
                return compiler;
            });
    }

    Compiler::~Compiler()
    {
        if (m_Initialized)
        {
            glslang::FinalizeProcess();
        }
        s_Claim.clear(std::memory_order_release);
    }
}
