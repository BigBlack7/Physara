#include "LightSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <Engine/Renderer/RenderView.hpp>
#include <Engine/Scene/Components/LightComponent.hpp>
#include <Engine/Scene/Components/TransformComponent.hpp>
#include <Engine/Scene/Scene.hpp>

namespace Physara::Engine
{
    namespace LightSystemDetail
    {
        struct LightCandidate
        {
            LightData data{};
            float sortScore{0.f};
            std::uint32_t sequence{0};
        };

        bool IsFiniteVec3(const glm::vec3 &value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        glm::vec3 DefaultLightDirection()
        {
            return glm::normalize(glm::vec3(-0.35f, -0.8f, -0.45f));
        }

        glm::vec3 GetForwardDirection(const glm::mat4 &world)
        {
            const glm::vec3 forward = glm::vec3(world * glm::vec4(0.f, 0.f, -1.f, 0.f));
            const float lengthSq = glm::dot(forward, forward);
            if (!IsFiniteVec3(forward) || lengthSq <= 0.000001f)
            {
                return DefaultLightDirection();
            }
            return forward * (1.f / std::sqrt(lengthSq));
        }

        LightTypeGPU ToLightTypeGPU(LightType type)
        {
            switch (type)
            {
            case LightType::Directional:
                return LightTypeGPU::Directional;
            case LightType::Point:
                return LightTypeGPU::Point;
            case LightType::Spot:
                return LightTypeGPU::Spot;
            case LightType::Area:
                return LightTypeGPU::Area;
            }

            return LightTypeGPU::Directional;
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

        float ExposureFromEV100(float ev100)
        {
            return 1.f / (std::exp2(ev100) * 1.2f);
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

        LightData BuildDefaultViewerLight(const RenderView *view)
        {
            LightData light{};
            light.positionRange = glm::vec4(0.f, 0.f, 0.f, 0.f);
            light.directionType = glm::vec4(DefaultLightDirection(), static_cast<float>(GPUValue(LightTypeGPU::Directional)));
            light.colorIntensity = glm::vec4(1.f, 1.f, 1.f, 25000.f);
            if (view != nullptr)
            {
                light.colorIntensity.a *= ExposureFromEV100(view->ev100);
            }
            light.spotAngles = glm::vec4(0.f);
            light.shadowParams = glm::vec4(0.f);
            return light;
        }

        float ComputeSortScore(const LightComponent &light, const glm::vec3 &position, const RenderView *view)
        {
            const float intensity = std::max(GetIntensity(light), 0.f);
            const float shadowBonus = light.castsShadow ? 10'000.f : 0.f;
            if (light.type == LightType::Directional)
            {
                return 1'000'000'000.f + shadowBonus + intensity;
            }

            const float range = std::max(light.rangeMeters, 0.001f);
            const float rangeWeight = range * range;
            float distanceSq = 1.f;
            if (view != nullptr)
            {
                const glm::vec3 cameraToLight = position - view->position;
                distanceSq = std::max(glm::dot(cameraToLight, cameraToLight), 1.f);
            }

            const float typeWeight = light.type == LightType::Spot ? 1.25f : 1.f;
            return shadowBonus + typeWeight * intensity * rangeWeight / distanceSq;
        }
    }

    void LightSystem::Collect(Scene &scene, std::vector<LightData> &lights, const RenderView *view)
    {
        auto &registry = scene.GetRegistry();
        auto lightView = registry.view<LightComponent, TransformComponent>();

        lights.clear();
        lights.reserve(std::min<std::size_t>(lightView.size_hint(), MaxForwardLights));

        std::vector<LightSystemDetail::LightCandidate> candidates;
        candidates.reserve(lightView.size_hint());

        std::uint32_t sequence = 0u;
        lightView.each([&candidates, &sequence, view](EntityId, LightComponent component, const TransformComponent &transform)
        {
            component.Sanitize();
            if (component.type == LightType::Area)
            {
                return;
            }

            LightData light{};
            const glm::vec3 position = glm::vec3(transform.GetWorldMatrix()[3]);
            light.positionRange = glm::vec4(position, component.rangeMeters);
            light.directionType = glm::vec4(
                LightSystemDetail::GetForwardDirection(transform.GetWorldMatrix()),
                static_cast<float>(GPUValue(LightSystemDetail::ToLightTypeGPU(component.type))));
            light.colorIntensity = glm::vec4(LightSystemDetail::GetLightColor(component), LightSystemDetail::GetIntensity(component));
            if (component.type == LightType::Directional && view != nullptr)
            {
                light.colorIntensity.a *= LightSystemDetail::ExposureFromEV100(view->ev100);
            }

            const glm::vec2 spotScaleOffset = LightSystemDetail::GetSpotScaleOffset(component);
            light.spotAngles = glm::vec4(spotScaleOffset, component.innerConeAngleRadians, component.outerConeAngleRadians);
            light.shadowParams = glm::vec4(
                component.type == LightType::Directional && component.castsShadow ? 1.f : 0.f,
                component.shadowBias,
                component.sourceRadiusMeters,
                0.f);

            LightSystemDetail::LightCandidate candidate{};
            candidate.data = light;
            candidate.sortScore = LightSystemDetail::ComputeSortScore(component, position, view);
            candidate.sequence = sequence++;
            candidates.push_back(candidate);
        });

        std::sort(candidates.begin(), candidates.end(), [](const LightSystemDetail::LightCandidate &lhs, const LightSystemDetail::LightCandidate &rhs)
        {
            if (lhs.sortScore == rhs.sortScore)
            {
                return lhs.sequence < rhs.sequence;
            }
            return lhs.sortScore > rhs.sortScore;
        });

        const std::size_t lightCount = std::min<std::size_t>(candidates.size(), MaxForwardLights);
        for (std::size_t i = 0; i < lightCount; ++i)
        {
            lights.push_back(candidates[i].data);
        }

        if (lights.empty())
        {
            lights.push_back(LightSystemDetail::BuildDefaultViewerLight(view));
        }
    }
}