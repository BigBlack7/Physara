#pragma once

// 仅检查配置可见的故障注入边界, 用于验证诊断写失败和计数饱和; 产品不提供这些控制入口.

#if !defined(PHYSARA_CORE_NATIVE_CHECKS)
#error This private header is available only to the temporary native checks.
#endif

#include "Diagnostics.hpp"

namespace Physara::Core::Detail
{
    // 只在临时检查构建定义, 不进入产品 target 的符号或配置.
    struct DiagnosticsChecks
    {
        static void FailNextWrite(Diagnostics &diagnostics);
        static void SetLossCounters(Diagnostics &diagnostics, std::uint64_t value);
    };
}
