#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec4.hpp>

#include <Engine/RHI/RHIDefinitions.hpp>

namespace Physara::RHI
{
    class RHIPipelineState;
    class RHIBuffer;
    class RHITexture;
    class RHISampler;
    class RHIFramebuffer;
    struct RHIRenderPassDesc;

    class RHICommandList
    {
    public:
        virtual ~RHICommandList() = default;

        // 状态设置
        virtual void SetPipelineState(RHIPipelineState *pso) = 0;
        virtual void SetVertexBuffer(std::uint32_t binding, RHIBuffer *buffer, std::uint32_t offset = 0) = 0;
        virtual void SetIndexBuffer(RHIBuffer *buffer, std::uint32_t offset = 0) = 0;
        virtual void SetUniformBuffer(std::uint32_t slot, RHIBuffer *buffer) = 0;
        virtual void SetTexture(std::uint32_t slot, RHITexture *texture, RHISampler *sampler) = 0;
        virtual void SetStorageBuffer(std::uint32_t slot, RHIBuffer *buffer) = 0;

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

        virtual void PushConstants(
            ShaderStage stage,
            const void *data,
            std::uint32_t size,
            std::uint32_t offset = 0) = 0;

        // 渲染Pass
        virtual void BeginRenderPass(
            RHIFramebuffer *framebuffer,
            const RHIRenderPassDesc &desc,
            const std::vector<glm::vec4> &clearColors,
            float clearDepth = 1.f) = 0;

        virtual void EndRenderPass() = 0;

        // 索引绘制(支持实例起始偏移)
        virtual void DrawIndexed(
            std::uint32_t indexCount,
            std::uint32_t instanceCount = 1,
            std::uint32_t firstIndex = 0,
            std::int32_t vertexOffset = 0,
            std::uint32_t firstInstance = 0) = 0;

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

        // 工具
        virtual void CopyTextureToTexture(RHITexture *src, RHITexture *dst) = 0;
        virtual void CopyBufferToTexture(RHIBuffer *src, RHITexture *dst) = 0;
        virtual void GenerateMipmaps(RHITexture *texture) = 0;

        virtual void BeginDebugLabel(const char *label) = 0;
        virtual void EndDebugLabel() = 0;
    };
}