#pragma once

#include <algorithm>
#include <numbers>
#include <string>

#include <glm/common.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Physara::Engine
{
    enum class LightType
    {
        Directional, // 只允许有一个全局平行光, 模拟太阳光等远距离光源
        Point,
        Spot,
        Area
    };

    /*
        1000K-2000K: 暖红色(蜡烛、日出日落)
        2700K-3000K: 暖黄色(白炽灯、家用灯泡)
        4000K-5000K: 中性白(荧光灯、办公室灯光)
        6500K: 正白色(正午太阳光)
        8000K-10000K: 冷蓝色(阴天、月光)
    */
    struct LightComponent
    {
        LightType type{LightType::Directional};
        glm::vec3 color{1.f, 1.f, 1.f};
        float colorTemperatureKelvin{6500.f}; // 色温度, 单位-开尔文
        bool useColorTemperature{false};

        float directionalIlluminanceLux{100000.f};        // 平行光照度, 单位-勒克斯 lux
        float pointLuminousPowerLumens{800.f};            // 点光源发光强度, 单位-流明 lm
        float spotLuminousIntensityCandela{1000.f};       // 聚光灯发光强度, 单位-坎德拉 cd
        float areaLuminanceCandelaPerSquareMeter{1000.f}; // 面光源亮度, 单位-坎德拉每平方米 cd/m^2

        float rangeMeters{10.f};
        float sourceRadiusMeters{0.05f};
        glm::vec2 areaSizeMeters{1.f, 1.f};
        float innerConeAngleRadians{glm::radians(20.f)};
        float outerConeAngleRadians{glm::radians(35.f)};

        bool castsShadow{true};
        float shadowBias{0.001f};
        std::string iesProfilePath{};

        LightComponent() = default;
        explicit LightComponent(LightType lightType)
            : type(lightType)
        {
        }

        void Sanitize()
        {
            color = glm::max(color, glm::vec3(0.f));
            colorTemperatureKelvin = std::clamp(colorTemperatureKelvin, 1000.f, 40000.f);
            directionalIlluminanceLux = std::max(directionalIlluminanceLux, 0.f);
            pointLuminousPowerLumens = std::max(pointLuminousPowerLumens, 0.f);
            spotLuminousIntensityCandela = std::max(spotLuminousIntensityCandela, 0.f);
            areaLuminanceCandelaPerSquareMeter = std::max(areaLuminanceCandelaPerSquareMeter, 0.f);
            rangeMeters = std::max(rangeMeters, 0.001f);
            sourceRadiusMeters = std::max(sourceRadiusMeters, 0.f);
            areaSizeMeters = glm::max(areaSizeMeters, glm::vec2(0.001f));
            innerConeAngleRadians = std::clamp(innerConeAngleRadians, 0.f, std::numbers::pi_v<float>);
            outerConeAngleRadians = std::clamp(outerConeAngleRadians, innerConeAngleRadians, std::numbers::pi_v<float>);
            shadowBias = std::max(shadowBias, 0.f);
        }

        [[nodiscard]] float GetEffectiveLuminousIntensityCandela() const
        {
            if (type == LightType::Point)
            {
                // 点光源的发光强度 = 总光通量 / 4π (向所有方向均匀发光)
                return pointLuminousPowerLumens / (4.f * std::numbers::pi_v<float>);
            }

            return type == LightType::Spot ? spotLuminousIntensityCandela : 0.f;
        }

        [[nodiscard]] bool IsPunctual() const
        {
            return type == LightType::Directional || type == LightType::Point || type == LightType::Spot;
        }
    };
}