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
    enum class RendererGPUTimingScope : std::uint32_t
    {
        Frame = 0,
        Shadow,
        Skybox,
        GBuffer,
        DeferredLighting,
        ForwardOpaque,
        ForwardTransparent,
        WorldGrid,
        PostProcess,
        MSAAResolve,
        BloomPrefilter,
        BloomDownsample,
        BloomUpsample,
        PostProcessComposite
    };

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
        std::uint32_t localLightCount{0};
        std::uint32_t clusterOverflowedLightReferences{0};
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
        std::uint64_t deferredGBufferBytes{0};
        std::uint64_t directSubmittedCommands{0};
        std::uint64_t indirectRuns{0};
        std::uint64_t indirectRunCommands{0};
        std::uint64_t maxIndirectRunCommands{0};
        std::uint64_t indirectMergeBreaks{0};
        std::uint64_t indirectGeometryBreaks{0};
        std::uint64_t indirectInvalidBreaks{0};
        std::uint64_t indirectShortRuns{0};
        float sceneBuildCpuMs{0.f};
        float sceneCollectionCpuMs{0.f};
        float clusterBuildCpuMs{0.f};
        float renderGraphCpuMs{0.f};
        float renderGraphBuildCpuMs{0.f};
        float renderGraphExecuteCpuMs{0.f};
        float shadowCpuMs{0.f};
        float forwardOpaqueCpuMs{0.f};
        float skyboxCpuMs{0.f};
        float forwardTransparentCpuMs{0.f};
        float deferredGBufferCpuMs{0.f};
        float deferredLightingCpuMs{0.f};
        float postProcessCpuMs{0.f};
        float gpuFrameMs{0.f};
        float shadowGpuMs{0.f};
        float forwardOpaqueGpuMs{0.f};
        float skyboxGpuMs{0.f};
        float forwardTransparentGpuMs{0.f};
        float deferredGBufferGpuMs{0.f};
        float deferredLightingGpuMs{0.f};
        float worldGridGpuMs{0.f};
        float postProcessGpuMs{0.f};
        float bloomPrefilterGpuMs{0.f};
        float bloomDownsampleGpuMs{0.f};
        float bloomUpsampleGpuMs{0.f};
        float postProcessCompositeGpuMs{0.f};
        bool benchmarkEnabled{false};
        bool benchmarkComplete{false};
        std::uint32_t benchmarkWarmupFrame{0};
        std::uint32_t benchmarkWarmupFrames{0};
        std::uint32_t benchmarkSampleFrame{0};
        std::uint32_t benchmarkSampleFrames{0};
        float benchmarkCpuMedianMs{0.f};
        float benchmarkCpuP95Ms{0.f};
        float benchmarkGpuMedianMs{0.f};
        float benchmarkGpuP95Ms{0.f};
        RHI::RHICommandStatistics backend{};
        std::vector<RHI::RHIBarrierDiagnostic> barrierDiagnostics{};

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