#pragma once

#include <array>
#include <cstdint>

#include <glad/glad.h>

#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>

namespace Physara::RHI
{
    class OpenGLCommandList final : public RHICommandList
    {
    public:
        OpenGLCommandList();
        ~OpenGLCommandList() override;

        void SetPipelineState(RHIPipelineState *pso) override;
        void SetVertexBuffer(std::uint32_t binding, RHIBuffer *buffer, std::uint32_t offset = 0) override;
        void SetIndexBuffer(RHIBuffer *buffer, std::uint32_t offset = 0) override;
        void SetUniformBuffer(std::uint32_t slot, RHIBuffer *buffer) override;
        void SetTexture(std::uint32_t slot, RHITexture *texture, RHISampler *sampler) override;
        void SetStorageBuffer(std::uint32_t slot, RHIBuffer *buffer) override;

        void SetViewport(float x, float y, float width, float height, float minDepth = 0.f, float maxDepth = 1.f) override;
        void SetScissor(std::int32_t x, std::int32_t y, std::uint32_t width, std::uint32_t height) override;

        void PushConstants(ShaderStage stage, const void *data, std::uint32_t size, std::uint32_t offset = 0) override;

        void BeginRenderPass(
            RHIFramebuffer *framebuffer,
            const RHIRenderPassDesc &desc,
            const std::vector<glm::vec4> &clearColors,
            float clearDepth = 1.f) override;

        void EndRenderPass() override;

        void DrawIndexed(
            std::uint32_t indexCount,
            std::uint32_t instanceCount = 1,
            std::uint32_t firstIndex = 0,
            std::int32_t vertexOffset = 0,
            std::uint32_t firstInstance = 0) override;

        void Draw(
            std::uint32_t vertexCount,
            std::uint32_t instanceCount = 1,
            std::uint32_t firstVertex = 0,
            std::uint32_t firstInstance = 0) override;

        void Dispatch(std::uint32_t groupX, std::uint32_t groupY, std::uint32_t groupZ) override;

        void DrawIndexedIndirect(RHIBuffer *indirectBuffer, std::uint32_t drawCount, std::uint32_t stride) override;

        void TextureBarrier(RHITexture *texture, ShaderStage srcStage, ShaderStage dstStage) override;
        void BufferBarrier(RHIBuffer *buffer, ShaderStage srcStage, ShaderStage dstStage) override;
        void TextureBarrier(RHITexture *texture, const RHIResourceBarrier &barrier) override;
        void BufferBarrier(RHIBuffer *buffer, const RHIResourceBarrier &barrier) override;

        void CopyTextureToTexture(RHITexture *src, RHITexture *dst) override;
        void CopyBufferToTexture(RHIBuffer *src, RHITexture *dst) override;
        void GenerateMipmaps(RHITexture *texture) override;

        void BeginDebugLabel(const char *label) override;
        void EndDebugLabel() override;

    private:
        static constexpr std::uint32_t kMaxColorAttachments = 4;
        static constexpr std::uint32_t kPushConstantsSize = 256;

        struct GLState
        {
            GLuint program = 0;
            GLuint vao = 0;
            GLuint framebuffer = 0;

            CullMode cullMode = CullMode::Back;
            PolygonMode polygonMode = PolygonMode::Fill;
            float depthBias = 0.f;
            float depthBiasSlope = 0.f;

            bool depthTest = true;
            bool depthWrite = true;
            DepthCompareOp depthFunc = DepthCompareOp::Less;

            std::array<RHIBlendState, kMaxColorAttachments> blendStates{};

            GLenum topology = GL_TRIANGLES;

            std::uint32_t indexOffset = 0;
            GLenum indexType = GL_UNSIGNED_INT;
        };

        GLState m_State{};
        const RHIRenderPassDesc *m_CurrentPassDesc{nullptr};
        const RHIPipelineStateDesc *m_CurrentPipelineDesc{nullptr};

        GLuint m_PushConstantsBuffer{0};
    };
}