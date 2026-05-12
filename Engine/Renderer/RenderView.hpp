#pragma once

#include <cstdint>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Physara::Engine
{
    struct ViewportRect
    {
        std::uint32_t x{0};
        std::uint32_t y{0};
        std::uint32_t width{1};
        std::uint32_t height{1};
    };

    struct RenderView
    {
        glm::mat4 view{1.f};
        glm::mat4 projection{1.f};
        glm::mat4 viewProjection{1.f};
        glm::mat4 inverseView{1.f};
        glm::mat4 inverseProjection{1.f};
        glm::mat4 inverseViewProjection{1.f};

        glm::vec3 position{0.f};
        float ev100{0.f};

        ViewportRect viewport{};
        float nearClipMeters{0.1f};
        float farClipMeters{1000.f};

        [[nodiscard]] float GetAspectRatio() const;

        static RenderView FromMatrices(const glm::mat4 &viewMatrix,
                                       const glm::mat4 &projectionMatrix,
                                       const glm::vec3 &worldPosition,
                                       const ViewportRect &viewportRect,
                                       float exposureValue100,
                                       float nearClip,
                                       float farClip);
    };
}