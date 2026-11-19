#pragma once

/// @file
/// @brief 原生图形事实的轻量值快照, 可在同一设备的录制边界取差.

#include <cstdint>

namespace Physara::RHI
{
    /// @brief 累计设备实际发出的绘制, 对象绑定, 上传和屏障. 纹理上传字节仅计 texel 有效载荷.
    /// @note 取差得到两次采样之间的调用数. 图版本转换由后端发出原生屏障时才贡献 barriers.
    struct GraphicsOperationCounts
    {
        std::uint64_t draws{};
        std::uint64_t bindings{};
        std::uint64_t uploads{};
        std::uint64_t uploadBytes{};
        std::uint64_t barriers{};

        /// @brief 只对同一设备按时间排序的两个快照取差, 使用无符号模差.
        [[nodiscard]] GraphicsOperationCounts operator-(const GraphicsOperationCounts &earlier) const noexcept
        {
            return {draws - earlier.draws, bindings - earlier.bindings, uploads - earlier.uploads,
                    uploadBytes - earlier.uploadBytes, barriers - earlier.barriers};
        }
    };
}
