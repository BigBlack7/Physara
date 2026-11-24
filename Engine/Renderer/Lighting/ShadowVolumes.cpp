#include "ShadowVolumes.hpp"

/// @file
/// @brief 拟合稳定接收段及灯空间有限范围, 在冻结场景全表中保守选择阴影投射物.
/// 局部灯先检查接收视锥与有限灯体能否分离. 获准后才按各自的投射物范围筛选 Primitive.

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <utility>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../Prepare/SceneFrame.hpp"
#include "../Prepare/ViewFrustum.hpp"

namespace Physara::Renderer::Detail
{
    constexpr double ProjectionRounding = 64 * std::numeric_limits<double>::epsilon();

    /// @brief 按 X/Y/Z 顺序保存右手基, Z 沿灯光从光源指向接收物.
    static std::array<glm::dvec3, 3> LightAxes(const glm::dvec3 &direction) noexcept
    {
        const glm::dvec3 reference = std::abs(direction.y) < 0.9 ? glm::dvec3(0, 1, 0) : glm::dvec3(1, 0, 0);
        const auto right = glm::normalize(glm::cross(reference, direction));
        return {right, glm::cross(direction, right), direction};
    }

    static std::array<double, 3> Value(const glm::dvec3 &vector) noexcept
    {
        return {vector.x, vector.y, vector.z};
    }

    /// @brief 相对相机原点的光空间区间. +Z 沿发射轴, 并为相切保留误差余量.
    struct ShadowLightBounds
    {
        std::array<double, 3> minimum;
        std::array<double, 3> maximum;
    };

    /// @brief 方向光的 receiver 以及与已选 caster 合并的范围. 合并范围参与深度拟合和舍入估计.
    struct DirectionalShadowVolume
    {
        glm::dvec3 origin;
        std::array<glm::dvec3, 3> axes;
        ShadowLightBounds receiver;
        ShadowLightBounds fitted;
    };

    /// @brief 将光轴转换到相机局部空间. 对近远截面分别求中心投影和半宽, 得到八角点的投影极值.
    /// 透视截面的宽高随视深放大, 正交截面保持固定. 区间向外留出浮点舍入余量以保留相切物体.
    static std::optional<DirectionalShadowVolume> DirectionalVolume(const ShadowReceiver &receiver,
        const std::array<glm::dvec3, 3> &axes, double nearPlane, double farPlane) noexcept
    {
        const auto world = glm::make_mat4(receiver.worldFromView.data());
        DirectionalShadowVolume volume{glm::dvec3(world[3]), axes, {}, {}};
        const auto viewFromWorld = glm::transpose(glm::dmat3(world));
        for (std::size_t axis = 0; axis < axes.size(); ++axis)
        {
            const auto viewAxis = viewFromWorld * axes[axis];
            double minimum = std::numeric_limits<double>::infinity();
            double maximum = -minimum;
            for (const double depth : {nearPlane, farPlane})
            {
                const double factor = receiver.orthographic ? 1 : depth;
                const double halfX = (factor / receiver.projectionScale[0]) * receiver.sideLimit[0];
                const double halfY = (factor / receiver.projectionScale[1]) * receiver.sideLimit[1];
                const double extent = std::abs(viewAxis.x) * halfX + std::abs(viewAxis.y) * halfY;
                const double center = -depth * viewAxis.z;
                const double allowance = ProjectionRounding * halfX + ProjectionRounding * halfY +
                    ProjectionRounding * depth;
                minimum = std::min(minimum, center - extent - allowance);
                maximum = std::max(maximum, center + extent + allowance);
                if (!std::isfinite(center) || !std::isfinite(extent))
                {
                    return std::nullopt;
                }
            }
            if (!std::isfinite(minimum) || !std::isfinite(maximum))
            {
                return std::nullopt;
            }
            volume.receiver.minimum[axis] = minimum;
            volume.receiver.maximum[axis] = maximum;
        }
        volume.fitted = volume.receiver;
        return volume;
    }

    /// @brief 原点到当前支点单纯形最近的特征. 掩码用于保留构成该特征的支点.
    struct ClosestFeature
    {
        glm::dvec3 point;
        unsigned vertices;
    };

    using SupportSimplex = std::array<glm::dvec3, 4>;

    /// @brief 将原点投到线段上, 端点或重合点只保留一个支点.
    static ClosestFeature ClosestSegment(const SupportSimplex &points, unsigned a, unsigned b) noexcept
    {
        const auto segment = points[b] - points[a];
        const auto along = glm::dot(-points[a], segment);
        if (along <= 0)
        {
            return {points[a], 1u << a};
        }
        const auto lengthSquared = glm::dot(segment, segment);
        if (along >= lengthSquared)
        {
            return {points[b], 1u << b};
        }
        return {points[a] + segment * (along / lengthSquared), (1u << a) | (1u << b)};
    }

    // 支点都在两凸体的差空间中, 比较的是各特征到原点的距离.
    static ClosestFeature Nearest(ClosestFeature left, ClosestFeature right) noexcept
    {
        return glm::dot(left.point, left.point) <= glm::dot(right.point, right.point) ? left : right;
    }

    /// @brief 用面内投影的重心坐标判断面是否最近. 投影超出面或面退化时考察边.
    static ClosestFeature ClosestTriangle(const SupportSimplex &points, unsigned a, unsigned b, unsigned c) noexcept
    {
        const auto ab = points[b] - points[a];
        const auto ac = points[c] - points[a];
        const auto normal = glm::cross(ab, ac);
        const double areaSquared = glm::dot(normal, normal);
        const auto u = glm::dot(glm::cross(-points[a], ac), normal);
        const auto v = glm::dot(glm::cross(ab, -points[a]), normal);
        if (u > 0 && v > 0 && u + v < areaSquared)
        {
            return {points[a] + ab * (u / areaSquared) + ac * (v / areaSquared), (1u << a) | (1u << b) | (1u << c)};
        }
        return Nearest(ClosestSegment(points, a, b),
                       Nearest(ClosestSegment(points, a, c), ClosestSegment(points, b, c)));
    }

    /// @brief 原点位于四面体内时已证相交. 否则最近点在边界的四个面之一.
    static ClosestFeature ClosestSimplex(const SupportSimplex &points, unsigned size) noexcept
    {
        switch (size)
        {
        case 1: return {points[0], 1};
        case 2: return ClosestSegment(points, 0, 1);
        case 3: return ClosestTriangle(points, 0, 1, 2);
        default: break;
        }
        assert(size == points.size());
        const auto ab = points[1] - points[0];
        const auto ac = points[2] - points[0];
        const auto ad = points[3] - points[0];
        const auto volume = glm::dot(ab, glm::cross(ac, ad));
        const auto sign = volume < 0 ? -1.0 : 1.0;
        const auto u = sign * glm::dot(-points[0], glm::cross(ac, ad));
        const auto v = sign * glm::dot(ab, glm::cross(-points[0], ad));
        const auto w = sign * glm::dot(ab, glm::cross(ac, -points[0]));
        if (volume != 0 && u >= 0 && v >= 0 && w >= 0 && u + v + w <= std::abs(volume))
        {
            return {glm::dvec3(0), 0};
        }
        return Nearest(Nearest(ClosestTriangle(points, 0, 1, 2), ClosestTriangle(points, 0, 1, 3)),
                       Nearest(ClosestTriangle(points, 0, 2, 3), ClosestTriangle(points, 1, 2, 3)));
    }

    /// @brief 相机轴系中相对灯心的接收范围, 长度以灯的影响半径归一.
    /// 切片的左右界为 minimum - slope * depth 和 maximum + slope * depth.
    /// depth 表示从灯心沿相机前向的位移. 构造支点时, 对应的相机轴系 Z 分量为 -depth.
    struct ReceiverWindow
    {
        glm::dvec2 minimum, maximum, slope;
        double nearDepth, farDepth;
    };

    /// @brief 只保留局部灯在相机深度上能触及的区间. 侧界以灯心深度为基准计算.
    /// @return 空值表示计算无法给出有限的误差或侧界, 调用方保留候选. nearDepth 大于 farDepth 表示确实分离.
    static std::optional<ReceiverWindow> WindowAtLight(const ShadowReceiver &receiver, const glm::dvec3 &center,
        double radius, double positionError) noexcept
    {
        const double depth = -center.z;
        const double margin = positionError / radius;
        if (!std::isfinite(margin))
        {
            return std::nullopt;
        }
        ReceiverWindow window{.nearDepth = std::max(-1.0, (receiver.nearPlane - depth) / radius - margin),
                              .farDepth = std::min(1.0, (receiver.farPlane - depth) / radius + margin)};
        for (glm::length_t axis = 0; axis < 2; ++axis)
        {
            const double scale = receiver.sideLimit[axis] / receiver.projectionScale[axis];
            const double slope = receiver.orthographic ? 0 : scale;
            const double extent = receiver.orthographic ? scale : 0;
            // 旋转和投影的误差只依赖到灯心的局部量. 向外扩张, 使接触边界的灯仍可参与准入.
            const double sideMargin = margin * (1 + slope) + (ProjectionRounding * extent) / radius;
            window.minimum[axis] = std::fma(-slope, depth, -extent - center[axis]) / radius - sideMargin;
            window.maximum[axis] = std::fma(slope, depth, extent - center[axis]) / radius + sideMargin;
            window.slope[axis] = slope;
            if (!std::isfinite(window.minimum[axis]) || !std::isfinite(window.maximum[axis]))
            {
                return std::nullopt;
            }
        }
        return window;
    }

    /// @brief 给定深度的最近距离由两个独立的侧向间距及轴向间距决定.
    /// 两个正部项的平方可微, 最小值在四种活跃项组合的驻点或区间端点.
    static bool SphereOutside(const ReceiverWindow &window) noexcept
    {
        const auto offset = glm::max(window.minimum, -window.maximum);
        for (const double x : {0.0, window.slope.x})
        {
            for (const double y : {0.0, window.slope.y})
            {
                // 假定两轴分别处于截面内或外, 求相应距离平方的驻点. 夹至近远端面后按真实侧界复算距离.
                const double scale = std::max({1.0, x, y});
                const glm::dvec2 coefficients(x / scale, y / scale);
                const double normal = 1 / scale;
                const double root = glm::dot(coefficients, offset / scale) /
                    (normal * normal + glm::dot(coefficients, coefficients));
                const double depth = std::clamp(root, window.nearDepth, window.farDepth);
                const double dx = std::max(0.0, std::fma(-window.slope.x, depth, offset.x));
                const double dy = std::max(0.0, std::fma(-window.slope.y, depth, offset.y));
                if (std::hypot(dx, dy, depth) <= 1 + ProjectionRounding)
                {
                    return false;
                }
            }
        }
        return true;
    }

    /// @brief 接收体与单位影响球包围盒的交集. 每个侧界跨入盒边后保持线性或饱和.
    /// 依次收集入口、出口和至多四个侧界转折处的角点, 供聚光支持查询使用.
    static std::span<const glm::dvec3> ReachableCorners(const ReceiverWindow &window,
                                                       std::array<glm::dvec3, 24> &corners) noexcept
    {
        constexpr double Reach = 1 + ProjectionRounding;
        const auto offset = glm::max(window.minimum, -window.maximum);
        double enter = window.nearDepth;
        for (glm::length_t axis = 0; axis < 2; ++axis)
        {
            if (window.slope[axis] > 0)
            {
                enter = std::max(enter, (offset[axis] - Reach) / window.slope[axis]);
            }
        }
        if (enter > window.farDepth)
        {
            return {};
        }
        std::array<double, 6> sections{enter, window.farDepth};
        std::size_t sectionCount = 2;
        for (glm::length_t axis = 0; axis < 2; ++axis)
        {
            const double slope = window.slope[axis];
            if (slope > 0)
            {
                for (const double boundary : {window.minimum[axis] + Reach, Reach - window.maximum[axis]})
                {
                    const double depth = boundary / slope;
                    if (depth > enter && depth < window.farDepth)
                    {
                        sections[sectionCount++] = depth;
                    }
                }
            }
        }
        std::size_t cornerCount = 0;
        // 相邻转折深度间的截面角点线性移动, 沿任意方向的最大投影必在端点取得. 支持查询可以直接遍历这些顶点.
        for (const double depth : std::span(sections).first(sectionCount))
        {
            const auto min = glm::clamp(window.minimum - window.slope * depth, -Reach, Reach);
            const auto max = glm::clamp(window.maximum + window.slope * depth, -Reach, Reach);
            for (const double x : {min.x, max.x})
            {
                for (const double y : {min.y, max.y})
                {
                    corners[cornerCount++] = {x, y, -depth};
                }
            }
        }
        return std::span(corners).first(cornerCount);
    }

    /// @brief 单位聚光体相对原点的支持点和支持距离.
    struct SpotSupport
    {
        glm::dvec3 point;
        double distance;
    };

    /// @brief 搜索方向落入外锥时取球面点, 其余方向取锥缘圆周或锥尖上投影最大的点.
    static SpotSupport SupportSpot(const glm::dvec3 &direction, const glm::dvec3 &axis,
                                   double cosine, double sine) noexcept
    {
        const auto alignment = glm::dot(direction, axis);
        if (alignment >= cosine)
        {
            return {direction, 1};
        }
        const auto lateral = direction - alignment * axis;
        const auto lateralLength = std::hypot(lateral.x, lateral.y, lateral.z);
        const auto distance = alignment * cosine + lateralLength * sine;
        if (distance <= 0)
        {
            return {glm::dvec3(0), 0};
        }
        const auto rim = cosine * axis + sine * (lateral / lateralLength);
        return {rim, distance};
    }

    /// @brief 用灯体可达范围内的接收凸体与聚光支持映射寻找分离平面.
    /// GJK 搜索方向来自两体差的最近单纯形特征. 仅支持距离证明有间隔时剔除, 保留相切及数值不确定项.
    static bool SpotOutside(std::span<const glm::dvec3> corners, const glm::dvec3 &axis, double outerCone) noexcept
    {
        const auto cosine = std::cos(outerCone);
        const auto sine = std::sin(outerCone);
        SupportSimplex simplex;
        unsigned size = 0;
        auto search = -(0.5 * (corners.front() + corners.back()));
        constexpr unsigned MaxSupportQueries = 48;
        for (unsigned iteration = 0; iteration < MaxSupportQueries; ++iteration)
        {
            const auto length = std::hypot(search.x, search.y, search.z);
            if (!(length > ProjectionRounding))
            {
                return false;
            }
            const auto direction = search / length;
            const auto support = std::ranges::max_element(corners, {}, [&](const auto &point)
            {
                return glm::dot(point, direction);
            });
            const auto local = SupportSpot(-direction, axis, cosine, sine);
            // 接收体沿 d 的最大投影与灯体沿 -d 的最大投影之和, 是两体差在 d 上的上界.
            // 上界留有余量仍为负时, 垂直于 d 的平面可以隔开两个凸体.
            if (glm::dot(*support, direction) + local.distance < -ProjectionRounding)
            {
                return true;
            }

            const auto point = *support - local.point;
            const auto active = std::span(simplex).first(size);
            if (std::ranges::find(active, point) != active.end())
            {
                return false;
            }
            simplex[size++] = point;
            const auto closest = ClosestSimplex(simplex, size);
            if (closest.vertices == 0)
            {
                return false;
            }
            // 四面体只可能在外表面上给出非零掩码, 下一步最多保留三个支点.
            unsigned retained = 0;
            for (unsigned i = 0; i < size; ++i)
            {
                if (closest.vertices & (1u << i))
                {
                    simplex[retained++] = simplex[i];
                }
            }
            size = retained;
            search = -closest.point;
        }
        // 在局部坐标下仍没有分离证据时保守保留.
        return false;
    }

    /// @brief 球体用最近距离判定, 有接收球覆盖的聚光再按真实外锥确认.
    static bool SeparatedFromReceiver(const ShadowReceiver &receiver, const LocalShadowVolume &volume) noexcept
    {
        const auto world = glm::make_mat4(receiver.worldFromView.data());
        const auto relative = glm::make_vec3(volume.position.data()) - glm::dvec3(world[3]);
        const auto viewFromWorld = glm::transpose(glm::dmat3(world));
        const double error = ProjectionRounding *
            std::max({std::abs(relative.x), std::abs(relative.y), std::abs(relative.z), volume.farPlane});
        const auto window = WindowAtLight(receiver, viewFromWorld * relative, volume.farPlane, error);
        if (!window)
        {
            return false;
        }
        if (window->nearDepth > window->farDepth || SphereOutside(*window))
        {
            return true;
        }
        if (volume.outerCone == 0)
        {
            return false;
        }
        std::array<glm::dvec3, 24> vertices;
        const auto corners = ReachableCorners(*window, vertices);
        const auto axis = glm::normalize(viewFromWorld * glm::make_vec3(volume.direction.data()));
        return !corners.empty() && SpotOutside(corners, axis, volume.outerCone);
    }

    std::variant<LocalShadowVolume, ShadowPlanReason> MakeLocalShadowVolume(const ShadowReceiver &receiver,
        std::span<const CullingPlane, 6> planes, const LightRecord &light, double shadowDistance) noexcept
    {
        const double farPlane = std::min(light.light.range.value_or(shadowDistance), shadowDistance);
        const double nearPlane = std::min(0.1, farPlane / 1024);
        constexpr double maxGpu = std::numeric_limits<float>::max();
        // 近远面和灯位由 float Shader 消费. 无法编码时只拒绝该灯的阴影组.
        if (!(nearPlane >= std::numeric_limits<float>::min() && nearPlane < farPlane &&
              farPlane <= maxGpu) ||
            !std::ranges::all_of(light.position, [](double value) { return value >= -maxGpu && value <= maxGpu; }))
        {
            return ShadowPlanReason::InvalidVolume;
        }
        const bool spot = light.light.type == Asset::ModelLightKind::Spot;
        const auto cone = light.light.outerConeAngle;
        if (spot && !(cone < std::numbers::pi / 2 && std::tan(cone) >= 1.0 / maxGpu))
        {
            return ShadowPlanReason::InvalidVolume;
        }
        const auto position = glm::make_vec3(light.position.data());
        const auto camera = glm::make_vec3(receiver.worldFromView.data() + 12);
        // 逐面球测试先排除明显分离. 凸体检查再处理投影角落和聚光实际朝向.
        if (std::ranges::any_of(planes, [&](const auto &plane) { return Outside(plane, position, camera, farPlane); }))
        {
            return ShadowPlanReason::NoReceiverCoverage;
        }
        LocalShadowVolume volume{.position = light.position, .nearPlane = nearPlane, .farPlane = farPlane};
        if (spot)
        {
            volume.direction = Value(glm::normalize(glm::make_vec3(light.direction.data())));
            volume.outerCone = cone;
        }
        if (SeparatedFromReceiver(receiver, volume))
        {
            return ShadowPlanReason::NoReceiverCoverage;
        }
        return volume;
    }

    /// @brief 方向光的 caster 投影区间. 投影原点与 receiver 相同, 上游沿负 Z 延伸.
    static std::optional<ShadowLightBounds> ProjectBounds(const Scene::Bounds &bounds,
                                                         const DirectionalShadowVolume &volume) noexcept
    {
        ShadowLightBounds projected;
        for (std::size_t axis = 0; axis < volume.axes.size(); ++axis)
        {
            const auto direction = volume.axes[axis];
            const auto lower = PlaneTerms(SupportPoint(bounds, -direction), volume.origin, direction);
            const auto upper = PlaneTerms(SupportPoint(bounds, direction), volume.origin, direction);
            const auto allowance = [](const glm::dvec3 &terms)
            {
                return ProjectionRounding * std::abs(terms.x) + ProjectionRounding * std::abs(terms.y) +
                    ProjectionRounding * std::abs(terms.z);
            };
            projected.minimum[axis] = (lower.x + lower.y + lower.z) - allowance(lower);
            projected.maximum[axis] = (upper.x + upper.y + upper.z) + allowance(upper);
            if (!std::isfinite(projected.minimum[axis]) || !std::isfinite(projected.maximum[axis]))
            {
                return std::nullopt;
            }
        }
        return projected;
    }

    /// @brief 保守局部体. 影响球限制距离, 聚光另有包住外锥的四侧平面. 不用近面剔除 caster.
    struct LocalCasterVolume
    {
        glm::dvec3 origin;
        double radius;
        std::array<CullingPlane, 4> sides{};
        bool spot;

        explicit LocalCasterVolume(const LocalShadowVolume &volume) noexcept
            : origin(glm::make_vec3(volume.position.data())), radius(volume.farPlane), spot(volume.outerCone > 0)
        {
            if (spot)
            {
                const auto axes = LightAxes(glm::make_vec3(volume.direction.data()));
                const auto axis = axes[2] * std::sin(volume.outerCone);
                const auto cosine = std::cos(volume.outerCone);
                const auto sideX = axes[0] * cosine;
                const auto sideY = axes[1] * cosine;
                sides = {CullingPlane{axis + sideX, 0}, CullingPlane{axis - sideX, 0},
                         CullingPlane{axis + sideY, 0}, CullingPlane{axis - sideY, 0}};
            }
        }

        /// @brief 只剔除与影响球或外锥包围体分离的 AABB. Point 六面共用此集合.
        [[nodiscard]] bool Intersects(const Scene::Bounds &bounds) const noexcept
        {
            const auto nearest = glm::clamp(origin, glm::make_vec3(bounds.minimum.data()),
                                           glm::make_vec3(bounds.maximum.data()));
            const auto delta = nearest - origin;
            const auto distance = std::hypot(delta.x, delta.y, delta.z);
            if (distance > radius + ProjectionRounding * radius)
            {
                return false;
            }
            return !spot || std::ranges::none_of(sides, [&](const auto &side)
            {
                return Outside(side, SupportPoint(bounds, side.normal), origin);
            });
        }
    };

    /// @brief 从冻结的场景全集取可参与阴影覆盖的有界 Primitive.
    static bool CanCast(const PrimitiveRef &primitive) noexcept
    {
        return primitive.visible && primitive.castsShadow && primitive.bounds.has_value();
    }

    /// @brief 按交付精度拟合右手透视相机. 横向投影或近端写入深度不可用时返回空.
    static std::optional<LocalShadowProjection> LocalProjection(const glm::dvec3 &forward, const glm::dvec3 &up,
        const LocalShadowVolume &volume, double halfTangent, std::uint32_t edge) noexcept
    {
        const auto right = glm::cross(forward, up);
        const double depthScale = volume.farPlane / (volume.farPlane - volume.nearPlane);
        const double depthOffset = volume.nearPlane * depthScale;
        const double biasRatio = 2 * ShadowDepthConvention::CasterBiasTexels * halfTangent / edge;
        // 轴深 t 沿前向增加到 t*(1+biasRatio) 时, 等效于缩小写入矩阵中齐次 Z 常量的绝对值.
        // 直接编码偏移后的常量, 避免在 FP32 中相减丢失宽锥的剩余量.
        LocalShadowProjection result{.casterClipZ = static_cast<float>(-depthOffset / (1 + biasRatio))};
        // 将方形视锥内的相对坐标除以轴深, 得到各世界分量的幅度上界. 此上界也包住聚光的圆锥.
        const auto extentPerDepth = glm::abs(forward) + halfTangent * (glm::abs(right) + glm::abs(up));
        constexpr double MaxGpu = std::numeric_limits<float>::max();
        constexpr double FloatEpsilon = std::numeric_limits<float>::epsilon();
        // 三项点积累积乘加舍入, 相对坐标也已编码为 FP32.
        constexpr double DotRoundoff = 4 * FloatEpsilon;
        glm::dvec3 clipError(0);
        double nearGap = -double(result.casterClipZ) / volume.nearPlane;
        for (glm::length_t i = 0; i < 3; ++i)
        {
            const glm::dvec3 coefficients{right[i] / halfTangent, up[i] / halfTangent, forward[i]};
            if (!(std::abs(coefficients.x) <= MaxGpu && std::abs(coefficients.y) <= MaxGpu))
            {
                return std::nullopt;
            }
            const glm::vec3 encoded(coefficients);
            const glm::dvec3 packed(encoded);
            // 按已编码的 X、Y、W 系数累计窄化误差, 并为相对坐标和点积的 FP32 舍入留余量.
            clipError += (glm::abs(packed - coefficients) + DotRoundoff * glm::abs(packed)) * extentPerDepth[i];
            const auto clipZ = static_cast<float>(forward[i] * depthScale);
            result.clipFromLight[i] = {encoded.x, encoded.y, clipZ, encoded.z};
            nearGap += (packed.z - clipZ) * forward[i];
        }
        // 除以理想轴深后 W 为 1, clipError.z 是 W 的误差上界. 有效画幅内 |X/W| 和 |Y/W| 至多为 1,
        // 因此两条 NDC 轴的分子误差都需加上 W 的误差, 再按分母下界 1 - clipError.z 衡量位移.
        const double ndcTexel = 2.0 / edge;
        const double xyError = glm::length(glm::dvec2(clipError) + clipError.z);
        // nearGap 是已编码写入投影在近轴端的 (w-z)/near. 保留可写入低于清除值的深度余量.
        if (!(xyError <= (ndcTexel - FloatEpsilon) * (1 - clipError.z) && nearGap > 8 * FloatEpsilon))
        {
            return std::nullopt;
        }
        result.clipFromLight[3] = {0, 0, -static_cast<float>(depthOffset), 0};
        return result;
    }

    std::optional<LocalShadowPlan> MakeLocalShadowPlan(const SceneFrame &scene, LocalShadowVolume volume,
                                                       std::uint32_t edge)
    {
        LocalShadowPlan result{.volume = volume};
        if (volume.outerCone > 0)
        {
            const auto axes = LightAxes(glm::make_vec3(volume.direction.data()));
            const auto projection = LocalProjection(axes[2], axes[1], volume, std::tan(volume.outerCone), edge);
            if (!projection)
            {
                return std::nullopt;
            }
            result.projections[0] = *projection;
        }
        else
        {
            // 与 Point 的 ShadowSubview 顺序一致. 上向决定每层图案的方向, 写入和查询共用此投影.
            static constexpr glm::dvec3 forward[] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
                {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
            static constexpr glm::dvec3 up[] = {{0, 1, 0}, {0, 1, 0}, {0, 0, -1},
                {0, 0, 1}, {0, 1, 0}, {0, 1, 0}};
            // 标称 45 度边缘投到 NDC 的 (edge - 2) / edge, 经 UV 映射后距纹理边缘恰好一个 texel.
            const double halfTangent = double(edge) / (edge - 2);
            for (std::size_t i = 0; i < result.projections.size(); ++i)
            {
                const auto projection = LocalProjection(forward[i], up[i], volume, halfTangent, edge);
                if (!projection)
                {
                    return std::nullopt;
                }
                result.projections[i] = *projection;
            }
        }
        const LocalCasterVolume local(volume);
        for (std::uint32_t index = 0; index < scene.primitives.size(); ++index)
        {
            const auto &primitive = scene.primitives[index];
            if (CanCast(primitive) && local.Intersects(*primitive.bounds))
            {
                result.casters.push_back(index);
            }
        }
        return result;
    }

    /// @brief 以接收切片和单点采样余量找上游 caster, 并将有关几何并入 Z.
    static bool CollectDirectionalCasters(const SceneFrame &scene, DirectionalShadowVolume &volume,
                                          double texel, std::vector<std::uint32_t> &casters)
    {
        // 遍历整个冻结表以保留画外遮挡. 空集合也能描述一个有效区域, 例如上一投射物刚被移除.
        for (std::uint32_t index = 0; index < scene.primitives.size(); ++index)
        {
            const auto &primitive = scene.primitives[index];
            if (!CanCast(primitive))
            {
                continue;
            }
            const auto candidate = ProjectBounds(*primitive.bounds, volume);
            if (!candidate)
            {
                return false;
            }
            const auto &receiver = volume.receiver;
            if (candidate->maximum[0] < receiver.minimum[0] - texel ||
                candidate->minimum[0] > receiver.maximum[0] + texel ||
                candidate->maximum[1] < receiver.minimum[1] - texel ||
                candidate->minimum[1] > receiver.maximum[1] + texel ||
                candidate->minimum[2] > receiver.maximum[2])
            {
                continue;
            }
            for (std::size_t axis = 0; axis < volume.axes.size(); ++axis)
            {
                volume.fitted.minimum[axis] = std::min(volume.fitted.minimum[axis], candidate->minimum[axis]);
                volume.fitted.maximum[axis] = std::max(volume.fitted.maximum[axis], candidate->maximum[axis]);
            }
            casters.push_back(index);
        }
        return true;
    }

    /// @brief 给出光空间盒相对指定原点时, 各世界分量绝对值的上界.
    /// 各光轴的贡献取绝对值后相加, 避免把旋转时的抵消计入精度余量.
    static glm::dvec3 WorldOffsetBound(const std::array<glm::dvec3, 3> &axes, const ShadowLightBounds &bounds,
                                       const glm::dvec3 &lightOrigin) noexcept
    {
        const auto lower = glm::abs(glm::make_vec3(bounds.minimum.data()) - lightOrigin);
        const auto upper = glm::abs(glm::make_vec3(bounds.maximum.data()) - lightOrigin);
        const auto extent = glm::max(lower, upper);
        return glm::abs(axes[0]) * extent.x + glm::abs(axes[1]) * extent.y + glm::abs(axes[2]) * extent.z;
    }

    /// @brief 按该轴参与点积的世界分量估计 FP32 误差, 包含相对位置、系数和乘加的舍入余量.
    static double ClipRoundoff(const glm::dvec3 &axis, const glm::dvec3 &worldOffsetBound) noexcept
    {
        constexpr double FloatRoundoff = 8 * std::numeric_limits<float>::epsilon();
        return FloatRoundoff * glm::dot(glm::abs(axis), worldOffsetBound);
    }

    /// @brief 用 GPU 高低位原点核对覆盖. XY 检查接收切片, Z 检查接收物和已选 caster, 远端预留写入偏移.
    static bool CoversFittedVolume(const DirectionalShadowVolume &volume, const glm::dvec3 &origin,
                                   double halfExtent, double depthRange, double biasMeters) noexcept
    {
        glm::dvec3 relativeOrigin;
        for (int axis = 0; axis < 3; ++axis)
        {
            if (!(std::abs(origin[axis]) <= std::numeric_limits<float>::max()))
            {
                return false;
            }
            // 与 GPU 常量相同的高低位. 先减去相机原点再加余量, 避免把小偏移加回绝对世界坐标.
            const double high = static_cast<float>(origin[axis]);
            const double low = static_cast<float>(origin[axis] - high);
            relativeOrigin[axis] = (high - volume.origin[axis]) + low;
        }
        const auto &axes = volume.axes;
        const glm::dvec3 center{glm::dot(axes[0], relativeOrigin), glm::dot(axes[1], relativeOrigin),
                               glm::dot(axes[2], relativeOrigin)};
        // XY 只需保证接收切片可查询. Z 另按完整拟合盒估算, 其中可以有远在切片上游的投射物.
        const auto receiverOffset = WorldOffsetBound(axes, volume.receiver, center);
        for (std::size_t axis = 0; axis < 2; ++axis)
        {
            const double xy = halfExtent - ClipRoundoff(axes[axis], receiverOffset);
            const double relativeCenter = center[static_cast<int>(axis)];
            if (!(volume.receiver.minimum[axis] - relativeCenter >= -xy &&
                  volume.receiver.maximum[axis] - relativeCenter <= xy))
            {
                return false;
            }
        }
        const double rounding = ClipRoundoff(axes[2], WorldOffsetBound(axes, volume.fitted, center));
        return volume.fitted.minimum[2] - center.z >= rounding &&
               volume.fitted.maximum[2] - center.z <= depthRange - biasMeters - rounding;
    }

    /// @brief 用接收切片的球固定 XY 尺度. 留一 texel 余量覆盖中心吸附和近邻采样.
    static std::optional<DirectionalShadowProjection> FitCascade(const SceneFrame &scene,
        const ShadowReceiver &receiver, const std::array<glm::dvec3, 3> &axes,
        double nearPlane, double farPlane, std::uint32_t edge, std::vector<std::uint32_t> &casters)
    {
        auto volume = DirectionalVolume(receiver, axes, nearPlane, farPlane);
        const double factor = receiver.orthographic ? 1 : farPlane;
        const double radius = std::hypot(factor / receiver.projectionScale[0] * receiver.sideLimit[0],
            factor / receiver.projectionScale[1] * receiver.sideLimit[1], (farPlane - nearPlane) * 0.5);
        const double halfExtent = radius / (1 - 2.0 / edge);
        const double texel = halfExtent * (2.0 / edge);
        if (!volume || !(texel > 0) || !std::isfinite(halfExtent) ||
            !CollectDirectionalCasters(scene, *volume, texel, casters))
        {
            return std::nullopt;
        }

        // 以固定世界网格为锚, 相机平移只影响中心落在哪个格点.
        const auto &x = axes[0];
        const auto &y = axes[1];
        const auto &z = axes[2];
        const auto offset = glm::make_vec3(receiver.worldFromView.data() + 8) * -std::midpoint(nearPlane, farPlane);
        const glm::dvec3 lightCamera{glm::dot(x, volume->origin), glm::dot(y, volume->origin),
                                    glm::dot(z, volume->origin)};
        const double cellX = (lightCamera.x + glm::dot(x, offset)) / texel;
        const double cellY = (lightCamera.y + glm::dot(y, offset)) / texel;
        // 吸附之前必须还能区分半格. 尺度不足时拒绝整组, 避免交付看似有效却无法随格点移动的投影.
        constexpr double CellLimit = double(std::uint64_t{1} << (std::numeric_limits<double>::digits - 1));
        if (!(std::abs(cellX) < CellLimit && std::abs(cellY) < CellLimit))
        {
            return std::nullopt;
        }
        const auto &fitted = volume->fitted;
        const double snappedX = std::round(cellX) * texel;
        const double snappedY = std::round(cellY) * texel;
        const double biasMeters = texel * ShadowDepthConvention::CasterBiasTexels;
        const glm::dvec3 unpaddedNear{snappedX - lightCamera.x, snappedY - lightCamera.y, fitted.minimum[2]};
        // 至少留一 texel. 长深度范围按实际 Z 轴加大余量, 再用交付原点验证偏移和舍入后的覆盖.
        const double depthRoundoff = ClipRoundoff(z, WorldOffsetBound(axes, fitted, unpaddedNear));
        const double padding = std::max(texel, biasMeters + 2 * depthRoundoff);
        const double nearZ = fitted.minimum[2] - padding;
        const double depthRange = fitted.maximum[2] + padding - nearZ;
        const auto origin = x * snappedX + y * snappedY + z * (lightCamera.z + nearZ);
        // 后续 UBO 保存区间的倒数, 在这里检查其 FP32 正规数范围.
        const auto usableScale = [](double meters)
        {
            const double scale = 1 / meters;
            return scale >= std::numeric_limits<float>::min() && scale <= std::numeric_limits<float>::max();
        };
        if (!usableScale(halfExtent) || !usableScale(depthRange) ||
            !CoversFittedVolume(*volume, origin, halfExtent, depthRange, biasMeters))
        {
            return std::nullopt;
        }
        // 光空间 +Z 为前方. 翻转 X 得到与主相机一致的屏幕绕序, 比较深度仍沿 +Z 增大.
        return DirectionalShadowProjection{Value(origin), {Value(-x), Value(y), Value(z)}, halfExtent, depthRange};
    }

    std::variant<DirectionalShadowPlan, ShadowPlanReason> MakeDirectionalShadowPlan(
        const SceneFrame &scene, const ShadowReceiver &receiver, const LightRecord &light,
        const ShadowSettings &settings, std::uint32_t edge)
    {
        const double nearPlane = receiver.nearPlane;
        const double farPlane = std::min(receiver.farPlane, settings.shadowDistanceMeters);
        if (!(farPlane > nearPlane))
        {
            return ShadowPlanReason::NoReceiverCoverage;
        }
        if (farPlane > std::numeric_limits<float>::max())
        {
            return ShadowPlanReason::InvalidVolume;
        }
        const auto count = settings.directionalCascades;
        std::array<float, MaxDirectionalCascades + 1> splits{};
        splits[0] = static_cast<float>(nearPlane);
        splits[count] = static_cast<float>(farPlane);
        for (std::uint32_t i = 1; i < count; ++i)
        {
            const double t = double(i) / count;
            const double linear = std::lerp(nearPlane, farPlane, t);
            const double split = receiver.orthographic ? linear :
                std::lerp(linear, std::exp(std::lerp(std::log(nearPlane), std::log(farPlane), t)), settings.splitLambda);
            splits[i] = static_cast<float>(split);
        }
        // 先舍入为查询端收到的端点, 再以相邻较短段确定过渡半宽. 末级只需保存远端.
        std::array<std::array<float, 2>, MaxDirectionalCascades> transition{};
        for (std::uint32_t i = 0; i < count; ++i)
        {
            if (!(splits[i] < splits[i + 1]))
            {
                return ShadowPlanReason::InvalidVolume;
            }
            const double center = splits[i + 1];
            const double halfWidth = i + 1 == count ? 0 : settings.transitionFraction *
                std::min(center - splits[i], splits[i + 2] - center);
            transition[i] = {static_cast<float>(center - halfWidth), static_cast<float>(center + halfWidth)};
        }

        DirectionalShadowPlan plan{.nearDepth = splits.front()};
        plan.cascades.reserve(count);
        const auto axes = LightAxes(glm::make_vec3(light.direction.data()));
        for (std::uint32_t i = 0; i < count; ++i)
        {
            // 投影覆盖 Shader 实际使用的过渡端点, 首末级还保留相机的有限端点精度.
            const double nearFit = i == 0 ? std::min(nearPlane, double(splits[0])) : transition[i - 1][0];
            const double farFit = i + 1 == count ? std::max(farPlane, double(splits[count])) : transition[i][1];
            DirectionalShadowCascade cascade{.farDepth = splits[i + 1], .transition = transition[i]};
            const auto projection = FitCascade(scene, receiver, axes, nearFit, farFit, edge, cascade.casters);
            if (!projection)
            {
                return ShadowPlanReason::InvalidVolume;
            }
            cascade.projection = *projection;
            plan.cascades.push_back(std::move(cascade));
        }
        return plan;
    }
}
