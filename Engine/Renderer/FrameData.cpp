#include "FrameData.hpp"

namespace Physara::Engine
{
    void FrameStatistics::Reset()
    {
        visibleSubmissions = 0;
        opaqueItems = 0;
        unlitItems = 0;
        transparentItems = 0;
        lightCount = 0;
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
        postProcessDrawCalls = 0;
        instances = 0;
        triangles = 0;
        bufferUploadBytes = 0;
        meshUploadBytes = 0;
        textureUploadBytes = 0;
        sceneBuildCpuMs = 0.f;
        renderGraphCpuMs = 0.f;
        shadowCpuMs = 0.f;
        forwardOpaqueCpuMs = 0.f;
        skyboxCpuMs = 0.f;
        forwardTransparentCpuMs = 0.f;
        postProcessCpuMs = 0.f;
        backend.Reset();
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
        lights.clear();
        stats.Reset();
        frameIndex = newFrameIndex;
        deltaTimeSeconds = deltaTime;
    }
}