#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/mat4x4.hpp>

namespace Physara::Engine
{
    enum class CameraProjectionType
    {
        Perspective,
        Orthographic
    };

    struct CameraComponent
    {
        CameraProjectionType projectionType{CameraProjectionType::Perspective};
        bool primary{false}; // 是否是主相机

        /*
            36mm(全画幅)
            23.5mm(APS-C)
            17.3mm(M4/3)
        */
        float sensorWidthMillimeters{36.f}; // 相机传感器宽度, 单位-mm

        /*
            24mm(全画幅)
            15.6mm(APS-C)
            13.0mm(M4/3)
        */
        float sensorHeightMillimeters{24.f}; // 相机传感器高度, 单位-mm

        /*
            24mm(广角)
            35mm(人文)
            50mm(标准)
            85mm(人像)
            200mm(长焦)
        */
        float focalLengthMillimeters{35.f}; // 镜头焦距, 单位-mm

        /*
            f/1.4(大光圈, 浅景深)
            f/2.8(标准)
            f/8(小光圈, 大景深)
            f/16(极小光圈)
        */
        float apertureFStop{2.8f}; // 镜头光圈, 单位-f

        /*
            1/1000s(高速)
            1/60s(标准)
            1/30s(慢速)
            1s(长曝光)
        */
        float shutterTimeSeconds{1.f / 60.f}; // 快门速度, 单位-秒

        /*
            100(低感光度, 画质好)
            400(标准)
            1600(高感光度, 噪点多)
            6400(极高感光度)
        */
        float iso{100.f}; // 感光度, 单位-ISO

        float nearClipMeters{0.1f};
        float farClipMeters{1000.f};
        float orthographicHeightMeters{10.f};

        CameraComponent() = default;
        explicit CameraComponent(bool isPrimary) : primary(isPrimary) {}

        void Sanitize()
        {
            sensorWidthMillimeters = std::max(sensorWidthMillimeters, 1.f);
            sensorHeightMillimeters = std::max(sensorHeightMillimeters, 1.f);
            focalLengthMillimeters = std::max(focalLengthMillimeters, 1.f);
            apertureFStop = std::max(apertureFStop, 0.1f);
            shutterTimeSeconds = std::max(shutterTimeSeconds, 1.f / 32000.f);
            iso = std::max(iso, 1.f);
            nearClipMeters = std::max(nearClipMeters, 0.001f);
            farClipMeters = std::max(farClipMeters, nearClipMeters + 0.001f);
            orthographicHeightMeters = std::max(orthographicHeightMeters, 0.001f);
        }

        // 真实相机焦距到视野角-计算垂直视野角(弧度)
        [[nodiscard]] float GetVerticalFieldOfViewRadians() const
        {
            const float focalLength = std::max(focalLengthMillimeters, 1.f);
            const float sensorHeight = std::max(sensorHeightMillimeters, 1.f);
            return 2.f * std::atan(sensorHeight / (2.f * focalLength));
        }

        // 计算水平视野角(弧度)
        [[nodiscard]] float GetHorizontalFieldOfViewRadians() const
        {
            const float focalLength = std::max(focalLengthMillimeters, 1.f);
            const float sensorWidth = std::max(sensorWidthMillimeters, 1.f);
            return 2.f * std::atan(sensorWidth / (2.f * focalLength));
        }

        // 计算曝光值EV100
        [[nodiscard]] float GetEV100() const
        {
            const float n = std::max(apertureFStop, 0.1f);
            const float t = std::max(shutterTimeSeconds, std::numeric_limits<float>::min());
            const float sensitivity = std::max(iso, 1.f);
            return std::log2((n * n) / t * (100.f / sensitivity));
        }

        [[nodiscard]] glm::mat4 GetProjectionMatrix(float aspectRatio) const
        {
            const float safeAspect = std::max(aspectRatio, 0.001f);
            if (projectionType == CameraProjectionType::Orthographic)
            {
                const float halfHeight = orthographicHeightMeters * 0.5f;
                const float halfWidth = halfHeight * safeAspect;
                return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, nearClipMeters, farClipMeters);
            }

            return glm::perspective(GetVerticalFieldOfViewRadians(), safeAspect, nearClipMeters, farClipMeters);
        }
    };
}