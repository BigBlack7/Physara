#include "ShadowPass.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Engine/Renderer/PipelineStateCache.hpp>
#include <Engine/Renderer/RenderProxy.hpp>
#include <Engine/Renderer/RenderView.hpp>
#include <Engine/Renderer/UploadHasher.hpp>
#include <Engine/Resource/ShaderLibrary.hpp>
#include <Engine/Resource/Types/Mesh.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHIBufferDesc.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>
#include <Engine/RHI/Pipeline/RHIFramebuffer.hpp>
#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>

namespace Physara::Engine
{
    namespace ShadowPassDetail
    {
        constexpr std::uint32_t CameraBinding = 0u;
        constexpr std::uint32_t ObjectBinding = 1u;
        constexpr std::uint32_t VertexStride = sizeof(MeshVertex);

        template <typename T>
        constexpr T MaxValue(T lhs, T rhs)
        {
            return lhs < rhs ? rhs : lhs;
        }

        RHI::RHIBufferDesc DynamicBufferDesc(std::uint32_t size, RHI::BufferUsageFlags usage)
        {
            RHI::RHIBufferDesc desc{};
            desc.size = MaxValue(size, 16u);
            desc.usage = usage;
            desc.dynamic = true;
            return desc;
        }

        bool IsShadowedDirectionalLight(const LightData &light)
        {
            return static_cast<std::uint32_t>(light.directionType.w) == static_cast<std::uint32_t>(RenderLightType::Directional) &&
                   light.shadowParams.x > 0.5f;
        }

        glm::vec3 PickStableUpVector(const glm::vec3 &direction)
        {
            const glm::vec3 worldUp(0.f, 1.f, 0.f);
            if (std::abs(glm::dot(direction, worldUp)) > 0.95f)
            {
                return {0.f, 0.f, 1.f};
            }
            return worldUp;
        }

        void ExpandBounds(glm::vec3 &minBounds, glm::vec3 &maxBounds, const glm::vec3 &center, float radius)
        {
            const glm::vec3 extent(std::max(radius, 0.05f));
            minBounds = glm::min(minBounds, center - extent);
            maxBounds = glm::max(maxBounds, center + extent);
        }

        bool BuildWorldCasterBounds(const RenderDrawBuckets &buckets, glm::vec3 &minBounds, glm::vec3 &maxBounds)
        {
            minBounds = glm::vec3(std::numeric_limits<float>::max());
            maxBounds = glm::vec3(-std::numeric_limits<float>::max());
            bool hasBounds = false;
            for (const RenderDrawItem &item : buckets.shadowCasters)
            {
                if (item.submission == nullptr)
                {
                    continue;
                }

                const RenderMeshSubmission &submission = *item.submission;
                if (submission.hasBounds)
                {
                    ExpandBounds(minBounds, maxBounds, submission.boundsCenter, submission.boundsRadius);
                }
                else
                {
                    ExpandBounds(minBounds, maxBounds, glm::vec3(submission.model[3]), 1.f);
                }
                hasBounds = true;
            }
            return hasBounds;
        }

        glm::mat4 BuildSingleMapLightViewProjection(
            const RenderDrawBuckets &buckets,
            const glm::vec3 &lightDirection,
            std::uint32_t resolution)
        {
            glm::vec3 minWorld{};
            glm::vec3 maxWorld{};
            if (!BuildWorldCasterBounds(buckets, minWorld, maxWorld))
            {
                return glm::mat4(1.f);
            }

            const glm::vec3 center = (minWorld + maxWorld) * 0.5f;
            const float radius = std::max(glm::length(maxWorld - center), 1.f);
            const glm::mat4 lightView = glm::lookAt(center - lightDirection * radius * 2.5f, center, PickStableUpVector(lightDirection));

            glm::vec3 minLS(std::numeric_limits<float>::max());
            glm::vec3 maxLS(-std::numeric_limits<float>::max());
            for (float z : {minWorld.z, maxWorld.z})
            {
                for (float y : {minWorld.y, maxWorld.y})
                {
                    for (float x : {minWorld.x, maxWorld.x})
                    {
                        const glm::vec3 pointLS = glm::vec3(lightView * glm::vec4(x, y, z, 1.f));
                        minLS = glm::min(minLS, pointLS);
                        maxLS = glm::max(maxLS, pointLS);
                    }
                }
            }

            const float paddingXY = std::max(radius * 0.05f, 0.5f);
            minLS.x -= paddingXY;
            minLS.y -= paddingXY;
            maxLS.x += paddingXY;
            maxLS.y += paddingXY;

            const float minExtent = 1.f;
            if (maxLS.x - minLS.x < minExtent)
            {
                const float centerX = (minLS.x + maxLS.x) * 0.5f;
                minLS.x = centerX - minExtent * 0.5f;
                maxLS.x = centerX + minExtent * 0.5f;
            }
            if (maxLS.y - minLS.y < minExtent)
            {
                const float centerY = (minLS.y + maxLS.y) * 0.5f;
                minLS.y = centerY - minExtent * 0.5f;
                maxLS.y = centerY + minExtent * 0.5f;
            }

            const float safeResolution = static_cast<float>(std::max(resolution, 1u));
            const float texelSizeX = (maxLS.x - minLS.x) / safeResolution;
            const float texelSizeY = (maxLS.y - minLS.y) / safeResolution;
            minLS.x = std::floor(minLS.x / texelSizeX) * texelSizeX;
            maxLS.x = std::floor(maxLS.x / texelSizeX) * texelSizeX;
            minLS.y = std::floor(minLS.y / texelSizeY) * texelSizeY;
            maxLS.y = std::floor(maxLS.y / texelSizeY) * texelSizeY;

            const float depthPadding = std::max((maxLS.z - minLS.z) * 0.5f, 10.f);
            const float nearDistance = std::max(0.01f, -maxLS.z - depthPadding);
            const float farDistance = std::max(nearDistance + 0.01f, -minLS.z + depthPadding);
            const glm::mat4 lightProjection = glm::ortho(minLS.x, maxLS.x, minLS.y, maxLS.y, nearDistance, farDistance);
            return lightProjection * lightView;
        }

        bool CanInstanceTogether(const RenderDrawItem &first, const RenderDrawItem &candidate)
        {
            return first.submission != nullptr &&
                   candidate.submission != nullptr &&
                   first.primitiveKey == candidate.primitiveKey;
        }
    }

    void ShadowPass::Execute(const ShadowPassContext &context)
    {
        if (context.commandList == nullptr || context.device == nullptr || context.frameData == nullptr ||
            context.renderProxy == nullptr || context.meshCache == nullptr)
        {
            return;
        }

        context.frameData->shadow = {};
        if (m_Settings.algorithm == ShadowAlgorithm::None)
        {
            return;
        }

        CameraData shadowCamera{};
        std::uint32_t lightIndex = 0u;
        if (!BuildShadowData(context, shadowCamera, lightIndex))
        {
            return;
        }

        EnsureResources(context);
        RHI::RHIPipelineState *pipeline = GetPipeline(context);
        if (pipeline == nullptr || m_Framebuffer == nullptr)
        {
            context.frameData->shadow = {};
            return;
        }

        UploadFrameBuffers(context, shadowCamera);
        context.commandList->SetViewport(0.f, 0.f, static_cast<float>(m_Settings.resolution), static_cast<float>(m_Settings.resolution));
        context.commandList->SetScissor(0, 0, m_Settings.resolution, m_Settings.resolution);
        context.commandList->BeginRenderPass(m_Framebuffer.get(), m_RenderPassDesc, std::span<const glm::vec4>{}, 1.f);
        context.commandList->SetPipelineState(pipeline);
        context.commandList->SetUniformBuffer(ShadowPassDetail::CameraBinding, m_CameraBuffer.get());
        context.commandList->SetStorageBuffer(ShadowPassDetail::ObjectBinding, m_ObjectBuffer.get());
        DrawShadowCasters(context);
        context.commandList->EndRenderPass();

        context.frameData->shadow.params = glm::vec4(
            1.f,
            static_cast<float>(m_Settings.resolution),
            static_cast<float>(lightIndex),
            1.f / static_cast<float>(m_Settings.resolution));
        context.frameData->shadow.controls = glm::vec4(
            std::max(m_Settings.receiverBiasScale, 0.f),
            static_cast<float>(m_Settings.algorithm),
            m_Settings.filterRadiusTexels,
            m_Settings.lightSizeTexels);
    }

    void ShadowPass::Reset()
    {
        m_Framebuffer.reset();
        m_ShadowMap.reset();
        m_CameraBuffer.reset();
        m_ObjectBuffer.reset();
        m_LastCameraUploadSignature = std::numeric_limits<std::uint64_t>::max();
        m_LastObjectUploadSignature = std::numeric_limits<std::uint64_t>::max();
    }

    void ShadowPass::SetSettings(const ShadowSettings &settings)
    {
        ShadowSettings sanitized = settings;
        sanitized.resolution = std::clamp(sanitized.resolution, 256u, 8192u);
        sanitized.depthBias = std::max(sanitized.depthBias, 0.f);
        sanitized.slopeBias = std::max(sanitized.slopeBias, 0.f);
        sanitized.receiverBiasScale = std::max(sanitized.receiverBiasScale, 0.f);
        sanitized.filterRadiusTexels = std::clamp(sanitized.filterRadiusTexels, 0.25f, 32.f);
        sanitized.lightSizeTexels = std::clamp(sanitized.lightSizeTexels, 0.25f, 128.f);
        if (m_Settings.resolution != sanitized.resolution)
        {
            Reset();
        }
        m_Settings = sanitized;
    }

    void ShadowPass::EnsureResources(const ShadowPassContext &context)
    {
        if (m_RenderPassDesc.hasDepth == false)
        {
            m_RenderPassDesc = {};
            m_RenderPassDesc.hasDepth = true;
            m_RenderPassDesc.depthAttachment = {
                RHI::TextureFormat::Depth32F,
                RHI::LoadOp::Clear,
                RHI::StoreOp::Store,
                1u};
        }

        if (m_ShadowMap == nullptr)
        {
            RHI::RHITextureDesc desc{};
            desc.width = m_Settings.resolution;
            desc.height = m_Settings.resolution;
            desc.format = RHI::TextureFormat::Depth32F;
            desc.dimension = RHI::TextureDimension::Tex2D;
            desc.usage = RHI::TextureUsage::DepthStencil | RHI::TextureUsage::Sampled;
            desc.mipLevels = 1u;
            desc.arrayLayers = 1u;
            desc.samples = 1u;
            m_ShadowMap = context.device->CreateTexture(desc);
        }

        if (m_Framebuffer == nullptr && m_ShadowMap != nullptr)
        {
            RHI::RHIFramebufferDesc framebufferDesc{};
            framebufferDesc.depthAttachment = m_ShadowMap.get();
            framebufferDesc.width = m_Settings.resolution;
            framebufferDesc.height = m_Settings.resolution;
            framebufferDesc.renderPassDesc = &m_RenderPassDesc;
            m_Framebuffer = context.device->CreateFramebuffer(framebufferDesc);
        }

        if (m_CameraBuffer == nullptr)
        {
            m_CameraBuffer = context.device->CreateBuffer(
                ShadowPassDetail::DynamicBufferDesc(sizeof(CameraData), RHI::BufferUsage::Uniform));
            m_LastCameraUploadSignature = std::numeric_limits<std::uint64_t>::max();
        }

        const std::uint32_t objectBufferSize = static_cast<std::uint32_t>(
            ShadowPassDetail::MaxValue<std::size_t>(context.renderProxy->GetBuckets().shadowCasters.size(), 1u) * sizeof(glm::mat4));
        if (m_ObjectBuffer == nullptr || m_ObjectBuffer->GetSize() < objectBufferSize)
        {
            m_ObjectBuffer = context.device->CreateBuffer(
                ShadowPassDetail::DynamicBufferDesc(objectBufferSize, RHI::BufferUsage::Storage));
            m_LastObjectUploadSignature = std::numeric_limits<std::uint64_t>::max();
        }
    }

    bool ShadowPass::BuildShadowData(
        const ShadowPassContext &context,
        CameraData &shadowCamera,
        std::uint32_t &lightIndex)
    {
        const FrameData &frameData = *context.frameData;
        const RenderDrawBuckets &buckets = context.renderProxy->GetBuckets();
        if (buckets.shadowCasters.empty())
        {
            return false;
        }

        bool foundLight = false;
        for (std::uint32_t i = 0u; i < frameData.lights.size(); ++i)
        {
            if (ShadowPassDetail::IsShadowedDirectionalLight(frameData.lights[i]))
            {
                lightIndex = i;
                foundLight = true;
                break;
            }
        }
        if (!foundLight)
        {
            return false;
        }

        const LightData &light = frameData.lights[lightIndex];
        const glm::vec3 lightDirection = glm::normalize(glm::vec3(light.directionType));
        const glm::mat4 lightViewProjection = ShadowPassDetail::BuildSingleMapLightViewProjection(
            buckets,
            lightDirection,
            m_Settings.resolution);

        RenderView shadowView = RenderView::FromMatrices(
            glm::mat4(1.f),
            lightViewProjection,
            frameData.view.position,
            ViewportRect{0u, 0u, m_Settings.resolution, m_Settings.resolution},
            frameData.view.ev100,
            0.01f,
            1.f);
        shadowView.viewProjection = lightViewProjection;
        shadowCamera = BuildCameraData(shadowView);
        context.frameData->shadow.lightViewProjection = lightViewProjection;
        context.frameData->shadow.params = glm::vec4(
            1.f,
            static_cast<float>(m_Settings.resolution),
            static_cast<float>(lightIndex),
            1.f / static_cast<float>(m_Settings.resolution));
        context.frameData->shadow.controls = glm::vec4(
            std::max(m_Settings.receiverBiasScale, 0.f),
            static_cast<float>(m_Settings.algorithm),
            m_Settings.filterRadiusTexels,
            m_Settings.lightSizeTexels);
        return true;
    }

    RHI::RHIPipelineState *ShadowPass::GetPipeline(const ShadowPassContext &context)
    {
        if (context.shaderLibrary == nullptr || context.pipelineCache == nullptr)
        {
            return nullptr;
        }

        ShaderProgramDesc shaderDesc{};
        shaderDesc.debugName = "Shadow";
        shaderDesc.vertexPath = "Shaders/Passes/Shadow/Shadow.vert";
        shaderDesc.fragmentPath = "Shaders/Passes/Shadow/Shadow.frag";

        ShaderVariant *variant = context.shaderLibrary->GetVariant(shaderDesc);
        if (variant == nullptr || !variant->IsValid())
        {
            return nullptr;
        }

        RHI::RHIPipelineStateDesc pipelineDesc{};
        pipelineDesc.vertexShader = variant->vertexShader.get();
        pipelineDesc.fragmentShader = variant->fragmentShader.get();
        pipelineDesc.renderPassDesc = &m_RenderPassDesc;
        pipelineDesc.vertexBindings.push_back({0u, ShadowPassDetail::VertexStride, 0u});
        pipelineDesc.vertexAttributes.push_back({0u, 0u, RHI::VertexFormat::RGB32F, static_cast<std::uint32_t>(offsetof(MeshVertex, position))});
        pipelineDesc.rasterizerState.cullMode = RHI::CullMode::Front;
        pipelineDesc.rasterizerState.depthBias = m_Settings.depthBias;
        pipelineDesc.rasterizerState.depthBiasSlope = m_Settings.slopeBias;
        pipelineDesc.depthStencilState.depthTest = true;
        pipelineDesc.depthStencilState.depthWrite = true;
        pipelineDesc.depthStencilState.compareOp = RHI::DepthCompareOp::Less;
        return context.pipelineCache->GetOrCreate(pipelineDesc);
    }

    void ShadowPass::UploadFrameBuffers(const ShadowPassContext &context, const CameraData &shadowCamera)
    {
        const std::uint64_t cameraSignature = UploadHash::Value(UploadHash::Offset, shadowCamera);
        if (cameraSignature != m_LastCameraUploadSignature)
        {
            m_CameraBuffer->UploadData(&shadowCamera, sizeof(CameraData));
            if (context.stats != nullptr)
            {
                context.stats->bufferUploadBytes += sizeof(CameraData);
            }
            m_LastCameraUploadSignature = cameraSignature;
        }

        m_ObjectUploadScratch.clear();
        m_ObjectUploadScratch.reserve(context.renderProxy->GetBuckets().shadowCasters.size());
        for (const RenderDrawItem &item : context.renderProxy->GetBuckets().shadowCasters)
        {
            if (item.submission != nullptr)
            {
                m_ObjectUploadScratch.push_back(item.submission->model);
            }
        }

        const std::uint32_t objectBufferSize = static_cast<std::uint32_t>(
            ShadowPassDetail::MaxValue<std::size_t>(m_ObjectUploadScratch.size(), 1u) * sizeof(glm::mat4));
        const std::uint64_t objectSignature = UploadHash::Vector(UploadHash::Offset, m_ObjectUploadScratch);
        if (!m_ObjectUploadScratch.empty() && objectSignature != m_LastObjectUploadSignature)
        {
            m_ObjectBuffer->UploadData(m_ObjectUploadScratch.data(), objectBufferSize);
            if (context.stats != nullptr)
            {
                context.stats->bufferUploadBytes += objectBufferSize;
            }
        }
        m_LastObjectUploadSignature = objectSignature;
    }

    void ShadowPass::DrawShadowCasters(const ShadowPassContext &context)
    {
        std::uint32_t shadowObjectIndex = 0u;
        const std::vector<RenderDrawItem> &shadowCasters = context.renderProxy->GetBuckets().shadowCasters;
        for (std::size_t i = 0; i < shadowCasters.size();)
        {
            const RenderDrawItem &item = shadowCasters[i];
            if (item.submission == nullptr)
            {
                ++i;
                continue;
            }

            const std::uint32_t drawObjectIndex = shadowObjectIndex;
            MeshGPUPrimitive *primitive = context.meshCache->GetOrCreate(context.device, context.assetManager, item, context.stats);
            if (primitive == nullptr || primitive->indexCount == 0)
            {
                ++i;
                ++shadowObjectIndex;
                continue;
            }

            std::uint32_t instanceCount = 1u;
            while (i + instanceCount < shadowCasters.size() &&
                   ShadowPassDetail::CanInstanceTogether(item, shadowCasters[i + instanceCount]))
            {
                ++instanceCount;
            }

            context.commandList->SetVertexBuffer(0u, primitive->vertexBuffer.get());
            context.commandList->SetIndexBuffer(primitive->indexBuffer.get());
            context.commandList->DrawIndexed(primitive->indexCount, instanceCount, 0u, 0, drawObjectIndex);
            if (context.stats != nullptr)
            {
                ++context.stats->drawCalls;
                context.stats->instances += instanceCount;
                context.stats->triangles += static_cast<std::uint64_t>(primitive->indexCount / 3u) * instanceCount;
            }

            i += instanceCount;
            shadowObjectIndex += instanceCount;
        }
    }
}