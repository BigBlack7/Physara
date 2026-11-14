#pragma once

/// @file
/// @brief 声明与编译共用的写入效果和范围扣除规则.
/// 调用方沿版本链从新到旧解释已校验的请求, 并据实际贡献判断内容有效性或登记依赖.

#include <cstdint>
#include <optional>
#include <vector>

#include "GraphRanges.hpp"

namespace Physara::Renderer::Detail
{
    // Load 的输入另行验证. Discard 后只有完整覆盖才定义内容, DontCare 总使所选范围失效.
    inline bool DefinesContent(const TextureWrite &write) noexcept
    {
        const auto &ops = write.ops;
        return ops.store == StoreOp::Store &&
               (ops.load != LoadOp::Discard || ops.coverage == WriteCoverage::Full);
    }

    // 每次 Buffer 写入都完整定义所声明的字节区间.
    inline bool DefinesContent(const BufferWrite &) noexcept
    {
        return true;
    }

    // cut 已包含在区间内. 依次提交其前后留下的正长度区间.
    template <typename T, typename Append>
    inline void AppendIntervalRemainder(T first, T count, T cutFirst, T cutCount, Append append)
    {
        const T end = first + count;
        const T cutEnd = cutFirst + cutCount;
        if (first < cutFirst)
        {
            append(first, cutFirst - first);
        }
        if (cutEnd < end)
        {
            append(cutEnd, end - cutEnd);
        }
    }

    // cut 是 range 内的交集. 先保留其它 aspect, 再沿 mip 分割相交 aspect.
    // 最后只在 cut 的 mip 区段内分割 layer, 保证输出分片互不重叠.
    inline void AppendRemainder(const TextureRange &range, const TextureRange &cut, std::vector<TextureRange> &out)
    {
        const auto otherAspects = static_cast<unsigned>(range.aspect) & ~static_cast<unsigned>(cut.aspect);
        if (otherAspects != 0)
        {
            out.push_back({range.firstMip, range.mipCount, range.firstLayer, range.layerCount,
                           static_cast<TextureAspect>(otherAspects)});
        }
        AppendIntervalRemainder(range.firstMip, range.mipCount, cut.firstMip, cut.mipCount,
                                [&](std::uint32_t mip, std::uint32_t mips)
        {
            out.push_back({mip, mips, range.firstLayer, range.layerCount, cut.aspect});
        });
        AppendIntervalRemainder(range.firstLayer, range.layerCount, cut.firstLayer, cut.layerCount,
                                [&](std::uint32_t layer, std::uint32_t layers)
        {
            out.push_back({cut.firstMip, cut.mipCount, layer, layers, cut.aspect});
        });
    }

    // 缓冲只留下本次写入之前和之后的字节区间.
    inline void AppendRemainder(const RHI::BufferRange &range, const RHI::BufferRange &cut,
                                std::vector<RHI::BufferRange> &out)
    {
        AppendIntervalRemainder(range.byteOffset, range.byteSize, cut.byteOffset, cut.byteSize,
                                [&](std::uint64_t offset, std::uint64_t bytes)
        {
            out.emplace_back(offset, bytes);
        });
    }

    /// @brief 处理当前写入后, 查询需要继续回溯, 已有完整来源, 或命中未定义内容.
    enum class ContentStatus
    {
        Pending,
        Defined,
        Undefined
    };

    // 一次查询从空 optional 开始. 首次局部覆盖后, remaining 保存尚未确定来源的互斥分片.
    // 调用方为每次写入将 contributed 初始化为 false. 写入定义任一未决分片时将其置为 true.
    // Defined 和 Undefined 都结束本次查询. 只有 Pending 需要继续检查前一版本或导入内容.
    template <typename Range>
    ContentStatus ApplyWriteToQuery(const Range &requested, const Range &written, bool defined,
                                    std::optional<std::vector<Range>> &remaining, bool &contributed)
    {
        if (!remaining)
        {
            const auto cut = Intersect(requested, written);
            if (!cut)
            {
                return ContentStatus::Pending;
            }
            if (!defined)
            {
                return ContentStatus::Undefined;
            }
            contributed = true;
            if (*cut == requested)
            {
                return ContentStatus::Defined;
            }
            AppendRemainder(requested, *cut, remaining.emplace(0));
            return ContentStatus::Pending;
        }

        auto &ranges = *remaining;
        for (std::size_t i = ranges.size(); i != 0;)
        {
            const Range range = ranges[--i];
            const auto cut = Intersect(range, written);
            if (!cut)
            {
                continue;
            }
            if (!defined)
            {
                return ContentStatus::Undefined;
            }
            contributed = true;
            // 倒序扫描允许移入已检查的尾部区间. 新分片也已排除本次写入, 可留给更早的版本.
            ranges[i] = ranges.back();
            ranges.pop_back();
            AppendRemainder(range, *cut, ranges);
        }
        return ranges.empty() ? ContentStatus::Defined : ContentStatus::Pending;
    }
}
