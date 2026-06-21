#include "FrameData.hpp"

#include <algorithm>
#include <cmath>

namespace Physara::Engine
{
    void FrameStatistics::Reset()
    {
        visibleSubmissions = 0;
        opaqueItems = 0;
        unlitItems = 0;
        transparentItems = 0;
        lightCount = 0;
        materialInstances = 0;
        materialResourceSets = 0;
        clusterCount = 0;
        clusterLightReferences = 0;
        maxLightsPerCluster = 0;
        localLightCount = 0;
        clusterOverflowedLightReferences = 0;
        meshUploads = 0;
        meshPrimitiveUploads = 0;
        textureUploads = 0;
        drawBatches = 0;
        shadowBatches = 0;
        forwardOpaqueBatches = 0;
        forwardTransparentBatches = 0;
        drawCalls = 0;
        shadowDrawCalls = 0;
        forwardOpaqueDrawCalls = 0;
        skyboxDrawCalls = 0;
        forwardTransparentDrawCalls = 0;
        deferredGBufferDrawCalls = 0;
        deferredLightingDrawCalls = 0;
        postProcessDrawCalls = 0;
        instances = 0;
        triangles = 0;
        bufferUploadBytes = 0;
        bufferUploadChunks = 0;
        meshUploadBytes = 0;
        textureUploadBytes = 0;
        deferredGBufferBytes = 0;
        directSubmittedCommands = 0;
        indirectRuns = 0;
        indirectRunCommands = 0;
        maxIndirectRunCommands = 0;
        indirectMergeBreaks = 0;
        indirectGeometryBreaks = 0;
        indirectInvalidBreaks = 0;
        indirectShortRuns = 0;
        sceneBuildCpuMs = 0.f;
        sceneCollectionCpuMs = 0.f;
        clusterBuildCpuMs = 0.f;
        renderGraphCpuMs = 0.f;
        renderGraphBuildCpuMs = 0.f;
        renderGraphExecuteCpuMs = 0.f;
        shadowCpuMs = 0.f;
        forwardOpaqueCpuMs = 0.f;
        skyboxCpuMs = 0.f;
        forwardTransparentCpuMs = 0.f;
        deferredGBufferCpuMs = 0.f;
        deferredLightingCpuMs = 0.f;
        postProcessCpuMs = 0.f;
        gpuFrameMs = 0.f;
        shadowGpuMs = 0.f;
        forwardOpaqueGpuMs = 0.f;
        skyboxGpuMs = 0.f;
        forwardTransparentGpuMs = 0.f;
        deferredGBufferGpuMs = 0.f;
        deferredLightingGpuMs = 0.f;
        worldGridGpuMs = 0.f;
        postProcessGpuMs = 0.f;
        bloomPrefilterGpuMs = 0.f;
        bloomDownsampleGpuMs = 0.f;
        bloomUpsampleGpuMs = 0.f;
        postProcessCompositeGpuMs = 0.f;
        benchmarkEnabled = false;
        benchmarkComplete = false;
        benchmarkWarmupFrame = 0;
        benchmarkWarmupFrames = 0;
        benchmarkSampleFrame = 0;
        benchmarkSampleFrames = 0;
        benchmarkCpuMedianMs = 0.f;
        benchmarkCpuP95Ms = 0.f;
        benchmarkGpuMedianMs = 0.f;
        benchmarkGpuP95Ms = 0.f;
        backend.Reset();
        barrierDiagnostics.clear();
    }

    std::uint64_t FrameStatistics::TotalUploadBytes() const
    {
        return bufferUploadBytes + meshUploadBytes + textureUploadBytes;
    }

    CameraData BuildCameraData(const RenderView &view)
    {
        CameraData camera{};
        camera.view = view.view;
        camera.projection = view.projection;
        camera.viewProjection = view.viewProjection;
        camera.inverseView = view.inverseView;
        camera.inverseProjection = view.inverseProjection;
        camera.inverseViewProjection = view.inverseViewProjection;
        camera.cameraPositionEV100 = glm::vec4(view.position, view.ev100);
        const float preExposure = 1.f / (std::exp2(view.ev100) * 1.2f);
        camera.exposure = glm::vec4(
            preExposure,
            1.f / std::max(preExposure, 0.000001f),
            0.f,
            0.f);
        camera.viewportRect = glm::vec4(
            static_cast<float>(view.viewport.x),
            static_cast<float>(view.viewport.y),
            static_cast<float>(view.viewport.width),
            static_cast<float>(view.viewport.height));
        camera.clipPlanes = glm::vec4(view.nearClipMeters, view.farClipMeters, 0.f, 0.f);
        return camera;
    }

    void FrameData::Reset(const RenderView &renderView, std::uint64_t newFrameIndex, float deltaTime)
    {
        view = renderView;
        camera = BuildCameraData(renderView);
        shadow = {};
        objects.clear();
        materials.clear();
        materialInstanceIds.clear();
        materialSignatures.clear();
        lights.clear();
        clusterGrid = {};
        clusterEntries.clear();
        clusterLightIndices.clear();
        stats.Reset();
        frameIndex = newFrameIndex;
        deltaTimeSeconds = deltaTime;
    }
}
