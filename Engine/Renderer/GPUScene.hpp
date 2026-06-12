#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/FrameUploadAllocator.hpp>
#include <Engine/RHI/Resource/RHIBuffer.hpp>

namespace Physara::RHI
{
    class RHIDevice;
}

namespace Physara::Engine
{
    class AssetManager;
    class IBLResources;
    class RenderProxy;

    class GPUScene final
    {
    public:
        void Reset();
        void Release();

        void UploadFrame(
            RHI::RHIDevice &device,
            FrameUploadAllocator &allocator,
            const FrameData &frameData,
            const RenderProxy &renderProxy,
            const AssetManager *assetManager,
            FrameStatistics *stats);

        void UploadFrameUniforms(
            RHI::RHIDevice &device,
            FrameUploadAllocator &allocator,
            const FrameData &frameData,
            const IBLResources *iblResources,
            float environmentExposureCompensation,
            std::uint32_t debugView,
            FrameStatistics *stats);

        void UploadShadowInstanceObjectIndices(
            RHI::RHIDevice &device,
            FrameUploadAllocator &allocator,
            const std::vector<std::uint32_t> &instanceObjectIndices,
            FrameStatistics *stats);

        [[nodiscard]] const FrameUploadAllocation &GetFrameUniformBuffer() const { return m_FrameUniformAllocation; }
        [[nodiscard]] const FrameUploadAllocation &GetObjectBuffer() const { return m_ObjectAllocation; }
        [[nodiscard]] const FrameUploadAllocation &GetLightBuffer() const { return m_LightAllocation; }
        [[nodiscard]] const FrameUploadAllocation &GetForwardInstanceObjectIndexBuffer() const { return m_ForwardInstanceObjectIndexAllocation; }
        [[nodiscard]] const FrameUploadAllocation &GetShadowInstanceObjectIndexBuffer() const { return m_ShadowInstanceObjectIndexAllocation; }
        [[nodiscard]] RHI::RHIBuffer *GetMaterialBuffer() const { return m_MaterialBuffer.get(); }

        [[nodiscard]] std::uint32_t GetObjectCount() const { return m_ObjectCount; }
        [[nodiscard]] std::uint32_t GetLightCount() const { return m_LightCount; }
        [[nodiscard]] std::uint32_t GetMaterialCount() const { return m_MaterialCount; }
        [[nodiscard]] std::uint32_t GetForwardInstanceObjectIndexCount() const { return m_ForwardInstanceObjectIndexCount; }
        [[nodiscard]] std::uint32_t GetShadowInstanceObjectIndexCount() const { return m_ShadowInstanceObjectIndexCount; }

    private:
        FrameUploadAllocation UploadObjectTable(
            RHI::RHIDevice &device,
            FrameUploadAllocator &allocator,
            const std::vector<ObjectData> &objects,
            FrameStatistics *stats);

        FrameUploadAllocation UploadInstanceObjectIndices(
            RHI::RHIDevice &device,
            FrameUploadAllocator &allocator,
            const std::vector<std::uint32_t> &indices,
            FrameStatistics *stats);

        FrameUploadAllocation UploadLightTable(
            RHI::RHIDevice &device,
            FrameUploadAllocator &allocator,
            const std::vector<LightData> &lights,
            FrameStatistics *stats);

        void UploadMaterialTable(
            RHI::RHIDevice &device,
            const std::vector<MaterialComponent> &materials,
            const AssetManager *assetManager,
            FrameStatistics *stats);

    private:
        FrameUploadAllocation m_FrameUniformAllocation{};
        FrameUploadAllocation m_ObjectAllocation{};
        FrameUploadAllocation m_LightAllocation{};
        FrameUploadAllocation m_ForwardInstanceObjectIndexAllocation{};
        FrameUploadAllocation m_ShadowInstanceObjectIndexAllocation{};
        std::unique_ptr<RHI::RHIBuffer> m_MaterialBuffer{};
        std::vector<std::uint8_t> m_LightUploadScratch{};
        std::vector<MaterialGPUData> m_MaterialUploadScratch{};
        std::uint32_t m_ObjectCount{0};
        std::uint32_t m_LightCount{0};
        std::uint32_t m_MaterialCount{0};
        std::uint32_t m_ForwardInstanceObjectIndexCount{0};
        std::uint32_t m_ShadowInstanceObjectIndexCount{0};
        std::uint64_t m_LastMaterialUploadSignature{std::numeric_limits<std::uint64_t>::max()};
    };
}