#pragma once

// 物理上传包使用的预算、资源 lease、持久映射 chunk 和 CPU 源 footprint 合同; 退休由 Device owner 推进.

#include "Backend.hpp"

#include <cstddef>
#include <cstdint>

namespace Physara::RHI::OpenGL
{
    inline constexpr std::size_t UploadCapacityLimit = 64 * 1024 * 1024;
    inline constexpr std::size_t UploadItemLimit = 4096;
    inline constexpr std::size_t SmallBufferLimit = 256 * 1024;

    // 此强引用的来源可精确核对. 只由 owner 创建/退休, worker 不销毁 lease.
    class UploadResourceLease
    {
    public:
        UploadResourceLease() noexcept = default;
        UploadResourceLease(OpenGLDevice &device, Resource *resource, unsigned long &holds) noexcept;
        ~UploadResourceLease();
        UploadResourceLease(UploadResourceLease &&other) noexcept;
        UploadResourceLease &operator=(UploadResourceLease &&) = delete;
        UploadResourceLease(const UploadResourceLease &) = delete;
        UploadResourceLease &operator=(const UploadResourceLease &) = delete;
        [[nodiscard]] Resource *Get() const noexcept;
        [[nodiscard]] bool OnlyInternalOwners() const noexcept;

    private:
        OpenGLDevice *m_Device = nullptr;
        RefCountPtr<Resource> m_Resource;
        unsigned long *m_Holds = nullptr;
    };

    struct UploadChunk
    {
        // 包独有的 persistent/coherent 存储. used 单调增长, 退休前不会覆写; capacity 计入 Device 总预算.
        // 这里只描述已取得的原生存储, 销毁/解映射由 owning/current 的 RetirePacket 统一完成.
        GLuint name = 0;
        std::byte *mapping = nullptr;
        std::size_t capacity = 0;
        std::size_t used = 0;
    };

    struct UploadSlice
    {
        GLuint buffer = 0;
        std::size_t offset = 0;
        std::byte *cpu = nullptr;
    };

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
    // desc 已由 Device 创建验证且 format 已通过颜色上传筛选. false 时不读取源, layout 不可作为成功结果使用.
    // sourceBytes 是含行/切片间隙的最小可读范围, tightBytes 是复制到 PBO 的 texel 字节数, 均不含末尾 padding.
    [[nodiscard]] bool BuildUploadLayout(const TextureDesc &desc, std::uint32_t slice, std::uint32_t mip,
                                         std::size_t rowPitch, std::size_t depthPitch,
                                         TextureUploadLayout &layout) noexcept;
    [[nodiscard]] bool CheckedAlign(std::size_t value, std::size_t alignment, std::size_t &result) noexcept;
}
