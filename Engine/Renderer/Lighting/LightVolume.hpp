#pragma once

/// @file
/// @brief 阴影接收体裁剪和 Froxel 共用的有限聚光影响体支持点.

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

namespace Physara::Renderer::Detail
{
    /// @brief 单位聚光体相对原点的支持点及沿查询轴的距离.
    struct SpotSupport
    {
        glm::dvec3 point;
        double distance;
    };

    /// @brief 查询方向在锥内时取单位球面点, 其余取投影最大的锥缘或锥尖.
    /// @pre 查询方向和发射轴为单位向量, cosine/sine 对应 [0, pi) 内的半锥角.
    /// @return point 位于单位半径的影响体内, 取锥尖时为零向量. distance 是该点沿查询方向的投影.
    /// 两者乘灯的实际半径后分别得到相对灯心的米制位移和投影距离.
    /// @note Froxel 以 distance 检查格子平面. 阴影 GJK 还用 point 构造下一步的单纯形.
    [[nodiscard]] inline SpotSupport SupportSpot(const glm::dvec3 &direction, const glm::dvec3 &axis,
                                                 double cosine, double sine) noexcept
    {
        const auto alignment = glm::dot(direction, axis);
        if (alignment >= cosine)
        {
            return {direction, 1};
        }
        // 叉乘最近极轴上的差向量, 横向方向来自真实偏离, 避免在近反平行时放大点积的轴向舍入残差.
        const auto pole = alignment >= 0 ? axis : -axis;
        const auto normal = glm::cross(axis, direction - pole);
        const auto lateralLength = std::hypot(normal.x, normal.y, normal.z);
        // 锥尖的投影为零. 锥缘的最大投影非正时, 锥尖就是支持点.
        if (alignment * cosine + lateralLength * sine <= 0)
        {
            return {glm::dvec3(0), 0};
        }
        glm::dvec3 rimDirection;
        if (lateralLength > 0)
        {
            rimDirection = glm::cross(normal / lateralLength, axis);
        }
        else
        {
            // 宽过半球时, 正背向在锥缘有正支持距离. 此时任一锥缘方向均达到该距离.
            const auto basis = std::abs(axis.x) < 0.5 ? glm::dvec3(1, 0, 0) : glm::dvec3(0, 1, 0);
            rimDirection = glm::normalize(glm::cross(axis, basis));
        }
        const auto rim = cosine * axis + sine * rimDirection;
        const auto distance = glm::dot(rim, direction);
        return distance > 0 ? SpotSupport{rim, distance} : SpotSupport{glm::dvec3(0), 0};
    }
}
