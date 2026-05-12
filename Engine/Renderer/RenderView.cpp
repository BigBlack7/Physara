#include "RenderView.hpp"

#include <algorithm>

#include <glm/gtc/matrix_inverse.hpp>

namespace Physara::Engine
{
    float RenderView::GetAspectRatio() const
    {
        return static_cast<float>(std::max(viewport.width, 1u)) /
               static_cast<float>(std::max(viewport.height, 1u));
    }

    RenderView RenderView::FromMatrices(const glm::mat4 &viewMatrix,
                                        const glm::mat4 &projectionMatrix,
                                        const glm::vec3 &worldPosition,
                                        const ViewportRect &viewportRect,
                                        float exposureValue100,
                                        float nearClip,
                                        float farClip)
    {
        RenderView result{};
        result.view = viewMatrix;
        result.projection = projectionMatrix;
        result.viewProjection = projectionMatrix * viewMatrix;
        result.inverseView = glm::inverse(viewMatrix);
        result.inverseProjection = glm::inverse(projectionMatrix);
        result.inverseViewProjection = glm::inverse(result.viewProjection);
        result.position = worldPosition;
        result.ev100 = exposureValue100;
        result.viewport = viewportRect;
        result.nearClipMeters = nearClip;
        result.farClipMeters = farClip;
        return result;
    }
}