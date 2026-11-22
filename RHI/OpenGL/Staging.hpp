#pragma once

/// @file
/// @brief 公开 staging 的原生 Buffer 归属和逐子资源布局. Map 与复制共用同一 footprint.

#include "Backend.hpp"

namespace Physara::RHI::OpenGL
{
    inline constexpr std::size_t StagingCapacityLimit = 128 * 1024 * 1024;
    inline constexpr std::size_t StagingObjectLimit = 128;

    /// @brief 一个 mip 的一个层在共享 PBO 中占用的字节区间. rowPitch 是相邻像素行或压缩块行的起点间距.
    /// @note ready 按子资源记录. 局部复制只定义实际写到的矩形, 调用方保存该范围.
    struct StagingFootprint
    {
        std::size_t offset = 0;
        std::size_t rowPitch = 0;
        std::size_t bytes = 0;
        bool ready = false; ///< Read 为已接受且完成的内容, Write 为成功 Unmap 后的 CPU 内容.
    };

    /// @brief 全部 mip 和层共用一个固定方向的 PBO. 创建时排定布局, Map 和复制使用同一组偏移.
    /// @note CPU 借用和物理录制包独占整个 PBO. 调用方持有 Texture 句柄到 Unmap.
    class OpenGLStagingTexture final : public RefCounter<StagingTexture>
    {
    public:
        OpenGLStagingTexture(OpenGLDevice &device, const TextureDesc &desc, CpuAccessMode mode);
        ~OpenGLStagingTexture() override;
        [[nodiscard]] const TextureDesc &GetDesc() const override
        {
            return m_Desc;
        }

    private:
        [[nodiscard]] GLuint Name() const noexcept
        {
            return m_Deletion->name;
        }
        /// @pre slice 的 mip 和层已经过界检. 布局先按 mip 排列, 同级各层紧邻.
        [[nodiscard]] StagingFootprint &Footprint(const TextureSlice &slice) noexcept
        {
            return m_Footprints[std::size_t(slice.mipLevel) * m_Desc.arraySize + slice.arraySlice];
        }

        RefCountPtr<OpenGLDevice> m_Device;
        InternalOwnership m_InternalOwners;
        const TextureDesc m_Desc;
        const CpuAccessMode m_Mode;
        std::unique_ptr<NativeDeletion> m_Deletion;
        std::vector<StagingFootprint> m_Footprints = std::vector<StagingFootprint>(0);
        StagingAccess m_Access;
        bool m_Published = false;
        friend class OpenGLDevice;
        friend class OpenGLCommandList;
    };

    /// @brief 已分配的二维存储上校验单一显式矩形, 不把越界收缩成有效传输.
    [[nodiscard]] bool ResolvePlanarSlice(const TextureDesc &desc, const TextureSlice &input,
                                          TextureSlice &resolved) noexcept;
}
