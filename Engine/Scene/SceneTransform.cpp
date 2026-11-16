#include "SceneState.hpp"

/// @file
/// @brief 场景局部 TRS、世界矩阵和包围盒. 仅在外部值或父子组合改变时求值.

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <type_traits>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Physara::Scene
{
    static constexpr double TransformTolerance = 1.e-5;

    /// @brief 拒绝不适合保存或矩阵表示的数值.
    static bool Finite(auto &&values) noexcept
    {
        return std::ranges::all_of(values, [](auto x) { return std::isfinite(x); });
    }

    bool IsValidPose(const Pose &pose) noexcept
    {
        const auto &q = pose.rotation;
        const double length = std::hypot(std::hypot(q[0], q[1]), std::hypot(q[2], q[3]));
        return Finite(pose.translation) && std::isfinite(length) && std::abs(length - 1) <= TransformTolerance;
    }

    bool ValidLocal(const LocalTransform &value) noexcept
    {
        return IsValidPose(value) && Finite(value.scale);
    }

    bool ValidCamera(const Camera &camera) noexcept
    {
        return std::visit([](const auto &v)
        {
            if constexpr (std::is_same_v<std::decay_t<decltype(v)>, Asset::ModelPerspective>)
            {
                return v.yfov > 0 && v.yfov < std::numbers::pi && std::isfinite(v.znear) && v.znear > 0 &&
                    (!v.zfar || (std::isfinite(*v.zfar) && *v.zfar > v.znear)) &&
                    (!v.aspectRatio || (std::isfinite(*v.aspectRatio) && *v.aspectRatio > 0));
            }
            else
            {
                return std::isfinite(v.xmag) && v.xmag > 0 && std::isfinite(v.ymag) && v.ymag > 0 &&
                    v.znear >= 0 && std::isfinite(v.zfar) && v.zfar > v.znear;
            }
        }, camera);
    }

    bool ValidLight(const Light &light) noexcept
    {
        using Kind = Asset::ModelLightKind;
        if (!std::ranges::all_of(light.color, [](float x) { return x >= 0 && x <= 1; }) ||
            !std::isfinite(light.intensity) || light.intensity < 0 ||
            (light.range && (!std::isfinite(*light.range) || *light.range <= 0)))
        {
            return false;
        }
        switch (light.type)
        {
        case Kind::Directional:
            return !light.range && light.innerConeAngle == 0 && light.outerConeAngle == 0;
        case Kind::Point:
            return light.innerConeAngle == 0 && light.outerConeAngle == 0;
        case Kind::Spot:
            return light.innerConeAngle >= 0 && light.outerConeAngle > light.innerConeAngle &&
                light.outerConeAngle <= std::numbers::pi / 2;
        }
        return false;
    }

    /// @brief 将已验证的 xyzw 四元数用于局部求值, 按列应用有符号缩放.
    static glm::dmat4 Compose(const LocalTransform &local) noexcept
    {
        const auto &q = local.rotation;
        auto out = glm::mat4_cast(glm::normalize(glm::dquat(q[3], q[0], q[1], q[2])));
        for (glm::length_t axis = 0; axis < 3; ++axis)
        {
            out[axis] *= local.scale[static_cast<std::size_t>(axis)];
        }
        out[3] = glm::dvec4(glm::make_vec3(local.translation.data()), 1.0);
        return out;
    }

    /// @brief 取与给定轴夹角最大的坐标轴, 使补齐零缩放列时投影有有效长度.
    static glm::dvec3 Perpendicular(const glm::dvec3 &direction) noexcept
    {
        const auto a = glm::abs(direction);
        const auto axis = a.x <= a.y && a.x <= a.z ? 0 : (a.y <= a.z ? 1 : 2);
        glm::dvec3 basis(0.0);
        basis[axis] = 1;
        return glm::normalize(basis - glm::dot(basis, direction) * direction);
    }

    /// @brief 从仿射列提取 TRS. 补齐零轴, 保留镜像, 用重组误差拒绝 shear.
    static std::optional<LocalTransform> Decompose(const glm::dmat4 &matrix) noexcept
    {
        if (!Finite(std::span(glm::value_ptr(matrix), 16)) || matrix[0][3] != 0 ||
            matrix[1][3] != 0 || matrix[2][3] != 0 || matrix[3][3] != 1)
        {
            return std::nullopt;
        }
        LocalTransform local;
        std::copy_n(glm::value_ptr(matrix[3]), 3, local.translation.begin());
        std::array<glm::dvec3, 3> columns{};
        std::array<glm::dvec3, 3> basis{};
        std::size_t nonzero = 0;
        std::size_t last = 0;
        std::size_t missing = 0;
        for (glm::length_t index = 0; index < 3; ++index)
        {
            const auto i = static_cast<std::size_t>(index);
            const glm::dvec3 column(matrix[index]);
            const auto length = std::hypot(column.x, column.y, column.z);
            if (!std::isfinite(length))
            {
                return std::nullopt;
            }
            local.scale[i] = length;
            if (length == 0.0)
            {
                missing = i;
                continue;
            }
            // 分量分别除以长度, 亚正规缩放也不必先求可能溢出的倒数.
            columns[i] = {column.x / length, column.y / length, column.z / length};
            basis[i] = columns[i];
            ++nonzero;
            last = i;
        }
        if (nonzero == 0)
        {
            return local;
        }
        if (nonzero == 1)
        {
            basis[(last + 1) % 3] = Perpendicular(basis[last]);
            basis[(last + 2) % 3] = glm::cross(basis[last], basis[(last + 1) % 3]);
        }
        else
        {
            const auto c = nonzero == 3 ? 2 : missing;
            const auto a = (c + 1) % 3;
            const auto b = (c + 2) % 3;
            const auto cross = glm::cross(basis[a], basis[b]);
            if (glm::length(cross) < 1 - TransformTolerance)
            {
                return std::nullopt;
            }
            basis[c] = glm::normalize(cross);
            basis[b] = glm::cross(basis[c], basis[a]);
            if (nonzero == 3 && glm::dot(basis[c], columns[c]) < 0)
            {
                local.scale[c] = -local.scale[c];
            }
        }
        const auto rotation = glm::normalize(glm::quat_cast(glm::dmat3(basis[0], basis[1], basis[2])));
        local.rotation = {rotation.x, rotation.y, rotation.z, rotation.w};
        const auto recomposed = glm::mat3_cast(rotation);
        for (glm::length_t index = 0; index < 3; ++index)
        {
            const auto i = static_cast<std::size_t>(index);
            const double sign = local.scale[i] < 0 ? -1.0 : 1.0;
            if (local.scale[i] != 0 && glm::length(sign * recomposed[index] - columns[i]) > TransformTolerance)
            {
                return std::nullopt;
            }
        }
        return local;
    }

    std::optional<LocalTransform> ImportLocal(const Asset::ModelTransform &value) noexcept
    {
        if (value.matrix)
        {
            return Decompose(glm::make_mat4(value.matrix->data()));
        }
        LocalTransform local{{value.translation, value.rotation}, value.scale};
        return ValidLocal(local) ? std::optional(local) : std::nullopt;
    }

    std::optional<LocalTransform> RelativeLocal(const WorldState &world, const WorldState *parent) noexcept
    {
        auto local = glm::make_mat4(world.worldFromLocal.data());
        if (parent)
        {
            if (parent->orientation == 0)
            {
                return std::nullopt;
            }
            // 先去除父矩阵各列长度, 避免直接对巨量或微小缩放取行列式.
            const auto parentWorld = glm::make_mat4(parent->worldFromLocal.data());
            glm::dmat3 basis(parentWorld);
            glm::dvec3 length;
            for (glm::length_t i = 0; i < 3; ++i)
            {
                const auto &column = basis[i];
                length[i] = std::hypot(column.x, column.y, column.z);
                if (length[i] == 0 || !std::isfinite(length[i]))
                {
                    return std::nullopt;
                }
                basis[i] = {column.x / length[i], column.y / length[i], column.z / length[i]};
            }
            if (glm::determinant(basis) == 0)
            {
                return std::nullopt;
            }
            const auto inverse = glm::inverse(basis);
            local[3] -= parentWorld[3];
            for (glm::length_t i = 0; i < 4; ++i)
            {
                const auto value = inverse * glm::dvec3(local[i]);
                local[i] = {value.x / length.x, value.y / length.y, value.z / length.z, i == 3 ? 1.0 : 0.0};
            }
        }
        return Decompose(local);
    }

    /// @brief 完整变换局部盒, 保留父子合成产生的 shear 与零缩放.
    static std::optional<Bounds> TransformBounds(const glm::dmat4 &matrix, const Asset::ModelBounds &local) noexcept
    {
        const auto limit = std::numeric_limits<double>::infinity();
        Bounds result{{limit, limit, limit}, {-limit, -limit, -limit}};
        for (unsigned corner = 0; corner < 8; ++corner)
        {
            glm::dvec4 point(0.0, 0.0, 0.0, 1.0);
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                point[static_cast<glm::length_t>(axis)] =
                    (corner & (1U << axis)) ? local.maximum[axis] : local.minimum[axis];
            }
            const auto world = matrix * point;
            if (!Finite(std::span(glm::value_ptr(world), 3)))
            {
                return std::nullopt;
            }
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                const auto value = world[static_cast<glm::length_t>(axis)];
                result.minimum[axis] = std::min(result.minimum[axis], value);
                result.maximum[axis] = std::max(result.maximum[axis], value);
            }
        }
        return result;
    }

    std::optional<WorldState> ComputeWorld(const LocalTransform &local, const WorldState *parent,
                                            const Asset::ModelMesh *mesh)
    {
        auto matrix = Compose(local);
        WorldState result;
        if (parent)
        {
            matrix = glm::make_mat4(parent->worldFromLocal.data()) * matrix;
            result.orientation = parent->orientation;
        }
        if (!Finite(std::span(glm::value_ptr(matrix), 16)))
        {
            return std::nullopt;
        }
        std::copy_n(glm::value_ptr(matrix), 16, result.worldFromLocal.begin());
        for (const double scale : local.scale)
        {
            result.orientation *= (scale > 0) - (scale < 0);
        }
        if (mesh)
        {
            result.primitives.reserve(mesh->primitives.size());
            for (const auto &primitive : mesh->primitives)
            {
                std::optional<Bounds> world;
                if (primitive.bounds)
                {
                    world = TransformBounds(matrix, *primitive.bounds);
                    if (!world)
                    {
                        return std::nullopt;
                    }
                    if (!result.bounds)
                    {
                        result.bounds = world;
                    }
                    for (std::size_t axis = 0; axis < 3; ++axis)
                    {
                        result.bounds->minimum[axis] = std::min(result.bounds->minimum[axis], world->minimum[axis]);
                        result.bounds->maximum[axis] = std::max(result.bounds->maximum[axis], world->maximum[axis]);
                    }
                }
                result.primitives.emplace_back(world);
            }
        }
        return result;
    }
}
