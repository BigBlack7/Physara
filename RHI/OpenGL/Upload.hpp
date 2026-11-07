#pragma once

/// @file
/// @brief 物理包共用的资源 lease 与上传预算/chunk/CPU footprint, 退休由 Device owner 推进.

#include "Backend.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Physara::RHI::OpenGL
{
    inline constexpr std::size_t UploadCapacityLimit = 64 * 1024 * 1024;
    inline constexpr std::size_t UploadItemLimit = 4096;
    inline constexpr std::size_t SmallBufferLimit = 256 * 1024;

    /// @brief 可精确记账的内部强引用, 构造/退休均由 owner 执行, worker 不销毁 lease.
    class UploadResourceLease
    {
    public:
        /// @brief 供包的空 vector count 构造满足元素约束, 不建立实际 lease; Debug 分配可传播.
        UploadResourceLease() : m_Dependencies(0)
        {
        }
        /// @brief 取得 wrapper 的包引用, 仅首个内部父 owner 计入实际子引用边, 多包不重复记账.
        /// @note dependencies 借用子对象中的账目, 每个元素对应一条真实强引用边, 不负责持有子对象.
        UploadResourceLease(OpenGLDevice &device, Resource *resource, InternalOwnership &owners,
                            std::vector<InternalOwnership *> dependencies = std::vector<InternalOwnership *>(0));
        ~UploadResourceLease();
        UploadResourceLease(UploadResourceLease &&other) noexcept;
        UploadResourceLease &operator=(UploadResourceLease &&) = delete;
        UploadResourceLease(const UploadResourceLease &) = delete;
        UploadResourceLease &operator=(const UploadResourceLease &) = delete;
        [[nodiscard]] Resource *Get() const noexcept;
        /// @brief 使用者停止后核对全部强引用是否来自包 lease/已保活复合对象, 不作为并发同步.
        [[nodiscard]] bool OnlyInternalOwners() const noexcept;
        [[nodiscard]] InternalOwnership *Owners() const noexcept
        {
            return m_Owners;
        }

    private:
        OpenGLDevice *m_Device = nullptr;
        RefCountPtr<Resource> m_Resource;
        InternalOwnership *m_Owners = nullptr;
        std::vector<InternalOwnership *> m_Dependencies;
    };

    /// @brief 包独有的 persistent/coherent 存储, used 单调增长, 退休前不覆写, capacity 计入设备总预算.
    struct UploadChunk
    {
        // 这里只描述已取得的原生存储, 销毁/解映射由 owning/current 的 RetirePacket 统一完成.
        GLuint name = 0;
        std::byte *mapping = nullptr;
        std::size_t capacity = 0;
        std::size_t used = 0;
    };

    /// @brief chunk 中一段连续借用, cpu 用于同步复制, buffer/offset 用于 GL 发出, 不跨包退休保留.
    struct UploadSlice
    {
        GLuint buffer = 0;
        std::size_t offset = 0;
        std::byte *cpu = nullptr;
    };

    /// @brief 已推导的源布局与紧凑上传大小, 所有跨度以字节计.
    /// sourceBytes 包含行/切片间隙且不含尾 padding, tightBytes 仅含 texel, 两者服务不同的边界检查.
    struct TextureUploadLayout
    {
        std::size_t rowBytes = 0;
        std::size_t rowStride = 0;
        std::size_t sliceStride = 0;
        std::size_t sourceBytes = 0;
        std::size_t tightBytes = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t depth = 0;
    };
    /// @brief 校验子资源和源 footprint, 不读取 CPU 源; false 时 layout 不可当作成功结果使用.
    /// @pre desc 已由 Device 创建验证且 format 已通过颜色上传筛选.
    [[nodiscard]] bool BuildUploadLayout(const TextureDesc &desc, std::uint32_t slice, std::uint32_t mip,
                                         std::size_t rowPitch, std::size_t depthPitch,
                                         TextureUploadLayout &layout) noexcept;
    /// @brief 对正二次幂对齐执行受检上取整, 非法对齐或溢出返回 false 且不写 result.
    [[nodiscard]] bool CheckedAlign(std::size_t value, std::size_t alignment, std::size_t &result) noexcept;
}
