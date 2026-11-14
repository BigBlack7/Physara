#pragma once

/// @file
/// @brief 从 RHI 描述建立逻辑范围, 供声明与查询核对显式请求.
/// 交集以已校验范围为输入, 供内容回溯和编译的存储冲突检查共用.

#include "GraphResources.hpp"

#include <algorithm>
#include <utility>

namespace Physara::Renderer::Detail
{
    // 在已规范化的区间上比较. 先证明起点在外层区间内, 再计算剩余长度.
    template <typename T>
    inline bool ContainsInterval(T start, T count, T first, T length) noexcept
    {
        return first >= start && first - start < count && length != 0 && length <= count - (first - start);
    }

    // mip, layer 和 aspect 均须包含在同一外层范围内.
    inline bool Contains(const TextureRange &outer, const TextureRange &inner) noexcept
    {
        const auto aspects = static_cast<unsigned>(inner.aspect);
        return aspects != 0 && (aspects & static_cast<unsigned>(outer.aspect)) == aspects &&
               ContainsInterval(outer.firstMip, outer.mipCount, inner.firstMip, inner.mipCount) &&
               ContainsInterval(outer.firstLayer, outer.layerCount, inner.firstLayer, inner.layerCount);
    }

    // 缓冲范围按字节解释为半开区间, 内层必须有正长度.
    inline bool Contains(const RHI::BufferRange &outer, const RHI::BufferRange &inner) noexcept
    {
        return ContainsInterval(outer.byteOffset, outer.byteSize, inner.byteOffset, inner.byteSize);
    }

    // 已校验的区间端点不超过资源大小. 交集只用于区分本次查询中实际受影响的部分.
    template <typename T>
    inline std::pair<T, T> IntersectInterval(T start, T count, T first, T length) noexcept
    {
        const T begin = (std::max)(start, first);
        const T end = (std::min)(start + count, first + length);
        return {begin, end > begin ? end - begin : T{}};
    }

    // 三个维度同时相交时才有共享子资源, 深度和模板可各自重叠.
    inline std::optional<TextureRange> Intersect(const TextureRange &a, const TextureRange &b) noexcept
    {
        const auto [mip, mips] = IntersectInterval(a.firstMip, a.mipCount, b.firstMip, b.mipCount);
        const auto [layer, layers] = IntersectInterval(a.firstLayer, a.layerCount, b.firstLayer, b.layerCount);
        const auto aspects = static_cast<unsigned>(a.aspect) & static_cast<unsigned>(b.aspect);
        if (mips == 0 || layers == 0 || aspects == 0)
        {
            return std::nullopt;
        }
        return TextureRange{mip, mips, layer, layers, static_cast<TextureAspect>(aspects)};
    }

    // 相邻区间交集为空, 只有共同覆盖的字节需要参与内容和顺序判断.
    inline std::optional<RHI::BufferRange> Intersect(const RHI::BufferRange &a, const RHI::BufferRange &b) noexcept
    {
        const auto [offset, bytes] = IntersectInterval(a.byteOffset, a.byteSize, b.byteOffset, b.byteSize);
        return bytes != 0 ? std::optional(RHI::BufferRange{offset, bytes}) : std::nullopt;
    }

    // 以格式确定可用 aspect, 以纹理维度区分数组层和 3D 深度.
    // 此处只校验逻辑范围所需的字段. 完整形状, 样本兼容和设备支持由后续编译及 RHI 检查.
    inline std::optional<TextureRange> FullRange(const RHI::TextureDesc &desc) noexcept
    {
        using Dimension = RHI::TextureDimension;
        const auto &format = RHI::GetFormatInfo(desc.format);
        if (format.format == RHI::Format::UNKNOWN || desc.dimension == Dimension::Unknown ||
            desc.dimension > Dimension::Texture3D || desc.width == 0 || desc.height == 0 || desc.depth == 0 ||
            desc.arraySize == 0 || desc.mipLevels == 0 || desc.sampleCount == 0)
        {
            return std::nullopt;
        }

        const bool layered = desc.dimension == Dimension::Texture1DArray || desc.dimension == Dimension::Texture2DArray ||
                             desc.dimension == Dimension::TextureCube || desc.dimension == Dimension::TextureCubeArray ||
                             desc.dimension == Dimension::Texture2DMSArray;
        if (!layered && desc.arraySize != 1)
        {
            return std::nullopt;
        }
        auto aspect = TextureAspect::Color;
        if (format.hasDepth)
        {
            aspect = format.hasStencil ? TextureAspect::DepthStencil : TextureAspect::Depth;
        }
        else if (format.hasStencil)
        {
            aspect = TextureAspect::Stencil;
        }
        return TextureRange{0, desc.mipLevels, 0, desc.arraySize, aspect};
    }

    // 字节容量为零的描述没有合法的半开区间.
    inline std::optional<RHI::BufferRange> FullRange(const RHI::BufferDesc &desc) noexcept
    {
        return desc.byteSize != 0 ? std::optional(RHI::BufferRange{0, desc.byteSize}) : std::nullopt;
    }
}
