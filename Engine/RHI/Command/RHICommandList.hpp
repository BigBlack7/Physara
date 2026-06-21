#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <glm/vec4.hpp>

#include <Engine/RHI/RHIDefinitions.hpp>
#include <Engine/RHI/Descriptors/RHIIndirectDrawCommand.hpp>
#include <Engine/RHI/Descriptors/RHIRenderPrimitive.hpp>
#include <Engine/RHI/Descriptors/RHIResourceSet.hpp>

namespace Physara::RHI
{
    class RHIPipelineState;
    class RHIBuffer;
    class RHITexture;
    class RHISampler;
    class RHIFramebuffer;
    struct RHIRenderPassDesc;

    struct RHITextureReadbackDesc
    {
        std::uint32_t x{0};
        std::uint32_t y{0};
        std::uint32_t width{0};
        std::uint32_t height{0};
        std::uint32_t mipLevel{0};
        std::uint32_t arrayLayer{0};
        TextureFormat format{TextureFormat::RGBA8};
    };

    class RHICommandList
    {
    public:
        virtual ~RHICommandList() = default;

        // 状态设置
        virtual void SetPipelineState(RHIPipelineState *pso) = 0;
        virtual void SetVertexBuffer(std::uint32_t binding, RHIBuffer *buffer, std::uint32_t offset = 0) = 0;
        virtual void SetIndexBuffer(RHIBuffer *buffer, std::uint32_t offset = 0) = 0;
        virtual void SetRenderPrimitive(const RHIRenderPrimitive &primitive)
        {
            for (const RHIVertexBufferBinding &binding : primitive.vertexBuffers)
            {
                SetVertexBuffer(binding.slot, binding.buffer, binding.offset);
            }
            SetIndexBuffer(primitive.indexBuffer.buffer, primitive.indexBuffer.offset);
        }
        virtual void SetUniformBuffer(std::uint32_t slot, RHIBuffer *buffer) = 0;
        virtual void SetUniformBuffer(std::uint32_t slot, RHIBuffer *buffer, std::uint32_t offset, std::uint32_t size) = 0;
        virtual void SetTexture(std::uint32_t slot, RHITexture *texture, RHISampler *sampler) = 0;
        virtual void SetResourceSet(std::uint32_t setIndex, const RHIResourceSet &resourceSet)
        {
            (void)setIndex;
            for (const RHITextureBinding &binding : resourceSet.textures)
            {
                SetTexture(binding.slot, binding.texture, binding.sampler);
            }
        }
        virtual void SetStorageBuffer(std::uint32_t slot, RHIBuffer *buffer) = 0;
        virtual void SetStorageBuffer(std::uint32_t slot, RHIBuffer *buffer, std::uint32_t offset, std::uint32_t size) = 0;
        virtual void SetStorageTexture(
            std::uint32_t slot,
            RHITexture *texture,
            std::uint32_t mipLevel = 0,
            std::uint32_t arrayLayer = 0,
            StorageTextureAccess access = StorageTextureAccess::ReadWrite) = 0;

        virtual void SetViewport(
            float x,
            float y,
            float width,
            float height,
            float minDepth = 0.f,
            float maxDepth = 1.f) = 0;

        virtual void SetScissor(
            std::int32_t x,
            std::int32_t y,
            std::uint32_t width,
            std::uint32_t height) = 0;

        void SetViewport(const RHIViewport &viewport)
        {
            SetViewport(viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth);
        }

        void SetScissor(const RHIRect2D &rect)
        {
            SetScissor(rect.x, rect.y, rect.width, rect.height);
        }

        virtual void PushConstants(
            ShaderStage stage,
            const void *data,
            std::uint32_t size,
            std::uint32_t offset = 0) = 0;

        // 渲染Pass
        virtual void BeginRenderPass(
            RHIFramebuffer *framebuffer,
            const RHIRenderPassDesc &desc,
            std::span<const glm::vec4> clearColors,
            float clearDepth = 1.f) = 0;

        void BeginRenderPass(
            RHIFramebuffer *framebuffer,
            const RHIRenderPassDesc &desc,
            const std::vector<RHIClearValue> &clearValues)
        {
            std::vector<glm::vec4> clearColors;
            clearColors.reserve(clearValues.size());
            float clearDepth = 1.f;

            for (const RHIClearValue &clearValue : clearValues)
            {
                clearColors.emplace_back(
                    clearValue.color[0],
                    clearValue.color[1],
                    clearValue.color[2],
                    clearValue.color[3]);
                clearDepth = clearValue.depth;
            }

            BeginRenderPass(framebuffer, desc, std::span<const glm::vec4>(clearColors.data(), clearColors.size()), clearDepth);
        }

        virtual void EndRenderPass() = 0;

        // 索引绘制(支持实例起始偏移)
        virtual void DrawIndexed(
            std::uint32_t indexCount,
            std::uint32_t instanceCount = 1,
            std::uint32_t firstIndex = 0,
            std::int32_t vertexOffset = 0,
            std::uint32_t firstInstance = 0) = 0;

        // MDI(Multi-Draw Indirect)
        virtual void DrawIndexedIndirect(
            RHIBuffer *indirectBuffer,
            std::uint32_t drawCount,
            std::uint32_t stride = sizeof(RHIDrawIndexedIndirectCommand),
            std::uint32_t offset = 0) = 0;

        // 顺序绘制(支持实例起始偏移)
        virtual void Draw(
            std::uint32_t vertexCount,
            std::uint32_t instanceCount = 1,
            std::uint32_t firstVertex = 0,
            std::uint32_t firstInstance = 0) = 0;

        // Compute任务
        virtual void Dispatch(
            std::uint32_t groupX,
            std::uint32_t groupY,
            std::uint32_t groupZ) = 0;

        // 资源同步
        virtual void TextureBarrier(RHITexture *texture, ShaderStage srcStage, ShaderStage dstStage) = 0;
        virtual void BufferBarrier(RHIBuffer *buffer, ShaderStage srcStage, ShaderStage dstStage) = 0;

        virtual void TextureBarrier(RHITexture *texture, const RHIResourceBarrier &barrier)
        {
            TextureBarrier(texture, FirstStage(barrier.srcStages), FirstStage(barrier.dstStages));
        }

        virtual void BufferBarrier(RHIBuffer *buffer, const RHIResourceBarrier &barrier)
        {
            BufferBarrier(buffer, FirstStage(barrier.srcStages), FirstStage(barrier.dstStages));
        }

        // 工具
        virtual void CopyTextureToTexture(RHITexture *src, RHITexture *dst) = 0;
        virtual void ResolveTexture(RHITexture *src, RHITexture *dst) = 0;
        virtual void CopyBufferToTexture(RHIBuffer *src, RHITexture *dst) = 0;
        virtual void GenerateMipmaps(RHITexture *texture) = 0;
        virtual std::vector<std::uint8_t> ReadTextureToCPU(RHITexture *texture, const RHITextureReadbackDesc &desc) = 0;

        virtual void BeginDebugLabel(const char *label) = 0;
        virtual void EndDebugLabel() = 0;
        virtual void SetGPUTimingEnabled(bool enabled) { (void)enabled; }
        virtual void BeginGPUTimingFrame() {}
        virtual void EndGPUTimingFrame() {}
        virtual void BeginGPUTimingScope(std::uint32_t scopeIndex) { (void)scopeIndex; }
        virtual void EndGPUTimingScope(std::uint32_t scopeIndex) { (void)scopeIndex; }
        [[nodiscard]] virtual RHIGPUTimingResult GetGPUTimingResult(std::uint32_t scopeIndex) const
        {
            (void)scopeIndex;
            return {};
        }
        virtual void SetBarrierDebugContext(std::string_view passName, std::string_view resourceName)
        {
            (void)passName;
            (void)resourceName;
        }
        [[nodiscard]] virtual std::vector<RHIBarrierDiagnostic> GetBarrierDiagnostics() const { return {}; }
        virtual void InvalidateResourceBindings() {}
        virtual void InvalidateExternalState() {}

        virtual void ResetStatistics() {}
        [[nodiscard]] virtual RHICommandStatistics GetStatistics() const { return {}; }

    private:
        static ShaderStage FirstStage(ShaderStageFlags stages)
        {
            if ((stages & ShaderStageBit::Vertex) != 0u)
            {
                return ShaderStage::Vertex;
            }
            if ((stages & ShaderStageBit::Fragment) != 0u)
            {
                return ShaderStage::Fragment;
            }
            return ShaderStage::Compute;
        }
    };
}
