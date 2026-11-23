#pragma once

/// @file
/// @brief 主 View 和阴影准备共用的保守半空间判定. 输入使用米制世界坐标.

#include <cmath>
#include <limits>

#include <glm/vec3.hpp>

#include <Scene/Scene.hpp>

namespace Physara::Renderer::Detail
{
    /// @brief 以调用方提供的原点为基准的单位法线平面, 正半空间表示保留侧.
    struct CullingPlane
    {
        glm::dvec3 normal;
        double offset;
    };

    /// @brief AABB 沿方向最远的支持点, 用于构造投影区间或证明分离.
    inline glm::dvec3 SupportPoint(const Scene::Bounds &bounds, const glm::dvec3 &direction) noexcept
    {
        return {direction.x >= 0 ? bounds.maximum[0] : bounds.minimum[0],
                direction.y >= 0 ? bounds.maximum[1] : bounds.minimum[1],
                direction.z >= 0 ? bounds.maximum[2] : bounds.minimum[2]};
    }

    /// @brief 计算相对空间各轴的投影贡献. 坐标差溢出时先乘系数再相减.
    inline glm::dvec3 PlaneTerms(const glm::dvec3 &point, const glm::dvec3 &origin,
                                const glm::dvec3 &normal) noexcept
    {
        glm::dvec3 terms;
        for (glm::length_t axis = 0; axis < 3; ++axis)
        {
            const auto relative = point[axis] - origin[axis];
            terms[axis] = std::isfinite(relative) ? relative * normal[axis] :
                point[axis] * normal[axis] - origin[axis] * normal[axis];
        }
        return terms;
    }

    /// @brief 证明支持点或影响球完全处于单个平面外. radius 为米制半径, 零值用于点或 AABB 支持点.
    /// 相切和数值不确定时返回 false. 多个平面均返回 false 只表示这组平面未能证明分离.
    inline bool Outside(const CullingPlane &plane, const glm::dvec3 &point, const glm::dvec3 &origin,
                        double radius = 0) noexcept
    {
        const auto terms = PlaneTerms(point, origin, plane.normal);
        const double distance = (terms.x + terms.y + terms.z) + plane.offset;
        constexpr double Rounding = 32 * std::numeric_limits<double>::epsilon();
        const double allowance = Rounding * std::abs(terms.x) + Rounding * std::abs(terms.y) +
            Rounding * std::abs(terms.z) + Rounding * std::abs(plane.offset) + Rounding * radius;
        return std::isfinite(distance) && distance < -radius - allowance;
    }
}
