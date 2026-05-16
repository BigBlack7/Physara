#include "LightSystem.hpp"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

#include <Engine/Scene/Components/LightComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Engine
{
    namespace LightSystemDetail
    {
        constexpr std::uint32_t MaxForwardLights = 128;

        glm::vec3 GetForwardDirection(const glm::mat4 &world)
        {
            return glm::normalize(glm::vec3(world * glm::vec4(0.f, 0.f, -1.f, 0.f)));
        }

        RenderLightType ToRenderLightType(LightType type)
        {
            switch (type)
            {
            case LightType::Directional:
                return RenderLightType::Directional;
            case LightType::Point:
                return RenderLightType::Point;
            case LightType::Spot:
                return RenderLightType::Spot;
            case LightType::Area:
                return RenderLightType::Area;
            }

            return RenderLightType::Directional;
        }

        float GetIntensity(const LightComponent &light)
        {
            switch (light.type)
            {
            case LightType::Directional:
                return light.directionalIlluminanceLux;
            case LightType::Point:
                return light.GetEffectiveLuminousIntensityCandela();
            case LightType::Spot:
                return light.spotLuminousIntensityCandela;
            case LightType::Area:
                return light.areaLuminanceCandelaPerSquareMeter;
            }

            return 0.f;
        }

        glm::vec2 GetSpotScaleOffset(const LightComponent &light)
        {
            const float cosInner = std::cos(light.innerConeAngleRadians);
            const float cosOuter = std::cos(light.outerConeAngleRadians);
            const float scale = 1.f / std::max(cosInner - cosOuter, 0.0001f);
            return {scale, -cosOuter * scale};
        }
    }

    std::vector<LightData> LightSystem::Collect(Scene &scene)
    {
        scene.UpdateTransforms();

        auto &registry = scene.GetRegistry();
        auto view = registry.view<LightComponent, TransformComponent>();

        std::vector<LightData> lights;
        lights.reserve(std::min<std::size_t>(view.size_hint(), LightSystemDetail::MaxForwardLights));

        view.each([&lights](EntityId, LightComponent component, const TransformComponent &transform)
        {
            if (lights.size() >= LightSystemDetail::MaxForwardLights)
            {
                return;
            }

            component.Sanitize();

            LightData light{};
            light.positionRange = glm::vec4(glm::vec3(transform.GetWorldMatrix()[3]), component.rangeMeters);
            light.directionType = glm::vec4(
                LightSystemDetail::GetForwardDirection(transform.GetWorldMatrix()),
                static_cast<float>(LightSystemDetail::ToRenderLightType(component.type)));
            light.colorIntensity = glm::vec4(component.color, LightSystemDetail::GetIntensity(component));

            const glm::vec2 spotScaleOffset = LightSystemDetail::GetSpotScaleOffset(component);
            light.spotAngles = glm::vec4(spotScaleOffset, component.innerConeAngleRadians, component.outerConeAngleRadians);
            light.shadowParams = glm::vec4(
                component.castsShadow ? 1.f : 0.f,
                component.shadowBias,
                component.sourceRadiusMeters,
                0.f);

            lights.push_back(light);
        });

        return lights;
    }
}