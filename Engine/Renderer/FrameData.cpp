#include "FrameData.hpp"

namespace Physara::Engine
{
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
        objects.clear();
        lights.clear();
        frameIndex = newFrameIndex;
        deltaTimeSeconds = deltaTime;
    }
}