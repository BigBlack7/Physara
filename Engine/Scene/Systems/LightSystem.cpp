#include "LightSystem.hpp"

#include <algorithm>
#include <cmath>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
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

        float SrgbChannelToLinear(float value)
        {
            value = std::clamp(value, 0.f, 1.f);
            if (value <= 0.04045f)
            {
                return value / 12.92f;
            }
            return std::pow((value + 0.055f) / 1.055f, 2.4f);
        }

        glm::vec3 ColorTemperatureToLinearRgb(float kelvin)
        {
            kelvin = std::clamp(kelvin, 1000.f, 40000.f) / 100.f;

            float red = 1.f;
            if (kelvin > 66.f)
            {
                red = 1.292936186062745f * std::pow(kelvin - 60.f, -0.1332047592f);
            }

            float green = 1.f;
            if (kelvin <= 66.f)
            {
                green = 0.3900815787690196f * std::log(kelvin) - 0.6318414437886275f;
            }
            else
            {
                green = 1.129890860895294f * std::pow(kelvin - 60.f, -0.0755148492f);
            }

            float blue = 1.f;
            if (kelvin < 19.f)
            {
                blue = 0.f;
            }
            else if (kelvin < 66.f)
            {
                blue = 0.5432067891101961f * std::log(kelvin - 10.f) - 1.19625408914f;
            }

            glm::vec3 linear(
                SrgbChannelToLinear(red),
                SrgbChannelToLinear(green),
                SrgbChannelToLinear(blue));

            const float luminance = glm::dot(linear, glm::vec3(0.2126f, 0.7152f, 0.0722f));
            return luminance > 0.0001f ? linear / luminance : glm::vec3(1.f);
        }

        glm::vec3 GetLightColor(const LightComponent &light)
        {
            glm::vec3 color = glm::max(light.color, glm::vec3(0.f));
            if (light.useColorTemperature)
            {
                color *= ColorTemperatureToLinearRgb(light.colorTemperatureKelvin);
            }
            return color;
        }

        glm::vec2 GetSpotScaleOffset(const LightComponent &light)
        {
            const float cosInner = std::cos(light.innerConeAngleRadians);
            const float cosOuter = std::cos(light.outerConeAngleRadians);
            const float scale = 1.f / std::max(cosInner - cosOuter, 0.0001f);
            return {scale, -cosOuter * scale};
        }

        LightData BuildDefaultViewerLight()
        {
            LightData light{};
            light.positionRange = glm::vec4(0.f, 0.f, 0.f, 0.f);
            light.directionType = glm::vec4(glm::normalize(glm::vec3(-0.35f, -0.8f, -0.45f)), static_cast<float>(RenderLightType::Directional));
            light.colorIntensity = glm::vec4(1.f, 1.f, 1.f, 25000.f);
            light.spotAngles = glm::vec4(0.f);
            light.shadowParams = glm::vec4(0.f);
            return light;
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
            light.colorIntensity = glm::vec4(LightSystemDetail::GetLightColor(component), LightSystemDetail::GetIntensity(component));

            const glm::vec2 spotScaleOffset = LightSystemDetail::GetSpotScaleOffset(component);
            light.spotAngles = glm::vec4(spotScaleOffset, component.innerConeAngleRadians, component.outerConeAngleRadians);
            light.shadowParams = glm::vec4(
                component.castsShadow ? 1.f : 0.f,
                component.shadowBias,
                component.sourceRadiusMeters,
                0.f);

            lights.push_back(light);
        });

        if (lights.empty())
        {
            lights.push_back(LightSystemDetail::BuildDefaultViewerLight());
        }

        return lights;
    }
}