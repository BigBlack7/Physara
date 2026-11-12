#pragma once

/// @file
/// @brief 为同一次帧槽使用提供稳定地址的 CPU 字节存储.
/// 由帧槽 owner 按准备结果预留和复用, 只处理字节范围及对齐.

#include <cstddef>
#include <memory>
#include <span>

namespace Physara::Core
{
    /// @brief 有界连续存储. owner 决定何时结束全部借用并复用容量.
    class FrameArena final
    {
    public:
        static constexpr std::size_t MaxBytes = 64 * 1024 * 1024;
        static constexpr std::size_t MaxAlignment = alignof(std::max_align_t);

        FrameArena() = default;
        FrameArena(const FrameArena &) = delete;
        FrameArena &operator=(const FrameArena &) = delete;

        /// @brief 预留本次使用所需的总容量, 包含已经占用的字节及对齐填充.
        /// @return 容量满足请求时为 true. 扩容要求本轮尚无分配, 失败时保留原存储.
        [[nodiscard]] bool Reserve(std::size_t bytes) noexcept;

        /// @brief 取得本次使用的非空区段, 地址按指定的基础对齐要求排列.
        /// @param alignment 须为不超过 MaxAlignment 的正二次幂.
        /// @return 输入为空或范围越界时返回空, 已发布的区段及游标保持有效.
        /// @note 帧槽 owner 负责在 Reset 前结束借用, 复用原地址会开始新的使用期.
        [[nodiscard]] std::span<std::byte> Allocate(std::size_t count, std::size_t elementSize,
                                                    std::size_t alignment = 1) noexcept;

        /// @brief 结束一次失败准备新增的尾部借用, 保留前缀地址, 容量和高水位.
        /// @pre used 为当前槽使用期内的准备起点, 调用者已结束其后的全部借用.
        /// @note 仅退回游标. 保留容量供后续准备复用, 高水位继续记录本次实际峰值.
        void Rewind(std::size_t used) noexcept;

        /// @brief 结束本次使用, 保留已分配容量与历史最高使用量.
        /// @pre owner 已结束全部借用并证明对应的槽可以复用.
        void Reset() noexcept { m_Used = 0; }
        /// @brief 已使用字节包含对齐填充. HighWater 跨槽复用保留历史峰值.
        [[nodiscard]] std::size_t Used() const noexcept { return m_Used; }
        [[nodiscard]] std::size_t Capacity() const noexcept { return m_Capacity; }
        [[nodiscard]] std::size_t HighWater() const noexcept { return m_HighWater; }

    private:
        std::unique_ptr<std::byte[]> m_Storage;
        std::size_t m_Capacity = 0;
        std::size_t m_Used = 0;
        std::size_t m_HighWater = 0;
    };
}
