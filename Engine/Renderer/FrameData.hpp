#pragma once

#include <cstdint>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <Engine/Renderer/RenderView.hpp>

namespace Physara::Engine
{
    enum class RenderLightType : std::uint32_t
    {
        Directional = 0,
        Point = 1,
        Spot = 2,
        Area = 3
    };

    struct CameraData
    {
        glm::mat4 view{1.f};
        glm::mat4 projection{1.f};
        glm::mat4 viewProjection{1.f};
        glm::mat4 inverseView{1.f};
        glm::mat4 inverseProjection{1.f};
        glm::mat4 inverseViewProjection{1.f};

        glm::vec4 cameraPositionEV100{0.f, 0.f, 0.f, 0.f};
        glm::vec4 viewportRect{0.f, 0.f, 1.f, 1.f};
        glm::vec4 clipPlanes{0.1f, 1000.f, 0.f, 0.f};
    };

    struct ObjectData
    {
        glm::mat4 model{1.f};
        glm::mat4 inverseTransposeModel{1.f};
        glm::vec4 boundsCenterRadius{0.f, 0.f, 0.f, 0.f};
        std::uint32_t objectId{0};
        std::uint32_t meshIndex{0};
        std::uint32_t materialIndex{0};
        std::uint32_t flags{0};
    };

    struct LightData
    {
        glm::vec4 positionRange{0.f, 0.f, 0.f, 0.f};
        glm::vec4 directionType{0.f, -1.f, 0.f, 0.f};
        glm::vec4 colorIntensity{1.f, 1.f, 1.f, 0.f};
        glm::vec4 spotAngles{0.f, 0.f, 0.f, 0.f};
        glm::vec4 shadowParams{0.f, 0.f, 0.f, 0.f};
    };

    struct FrameData
    {
        RenderView view{};
        CameraData camera{};
        std::vector<ObjectData> objects{};
        std::vector<LightData> lights{};
        std::uint64_t frameIndex{0};
        float deltaTimeSeconds{0.f};

        void Reset(const RenderView &renderView, std::uint64_t newFrameIndex, float deltaTime);
    };

    [[nodiscard]] CameraData BuildCameraData(const RenderView &view);
}