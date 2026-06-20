#pragma once

#include <cstdint>
#include <array>
#include <vector>

#include <Engine/Renderer/GPUContracts.hpp>
#include <Engine/Renderer/MaterialInstance.hpp>
#include <Engine/Renderer/RenderView.hpp>
#include <Engine/RHI/RHIDefinitions.hpp>
#include <Engine/Scene/Components/MaterialComponent.hpp>

namespace Physara::Engine
{
    struct FrameStatistics
    {
        std::uint32_t visibleSubmissions{0};
        std::uint32_t opaqueItems{0};
        std::uint32_t unlitItems{0};
        std::uint32_t transparentItems{0};
        std::uint32_t lightCount{0};
        std::uint32_t materialInstances{0};
        std::uint32_t materialResourceSets{0};
        std::uint32_t clusterCount{0};
        std::uint32_t clusterLightReferences{0};
        std::uint32_t maxLightsPerCluster{0};
        std::uint32_t meshUploads{0};
        std::uint32_t meshPrimitiveUploads{0};
        std::uint32_t textureUploads{0};
        std::uint32_t drawBatches{0};
        std::uint32_t shadowBatches{0};
        std::uint32_t forwardOpaqueBatches{0};
        std::uint32_t forwardTransparentBatches{0};
        std::uint64_t drawCalls{0};
        std::uint64_t shadowDrawCalls{0};
        std::uint64_t forwardOpaqueDrawCalls{0};
        std::uint64_t skyboxDrawCalls{0};
        std::uint64_t forwardTransparentDrawCalls{0};
        std::uint64_t deferredGBufferDrawCalls{0};
        std::uint64_t deferredLightingDrawCalls{0};
        std::uint64_t postProcessDrawCalls{0};
        std::uint64_t instances{0};
        std::uint64_t triangles{0};
        std::uint64_t bufferUploadBytes{0};
        std::uint64_t bufferUploadChunks{0};
        std::uint64_t meshUploadBytes{0};
        std::uint64_t textureUploadBytes{0};
        float sceneBuildCpuMs{0.f};
        float renderGraphCpuMs{0.f};
        float shadowCpuMs{0.f};
        float forwardOpaqueCpuMs{0.f};
        float skyboxCpuMs{0.f};
        float forwardTransparentCpuMs{0.f};
        float deferredGBufferCpuMs{0.f};
        float deferredLightingCpuMs{0.f};
        float postProcessCpuMs{0.f};
        RHI::RHICommandStatistics backend{};

        void Reset();
        [[nodiscard]] std::uint64_t TotalUploadBytes() const;
    };

    struct FrameData
    {
        RenderView view{};
        CameraData camera{};
        ShadowData shadow{};
        std::vector<ObjectData> objects{};
        std::vector<MaterialComponent> materials{};
        std::vector<MaterialInstanceId> materialInstanceIds{};
        std::vector<std::uint64_t> materialSignatures{};
        std::vector<LightData> lights{};
        ClusterGridData clusterGrid{};
        std::vector<ClusterEntryGPU> clusterEntries{};
        std::vector<std::uint32_t> clusterLightIndices{};
        FrameStatistics stats{};
        std::uint64_t frameIndex{0};
        float deltaTimeSeconds{0.f};

        void Reset(const RenderView &renderView, std::uint64_t newFrameIndex, float deltaTime);
    };

    [[nodiscard]] CameraData BuildCameraData(const RenderView &view);
}