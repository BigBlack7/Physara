#pragma once

/// @file
/// @brief Validation 和 GL 共用的传输描述规则, 不查询设备或推导完成状态.

#include "../RHI.hpp"

namespace Physara::RHI::Transfer
{
    /// @brief 二维传输以 arraySlice 选层或 cube 面, 矩形坐标位于选中的 mip 内.
    [[nodiscard]] inline bool Planar(TextureDimension dimension) noexcept
    {
        return dimension == TextureDimension::Texture2D || dimension == TextureDimension::Texture2DArray ||
               dimension == TextureDimension::TextureCube || dimension == TextureDimension::TextureCubeArray;
    }

    [[nodiscard]] inline bool Multisample(TextureDimension dimension) noexcept
    {
        return dimension == TextureDimension::Texture2DMS || dimension == TextureDimension::Texture2DMSArray;
    }

    /// @brief 描述中可直接判定的格式与用途互斥. 实际 target 支持仍由各后端查询.
    [[nodiscard]] inline bool CompatibleTextureUsage(const TextureDesc &desc, const FormatInfo &format) noexcept
    {
        return !(format.blockSize > 1 && (desc.isRenderTarget || desc.isUAV || Multisample(desc.dimension))) &&
               !(format.kind == FormatKind::DepthStencil && desc.isUAV);
    }

    /// @brief OpenGL 颜色 mip 的固定格式范围. 过滤与附件能力还须按实际 target 核对.
    [[nodiscard]] inline bool ColorMipFormat(Format format) noexcept
    {
        return format == Format::RGBA8_UNORM || format == Format::SRGBA8_UNORM || format == Format::RGBA16_FLOAT;
    }

    /// @brief 明确区分追踪器的范围求交与传输使用的精确边界, 保留全剩余哨兵.
    [[nodiscard]] inline bool ExactRange(const TextureDesc &desc, TextureSubresourceSet range) noexcept
    {
        return range.baseMipLevel < desc.mipLevels && range.baseArraySlice < desc.arraySize &&
               range.numMipLevels != 0 && range.numArraySlices != 0 &&
               (range.numMipLevels == TextureSubresourceSet::AllMipLevels ||
                range.numMipLevels <= desc.mipLevels - range.baseMipLevel) &&
               (range.numArraySlices == TextureSubresourceSet::AllArraySlices ||
                range.numArraySlices <= desc.arraySize - range.baseArraySlice);
    }

    /// @pre level 来自已验证的描述与范围, 小于尺寸字段的 32 位宽度.
    [[nodiscard]] inline std::uint32_t MipSize(std::uint32_t size, std::uint32_t level) noexcept
    {
        const auto extent = size >> level;
        return extent != 0 ? extent : 1u;
    }

    /// @brief 对已经界检且已解析宽高的矩形判断是否覆盖整个 mip 的一个层或面.
    [[nodiscard]] inline bool WholeSlice(const TextureDesc &desc, const TextureSlice &slice) noexcept
    {
        return slice.x == 0 && slice.y == 0 && slice.z == 0 && slice.depth == 1 &&
               slice.width == MipSize(desc.width, slice.mipLevel) &&
               slice.height == MipSize(desc.height, slice.mipLevel);
    }

    /// @brief 判断同一纹理的已解析二维区域是否在同一 mip/layer 上相交.
    /// @pre 两个 slice 已按实际尺寸界检. 半开矩形的终点可直接相加, 边缘相接不相交.
    [[nodiscard]] inline bool PlanarSlicesOverlap(const TextureSlice &first, const TextureSlice &second) noexcept
    {
        return first.mipLevel == second.mipLevel && first.arraySlice == second.arraySlice &&
               first.x < second.x + second.width && second.x < first.x + first.width &&
               first.y < second.y + second.height && second.y < first.y + first.height;
    }

    /// @brief OpenGL 普通纹理间的复制要求未压缩格式. BC5 的整级块传输由 staging 重载处理.
    /// @note 此处只判断已知格式的块大小. 录制入口核对两端格式是否相同, 设备核对各自 target 的支持情况.
    [[nodiscard]] inline bool OrdinaryCopyFormat(const FormatInfo &format) noexcept
    {
        return format.blockSize == 1;
    }

    /// @brief OpenGL staging 的块格式只复制完整 mip, 普通格式可复制已经界检的矩形.
    /// @pre 两端格式与矩形尺寸相同, 两个 slice 均已解析且位于各自选定的 mip 和层内.
    [[nodiscard]] inline bool StagingCopyRegionSupported(const FormatInfo &format, const TextureDesc &dest,
                                                         const TextureSlice &dst, const TextureDesc &source,
                                                         const TextureSlice &src) noexcept
    {
        return format.blockSize == 1 || (WholeSlice(dest, dst) && WholeSlice(source, src));
    }

    /// @brief 公共 CPU staging 只提供普通复制方向及匹配的 map, 无 GPU 绑定角色.
    [[nodiscard]] inline bool DedicatedBuffer(const BufferDesc &desc) noexcept
    {
        const auto mode = desc.cpuAccess;
        const auto copyState = mode == CpuAccessMode::Read ? ResourceStates::CopyDest : ResourceStates::CopySource;
        return (mode == CpuAccessMode::Read || mode == CpuAccessMode::Write) &&
               desc.structStride == 0 && desc.maxVersions == 0 && desc.format == Format::UNKNOWN &&
               !desc.canHaveUAVs && !desc.canHaveRawViews && !desc.canHaveTypedViews &&
               !desc.isVertexBuffer && !desc.isIndexBuffer && !desc.isConstantBuffer && !desc.isDrawIndirectArgs &&
               !desc.isVolatile && !desc.isVirtual && !desc.isAccelStructBuildInput && !desc.isAccelStructStorage &&
               !desc.isShaderBindingTable && desc.sharedResourceFlags == SharedResourceFlags::None &&
               (desc.initialState == ResourceStates::Common || desc.initialState == ResourceStates::Unknown ||
                desc.initialState == copyState);
    }

    /// @brief 未解析的维度访问在公共描述层拒绝, 设备负责目标实际能力和地址可表示性.
    [[nodiscard]] inline bool StagingTextureShape(const TextureDesc &desc) noexcept
    {
        return Planar(desc.dimension) && desc.sampleCount == 1 && desc.sampleQuality == 0 &&
               !desc.isVirtual && !desc.isTiled && !desc.isTypeless &&
               !desc.isShadingRateSurface && desc.sharedResourceFlags == SharedResourceFlags::None;
    }

    /// @brief 颜色 resolve 只接受对应的二维与二维数组目标.
    [[nodiscard]] inline bool ResolveTargets(TextureDimension dest, TextureDimension source) noexcept
    {
        return (source == TextureDimension::Texture2DMS && dest == TextureDimension::Texture2D) ||
               (source == TextureDimension::Texture2DMSArray && dest == TextureDimension::Texture2DArray);
    }
}
