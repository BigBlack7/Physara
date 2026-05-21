#include "ShadowPass.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
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

        context.frameData->shadow.params.x = 1.f;
        context.frameData->shadow.params.y = static_cast<float>(m_Settings.resolution);
        context.frameData->shadow.params.z = static_cast<float>(lightIndex);
        context.frameData->shadow.params.w = 1.f / static_cast<float>(m_Settings.resolution);
        context.frameData->shadow.controls.x = std::max(m_Settings.receiverBiasScale, 0.f);
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
            ShadowPassDetail::MaxValue<std::size_t>(context.frameData->objects.size(), 1u) * sizeof(ObjectData));
        if (m_ObjectBuffer == nullptr || m_ObjectBuffer->GetSize() < objectBufferSize)
        {
            m_ObjectBuffer = context.device->CreateBuffer(
                ShadowPassDetail::DynamicBufferDesc(objectBufferSize, RHI::BufferUsage::Storage));
            m_LastObjectUploadSignature = std::numeric_limits<std::uint64_t>::max();
        }
    }

    bool ShadowPass::BuildShadowData(const ShadowPassContext &context, CameraData &shadowCamera, std::uint32_t &lightIndex)
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

        bool hasBounds = false;
        glm::vec3 minBounds(0.f);
        glm::vec3 maxBounds(0.f);
        for (const RenderDrawItem &item : buckets.shadowCasters)
        {
            if (item.objectIndex >= frameData.objects.size())
            {
                continue;
            }

            const glm::vec4 bounds = frameData.objects[item.objectIndex].boundsCenterRadius;
            const glm::vec3 radius(bounds.w);
            const glm::vec3 itemMin = glm::vec3(bounds) - radius;
            const glm::vec3 itemMax = glm::vec3(bounds) + radius;
            if (!hasBounds)
            {
                minBounds = itemMin;
                maxBounds = itemMax;
                hasBounds = true;
            }
            else
            {
                minBounds = glm::min(minBounds, itemMin);
                maxBounds = glm::max(maxBounds, itemMax);
            }
        }
        if (!hasBounds)
        {
            return false;
        }

        const LightData &light = frameData.lights[lightIndex];
        const glm::vec3 lightDirection = glm::normalize(glm::vec3(light.directionType));
        const glm::vec3 center = (minBounds + maxBounds) * 0.5f;
        const float radius = std::max(glm::length(maxBounds - minBounds) * 0.5f, 1.f);
        const glm::vec3 lightPosition = center - lightDirection * radius * 2.f;
        const glm::mat4 view = glm::lookAt(lightPosition, center, ShadowPassDetail::PickStableUpVector(lightDirection));
        const glm::mat4 projection = glm::ortho(-radius, radius, -radius, radius, 0.1f, radius * 4.f);

        RenderView shadowView = RenderView::FromMatrices(
            view,
            projection,
            lightPosition,
            ViewportRect{0u, 0u, m_Settings.resolution, m_Settings.resolution},
            frameData.view.ev100,
            0.1f,
            radius * 4.f);
        shadowCamera = BuildCameraData(shadowView);
        context.frameData->shadow.lightViewProjection = shadowView.viewProjection;
        context.frameData->shadow.params = glm::vec4(1.f, static_cast<float>(m_Settings.resolution), static_cast<float>(lightIndex), 1.f / static_cast<float>(m_Settings.resolution));
        context.frameData->shadow.controls = glm::vec4(std::max(m_Settings.receiverBiasScale, 0.f), 0.f, 0.f, 0.f);
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
        pipelineDesc.rasterizerState.cullMode = RHI::CullMode::Back;
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

        const std::uint32_t objectBufferSize = static_cast<std::uint32_t>(
            ShadowPassDetail::MaxValue<std::size_t>(context.frameData->objects.size(), 1u) * sizeof(ObjectData));
        const std::uint64_t objectSignature = UploadHash::Vector(UploadHash::Offset, context.frameData->objects);
        if (!context.frameData->objects.empty() && objectSignature != m_LastObjectUploadSignature)
        {
            m_ObjectBuffer->UploadData(context.frameData->objects.data(), objectBufferSize);
            if (context.stats != nullptr)
            {
                context.stats->bufferUploadBytes += objectBufferSize;
            }
        }
        m_LastObjectUploadSignature = objectSignature;
    }

    void ShadowPass::DrawShadowCasters(const ShadowPassContext &context)
    {
        for (const RenderDrawItem &item : context.renderProxy->GetBuckets().shadowCasters)
        {
            MeshGPUPrimitive *primitive = context.meshCache->GetOrCreate(context.device, context.assetManager, item, context.stats);
            if (primitive == nullptr || primitive->indexCount == 0)
            {
                continue;
            }

            context.commandList->SetVertexBuffer(0u, primitive->vertexBuffer.get());
            context.commandList->SetIndexBuffer(primitive->indexBuffer.get());
            context.commandList->DrawIndexed(primitive->indexCount, 1u, 0u, 0, item.objectIndex);
            if (context.stats != nullptr)
            {
                ++context.stats->drawCalls;
                ++context.stats->instances;
                context.stats->triangles += primitive->indexCount / 3u;
            }
        }
    }
}