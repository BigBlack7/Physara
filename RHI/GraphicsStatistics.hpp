#pragma once

/// @file
/// @brief 原生图形事实的轻量值快照, 可在同一设备的录制边界取差.

#include <cstdint>

namespace Physara::RHI
{
    /// @brief 累计设备实际发出的绘制, 对象绑定, 传输和屏障. 纹理传输字节只计有效 texel.
    /// @note 取差得到两次采样之间的调用数. 图版本转换由后端发出原生屏障时才贡献 barriers.
    /// 映射块按新分配或从空闲表重借计数, 同包在已有块尾部追加写入沿用该次分配.
    struct GraphicsOperationCounts
    {
        std::uint64_t draws{};
        std::uint64_t bindings{};
        std::uint64_t uploads{};
        std::uint64_t uploadBytes{};
        std::uint64_t barriers{};
        std::uint64_t uploadChunksAllocated{};
        std::uint64_t uploadChunksReused{};

        /// @brief 只对同一设备按时间排序的两个快照取差, 使用无符号模差.
        [[nodiscard]] GraphicsOperationCounts operator-(const GraphicsOperationCounts &earlier) const noexcept
        {
            return {draws - earlier.draws, bindings - earlier.bindings, uploads - earlier.uploads,
                    uploadBytes - earlier.uploadBytes, barriers - earlier.barriers,
                    uploadChunksAllocated - earlier.uploadChunksAllocated,
                    uploadChunksReused - earlier.uploadChunksReused};
        }
    };

    /// @brief 上传池的字节快照. capacity 包含空闲块, capacity 减 available 是仍租给录制或提交包的容量.
    /// highWater 为该池已分配容量的历史峰值. volatileCapacity 属于单独计费的常量缓冲存储.
    struct GraphicsUploadStorage
    {
        std::uint64_t capacity{};
        std::uint64_t available{};
        std::uint64_t highWater{};
        std::uint64_t limit{};
        std::uint64_t volatileCapacity{};
    };
}
