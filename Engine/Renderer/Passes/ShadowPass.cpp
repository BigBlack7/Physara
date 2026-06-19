#include "ShadowPass.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Engine/Renderer/FrustumPartition.hpp>
#include <Engine/Renderer/PipelineStateCache.hpp>
#include <Engine/Renderer/RenderProxy.hpp>
#include <Engine/Renderer/RenderView.hpp>
#include <Engine/Resource/ShaderLibrary.hpp>
#include <Engine/Resource/Types/Mesh.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/RHI/Descriptors/RHITextureDesc.hpp>
#include <Engine/RHI/Pipeline/RHIFramebuffer.hpp>
#include <Engine/RHI/Pipeline/RHIPipelineState.hpp>

namespace Physara::Engine
{
    namespace ShadowPassDetail
    {
        constexpr std::uint32_t CameraBinding = Binding(GPUBufferBinding::Camera);
        constexpr std::uint32_t ObjectBinding = Binding(GPUBufferBinding::Objects);
        constexpr std::uint32_t InstanceObjectIndexBinding = Binding(GPUBufferBinding::InstanceIndices);
        constexpr std::uint32_t VertexStride = sizeof(MeshVertex);

        bool IsShadowedDirectionalLight(const LightData &light)
        {
            return static_cast<std::uint32_t>(light.directionType.w) == GPUValue(LightTypeGPU::Directional) &&
                   light.shadowParams.x > 0.5f;
        }

        bool IsFiniteVec3(const glm::vec3 &value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        glm::vec3 SafeLightDirection(const glm::vec3 &direction)
        {
            const float lengthSq = glm::dot(direction, direction);
            if (!IsFiniteVec3(direction) || lengthSq <= 0.000001f)
            {
                return glm::normalize(glm::vec3(-0.35f, -0.8f, -0.45f));
            }
            return direction * (1.f / std::sqrt(lengthSq));
        }

        glm::vec3 PickStableUpVector(const glm::vec3 &direction)
        {
            const glm::vec3 worldUp(0.f, 1.f, 0.f);
            return std::abs(glm::dot(direction, worldUp)) > 0.95f
                       ? glm::vec3(0.f, 0.f, 1.f)
                       : worldUp;
        }

        void ExpandBounds(glm::vec3 &minBounds, glm::vec3 &maxBounds, const glm::vec3 &center, float radius)
        {
            const glm::vec3 extent(std::max(radius, 0.05f));
            minBounds = glm::min(minBounds, center - extent);
            maxBounds = glm::max(maxBounds, center + extent);
        }

        void GetItemBounds(const RenderDrawItem &item, glm::vec3 &center, float &radius)
        {
            const RenderMeshSubmission &submission = *item.submission;
            center = submission.hasBounds ? submission.boundsCenter : glm::vec3(submission.model[3]);
            radius = submission.hasBounds ? std::max(submission.boundsRadius, 0.05f) : 1.f;
        }

        bool ItemOverlapsLightSpaceXY(
            const glm::mat4 &lightView,
            const RenderDrawItem &item,
            const glm::vec2 &minBounds,
            const glm::vec2 &maxBounds)
        {
            if (item.submission == nullptr)
            {
                return false;
            }

            glm::vec3 center{};
            float radius = 0.f;
            GetItemBounds(item, center, radius);
            const glm::vec3 lightSpaceCenter = glm::vec3(lightView * glm::vec4(center, 1.f));
            return lightSpaceCenter.x + radius >= minBounds.x &&
                   lightSpaceCenter.x - radius <= maxBounds.x &&
                   lightSpaceCenter.y + radius >= minBounds.y &&
                   lightSpaceCenter.y - radius <= maxBounds.y;
        }

        float QuantizeCascadeRadius(float radius)
        {
            constexpr float RadiusQuantization = 16.f;
            return std::ceil(std::max(radius, 1.f) * RadiusQuantization) / RadiusQuantization;
        }

        glm::vec3 SnapCascadeCenter(
            const glm::vec3 &center,
            const glm::vec3 &lightDirection,
            const glm::vec3 &up,
            float texelWorldSize)
        {
            const glm::vec3 right = glm::normalize(glm::cross(lightDirection, up));
            const glm::vec3 lightUp = glm::normalize(glm::cross(right, lightDirection));
            const float centerX = glm::dot(center, right);
            const float centerY = glm::dot(center, lightUp);
            const float snappedX = std::floor(centerX / texelWorldSize + 0.5f) * texelWorldSize;
            const float snappedY = std::floor(centerY / texelWorldSize + 0.5f) * texelWorldSize;
            return center + right * (snappedX - centerX) + lightUp * (snappedY - centerY);
        }

        bool BuildCascade(
            const std::array<glm::vec3, 8> &corners,
            const std::vector<RenderDrawItem> &shadowCasters,
            const glm::vec3 &lightDirection,
            std::uint32_t resolution,
            glm::mat4 &lightViewProjection,
            float &texelWorldSize,
            std::vector<RenderDrawItem> &visibleShadowCasters)
        {
            glm::vec3 center(0.f);
            for (const glm::vec3 &corner : corners)
            {
                center += corner;
            }
            center /= static_cast<float>(corners.size());

            float radius = 0.f;
            for (const glm::vec3 &corner : corners)
            {
                radius = std::max(radius, glm::length(corner - center));
            }
            radius = QuantizeCascadeRadius(radius);
            radius = QuantizeCascadeRadius(
                radius + (2.f * radius) / static_cast<float>(std::max(resolution, 1u)));
            texelWorldSize = (2.f * radius) / static_cast<float>(std::max(resolution, 1u));

            const glm::vec3 up = PickStableUpVector(lightDirection);
            const glm::vec3 snappedCenter = SnapCascadeCenter(center, lightDirection, up, texelWorldSize);
            float eyeDistance = radius * 2.f + 10.f;
            glm::mat4 lightView = glm::lookAt(
                snappedCenter - lightDirection * eyeDistance,
                snappedCenter,
                up);

            glm::vec3 minReceiver{};
            glm::vec3 maxReceiver{};
            glm::vec3 minCaster{};
            glm::vec3 maxCaster{};
            auto collectLightSpaceBounds = [&]()
            {
                minReceiver = glm::vec3(std::numeric_limits<float>::max());
                maxReceiver = glm::vec3(-std::numeric_limits<float>::max());
                for (const glm::vec3 &corner : corners)
                {
                    const glm::vec3 lightSpaceCorner = glm::vec3(lightView * glm::vec4(corner, 1.f));
                    minReceiver = glm::min(minReceiver, lightSpaceCorner);
                    maxReceiver = glm::max(maxReceiver, lightSpaceCorner);
                }
                minReceiver.x = -radius;
                minReceiver.y = -radius;
                maxReceiver.x = radius;
                maxReceiver.y = radius;

                visibleShadowCasters.clear();
                visibleShadowCasters.reserve(shadowCasters.size());
                minCaster = minReceiver;
                maxCaster = maxReceiver;
                const glm::vec2 minXY(minReceiver);
                const glm::vec2 maxXY(maxReceiver);
                for (const RenderDrawItem &item : shadowCasters)
                {
                    if (!ItemOverlapsLightSpaceXY(lightView, item, minXY, maxXY))
                    {
                        continue;
                    }

                    visibleShadowCasters.push_back(item);
                    glm::vec3 centerWS{};
                    float boundsRadius = 0.f;
                    GetItemBounds(item, centerWS, boundsRadius);
                    const glm::vec3 centerLS = glm::vec3(lightView * glm::vec4(centerWS, 1.f));
                    ExpandBounds(minCaster, maxCaster, centerLS, boundsRadius);
                }
            };
            collectLightSpaceBounds();

            const float depthPadding = std::max(radius * 0.25f, 10.f);
            const float cameraShift = std::max(maxCaster.z + depthPadding + 0.01f, 0.f);
            if (cameraShift > 0.f)
            {
                eyeDistance += cameraShift;
                lightView = glm::lookAt(
                    snappedCenter - lightDirection * eyeDistance,
                    snappedCenter,
                    up);
                collectLightSpaceBounds();
            }
            const float nearDistance = std::max(0.01f, -maxCaster.z - depthPadding);
            const float farDistance = std::max(nearDistance + 0.01f, -minCaster.z + depthPadding);
            const glm::mat4 lightProjection = glm::ortho(
                minReceiver.x,
                maxReceiver.x,
                minReceiver.y,
                maxReceiver.y,
                nearDistance,
                farDistance);
            lightViewProjection = lightProjection * lightView;
            return true;
        }
    }

    void ShadowPass::Execute(const ShadowPassContext &context)
    {
        if (context.commandList == nullptr || context.device == nullptr || context.frameData == nullptr ||
            context.frameUploadAllocator == nullptr || context.gpuScene == nullptr || context.renderProxy == nullptr ||
            context.meshCache == nullptr)
        {
            return;
        }

        context.frameData->shadow = {};
        if (!m_Settings.enabled)
        {
            return;
        }

        std::uint32_t lightIndex = 0u;
        if (!BuildShadowData(context, lightIndex))
        {
            return;
        }

        PrepareResources(*context.device);
        RHI::RHIPipelineState *singleSidedPipeline = GetPipeline(context, RHI::CullMode::Front);
        RHI::RHIPipelineState *doubleSidedPipeline = GetPipeline(context, RHI::CullMode::None);
        if (singleSidedPipeline == nullptr || doubleSidedPipeline == nullptr || m_ShadowMap == nullptr)
        {
            context.frameData->shadow = {};
            return;
        }

        BuildShadowCommands(context);
        UploadFrameBuffers(context);
        context.frameUploadAllocator->Flush(context.stats);
        const FrameUploadAllocation &objectAllocation = context.gpuScene->GetObjectBuffer();
        const FrameUploadAllocation &instanceObjectIndexAllocation = context.gpuScene->GetShadowInstanceObjectIndexBuffer();
        if (!objectAllocation.IsValid() || !instanceObjectIndexAllocation.IsValid())
        {
            context.frameData->shadow = {};
            return;
        }

        m_CommandExecutor.BeginFrame();
        for (std::uint32_t cascadeIndex = 0u; cascadeIndex < m_Settings.cascadeCount; ++cascadeIndex)
        {
            CascadeState &cascade = m_Cascades[cascadeIndex];
            RHI::RHIFramebuffer *framebuffer = m_Framebuffers[cascadeIndex].get();
            if (framebuffer == nullptr || !cascade.cameraAllocation.IsValid())
            {
                context.frameData->shadow = {};
                return;
            }

            context.commandList->BeginRenderPass(framebuffer, m_RenderPassDesc, std::span<const glm::vec4>{}, 1.f);
            context.commandList->SetViewport(0.f, 0.f, static_cast<float>(m_Settings.resolution), static_cast<float>(m_Settings.resolution));
            context.commandList->SetScissor(0, 0, m_Settings.resolution, m_Settings.resolution);
            context.commandList->SetUniformBuffer(
                ShadowPassDetail::CameraBinding,
                cascade.cameraAllocation.buffer,
                cascade.cameraAllocation.offset,
                cascade.cameraAllocation.size);
            context.commandList->SetStorageBuffer(
                ShadowPassDetail::ObjectBinding,
                objectAllocation.buffer,
                objectAllocation.offset,
                objectAllocation.size);
            context.commandList->SetStorageBuffer(
                ShadowPassDetail::InstanceObjectIndexBinding,
                instanceObjectIndexAllocation.buffer,
                instanceObjectIndexAllocation.offset,
                instanceObjectIndexAllocation.size);
            context.commandList->SetPipelineState(singleSidedPipeline);
            DrawShadowCasters(context, cascade.singleSidedCommands);
            context.commandList->SetPipelineState(doubleSidedPipeline);
            DrawShadowCasters(context, cascade.doubleSidedCommands);
            context.commandList->EndRenderPass();
        }
    }

    void ShadowPass::Reset()
    {
        for (std::unique_ptr<RHI::RHIFramebuffer> &framebuffer : m_Framebuffers)
        {
            framebuffer.reset();
        }
        m_ShadowMap.reset();
        for (CascadeState &cascade : m_Cascades)
        {
            cascade = {};
        }
        m_ShadowInstanceObjectIndexScratch.clear();
        m_CommandExecutor.Reset();
    }

    void ShadowPass::SetSettings(const ShadowSettings &settings)
    {
        ShadowSettings sanitized = settings;
        sanitized.resolution = std::clamp(sanitized.resolution, 256u, 8192u);
        sanitized.cascadeCount = std::clamp(sanitized.cascadeCount, 1u, MaxShadowCascades);
        sanitized.maxDistanceMeters = std::clamp(sanitized.maxDistanceMeters, 1.f, 100000.f);
        sanitized.splitLambda = std::clamp(sanitized.splitLambda, 0.f, 1.f);
        sanitized.transitionFraction = std::clamp(sanitized.transitionFraction, 0.f, 0.3f);
        sanitized.depthBias = std::max(sanitized.depthBias, 0.f);
        sanitized.slopeBias = std::max(sanitized.slopeBias, 0.f);
        sanitized.normalBiasTexels = std::clamp(sanitized.normalBiasTexels, 0.f, 8.f);
        sanitized.receiverBiasScale = std::clamp(sanitized.receiverBiasScale, 0.f, 8.f);
        sanitized.filterRadiusTexels = std::clamp(sanitized.filterRadiusTexels, 0.25f, 8.f);
        sanitized.lightSizeTexels = std::clamp(sanitized.lightSizeTexels, 1.f, 128.f);
        if (m_Settings.resolution != sanitized.resolution ||
            m_Settings.cascadeCount != sanitized.cascadeCount)
        {
            Reset();
        }
        m_Settings = sanitized;
    }

    void ShadowPass::PrepareResources(RHI::RHIDevice &device)
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
            desc.dimension = RHI::TextureDimension::Tex2DArray;
            desc.usage = RHI::TextureUsage::DepthStencil | RHI::TextureUsage::Sampled;
            desc.mipLevels = 1u;
            desc.arrayLayers = m_Settings.cascadeCount;
            desc.samples = 1u;
            m_ShadowMap = device.CreateTexture(desc);
        }

        if (m_ShadowMap == nullptr)
        {
            return;
        }

        for (std::uint32_t cascadeIndex = 0u; cascadeIndex < m_Settings.cascadeCount; ++cascadeIndex)
        {
            if (m_Framebuffers[cascadeIndex] != nullptr)
            {
                continue;
            }

            RHI::RHIFramebufferDesc framebufferDesc{};
            framebufferDesc.depthAttachment = m_ShadowMap.get();
            framebufferDesc.width = m_Settings.resolution;
            framebufferDesc.height = m_Settings.resolution;
            framebufferDesc.arrayLayer = cascadeIndex;
            framebufferDesc.bindArrayLayer = true;
            framebufferDesc.renderPassDesc = &m_RenderPassDesc;
            m_Framebuffers[cascadeIndex] = device.CreateFramebuffer(framebufferDesc);
        }
    }

    bool ShadowPass::BuildShadowData(const ShadowPassContext &context, std::uint32_t &lightIndex)
    {
        FrameData &frameData = *context.frameData;
        const RenderDrawBuckets &buckets = context.renderProxy->GetBuckets();
        if (buckets.shadowCasters.empty())
        {
            return false;
        }

        bool foundLight = false;
        for (std::uint32_t index = 0u; index < frameData.lights.size(); ++index)
        {
            if (ShadowPassDetail::IsShadowedDirectionalLight(frameData.lights[index]))
            {
                lightIndex = index;
                foundLight = true;
                break;
            }
        }
        if (!foundLight)
        {
            return false;
        }

        const float nearDistance = std::max(frameData.view.nearClipMeters, 0.001f);
        const float farDistance = std::min(
            std::max(frameData.view.farClipMeters, nearDistance + 0.001f),
            std::max(m_Settings.maxDistanceMeters, nearDistance + 0.001f));
        const std::vector<float> splits = FrustumPartition::BuildPracticalDepthSplits(
            nearDistance,
            farDistance,
            m_Settings.cascadeCount,
            m_Settings.splitLambda);
        if (splits.size() != m_Settings.cascadeCount)
        {
            return false;
        }

        const glm::vec3 lightDirection = ShadowPassDetail::SafeLightDirection(
            glm::vec3(frameData.lights[lightIndex].directionType));
        float cascadeNear = nearDistance;
        for (std::uint32_t cascadeIndex = 0u; cascadeIndex < m_Settings.cascadeCount; ++cascadeIndex)
        {
            CascadeState &cascade = m_Cascades[cascadeIndex];
            const float cascadeFar = splits[cascadeIndex];
            const std::array<glm::vec3, 8> corners = FrustumPartition::BuildSliceCorners(
                frameData.view,
                cascadeNear,
                cascadeFar);

            glm::mat4 lightViewProjection(1.f);
            float texelWorldSize = 0.f;
            if (!ShadowPassDetail::BuildCascade(
                    corners,
                    buckets.shadowCasters,
                    lightDirection,
                    m_Settings.resolution,
                    lightViewProjection,
                    texelWorldSize,
                    cascade.shadowCasters))
            {
                return false;
            }

            cascade.camera = {};
            cascade.camera.viewProjection = lightViewProjection;
            cascade.camera.projection = lightViewProjection;
            cascade.camera.inverseProjection = glm::inverse(lightViewProjection);
            cascade.camera.inverseViewProjection = cascade.camera.inverseProjection;
            cascade.camera.viewportRect = glm::vec4(
                0.f,
                0.f,
                static_cast<float>(m_Settings.resolution),
                static_cast<float>(m_Settings.resolution));
            cascade.camera.clipPlanes = glm::vec4(0.01f, farDistance, 0.f, 0.f);
            frameData.shadow.lightViewProjection[cascadeIndex] = lightViewProjection;
            frameData.shadow.cascadeSplits[cascadeIndex] = cascadeFar;
            frameData.shadow.cascadeTexelWorldSize[cascadeIndex] = texelWorldSize;
            cascadeNear = cascadeFar;
        }

        for (std::uint32_t cascadeIndex = m_Settings.cascadeCount; cascadeIndex < MaxShadowCascades; ++cascadeIndex)
        {
            m_Cascades[cascadeIndex] = {};
            frameData.shadow.lightViewProjection[cascadeIndex] = glm::mat4(1.f);
            frameData.shadow.cascadeSplits[cascadeIndex] = farDistance;
            frameData.shadow.cascadeTexelWorldSize[cascadeIndex] = 0.f;
        }

        frameData.shadow.params = glm::vec4(
            1.f,
            static_cast<float>(m_Settings.resolution),
            static_cast<float>(lightIndex),
            1.f / static_cast<float>(m_Settings.resolution));
        frameData.shadow.controls = glm::vec4(
            static_cast<float>(m_Settings.cascadeCount),
            m_Settings.transitionFraction,
            m_Settings.normalBiasTexels,
            m_Settings.receiverBiasScale);
        frameData.shadow.samplingParams = glm::vec4(
            m_Settings.filterRadiusTexels,
            m_Settings.lightSizeTexels,
            static_cast<float>(m_Settings.filter),
            farDistance);
        return true;
    }

    RHI::RHIPipelineState *ShadowPass::GetPipeline(const ShadowPassContext &context, RHI::CullMode cullMode)
    {
        if (context.shaderLibrary == nullptr || context.pipelineCache == nullptr)
        {
            return nullptr;
        }

        ShaderProgramDesc shaderDesc{};
        shaderDesc.debugName = "ShadowCSM";
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
        pipelineDesc.rasterizerState.cullMode = cullMode;
        pipelineDesc.rasterizerState.depthBias = m_Settings.depthBias;
        pipelineDesc.rasterizerState.depthBiasSlope = m_Settings.slopeBias;
        pipelineDesc.depthStencilState.depthTest = true;
        pipelineDesc.depthStencilState.depthWrite = true;
        pipelineDesc.depthStencilState.compareOp = RHI::DepthCompareOp::Less;
        return context.pipelineCache->GetOrCreate(pipelineDesc);
    }

    void ShadowPass::BuildShadowCommands(const ShadowPassContext &context)
    {
        m_ShadowInstanceObjectIndexScratch.clear();
        std::uint32_t totalCommandCount = 0u;
        const std::uint32_t objectCount = static_cast<std::uint32_t>(context.frameData->objects.size());
        for (std::uint32_t cascadeIndex = 0u; cascadeIndex < m_Settings.cascadeCount; ++cascadeIndex)
        {
            CascadeState &cascade = m_Cascades[cascadeIndex];
            cascade.singleSidedCommands.clear();
            cascade.doubleSidedCommands.clear();
            cascade.singleSidedCommands.reserve(cascade.shadowCasters.size());
            cascade.doubleSidedCommands.reserve(cascade.shadowCasters.size());
            for (std::uint32_t itemIndex = 0u; itemIndex < cascade.shadowCasters.size(); ++itemIndex)
            {
                const RenderDrawItem &item = cascade.shadowCasters[itemIndex];
                if (item.submission == nullptr || item.objectIndex >= objectCount)
                {
                    continue;
                }

                std::vector<RenderCommand> &commands = item.doubleSided
                                                           ? cascade.doubleSidedCommands
                                                           : cascade.singleSidedCommands;
                if (!commands.empty() &&
                    item.primitiveKey == commands.back().primitiveKey)
                {
                    ++commands.back().instanceCount;
                    m_ShadowInstanceObjectIndexScratch.push_back(item.objectIndex);
                    continue;
                }

                RenderCommand command{};
                command.submission = item.submission;
                command.sourceItemIndex = itemIndex;
                command.instanceCount = 1u;
                command.firstObjectIndex = item.objectIndex;
                command.firstInstanceIndex = static_cast<std::uint32_t>(m_ShadowInstanceObjectIndexScratch.size());
                command.sortKey = item.sortKey;
                command.meshKey = item.meshKey;
                command.primitiveKey = item.primitiveKey;
                command.materialInstanceId = item.materialInstanceId;
                command.bucket = RenderBucket::Opaque;
                command.doubleSided = item.doubleSided;
                commands.push_back(command);
                m_ShadowInstanceObjectIndexScratch.push_back(item.objectIndex);
            }
            totalCommandCount += static_cast<std::uint32_t>(
                cascade.singleSidedCommands.size() + cascade.doubleSidedCommands.size());
        }

        if (context.stats != nullptr)
        {
            context.stats->shadowBatches = totalCommandCount;
            context.stats->drawBatches += totalCommandCount;
        }
    }

    void ShadowPass::UploadFrameBuffers(const ShadowPassContext &context)
    {
        for (std::uint32_t cascadeIndex = 0u; cascadeIndex < m_Settings.cascadeCount; ++cascadeIndex)
        {
            CascadeState &cascade = m_Cascades[cascadeIndex];
            cascade.cameraAllocation = context.frameUploadAllocator->Upload(
                *context.device,
                cascade.camera,
                context.stats);
        }
        context.gpuScene->UploadShadowInstanceObjectIndices(
            *context.device,
            *context.frameUploadAllocator,
            m_ShadowInstanceObjectIndexScratch,
            context.stats);
    }

    void ShadowPass::DrawShadowCasters(const ShadowPassContext &context, const std::vector<RenderCommand> &commands)
    {
        CommandSubmitContext submitContext{&context};
        RenderCommandExecutorContext executorContext{};
        executorContext.device = context.device;
        executorContext.commandList = context.commandList;
        executorContext.meshCache = context.meshCache;
        executorContext.assetManager = context.assetManager;
        executorContext.stats = context.stats;
        RenderCommandSubmitCallbacks callbacks{};
        callbacks.userData = &submitContext;
        callbacks.canMergeIndirectRun = &ShadowPass::CanMergeShadowIndirectRun;
        callbacks.recordCommand = &ShadowPass::RecordSubmittedCommand;
        m_CommandExecutor.Submit(executorContext, commands, callbacks);
    }

    bool ShadowPass::CanMergeShadowIndirectRun(void *userData, const RenderCommand &lhs, const RenderCommand &rhs)
    {
        (void)userData;
        return lhs.bucket == rhs.bucket;
    }

    void ShadowPass::RecordSubmittedCommand(
        void *userData,
        const RenderCommand &command,
        const MeshGPUPrimitive &primitive,
        RenderCommandSubmitMode mode)
    {
        (void)mode;
        auto *submitContext = static_cast<CommandSubmitContext *>(userData);
        if (submitContext == nullptr || submitContext->passContext == nullptr || submitContext->passContext->stats == nullptr)
        {
            return;
        }

        FrameStatistics &stats = *submitContext->passContext->stats;
        ++stats.drawCalls;
        ++stats.shadowDrawCalls;
        stats.instances += command.instanceCount;
        stats.triangles += static_cast<std::uint64_t>(primitive.indexCount / 3u) * command.instanceCount;
    }
}
