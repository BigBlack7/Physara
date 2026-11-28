#include "GltfTangents.hpp"
#include "StaticTangents.hpp"

/// @file
/// @brief 在进入固定 C 算法前约束 float32 数值范围和工作空间, 取得可校验的逐角点切线.

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <tuple>

#include <mikktspace.h>

namespace Physara::Asset::Gltf
{
    bool IsUnit(std::span<const float, 3> direction) noexcept
    {
        const auto length = std::hypot(direction[0], direction[1], direction[2]);
        return std::abs(length - 1) <= 0.001f;
    }

    bool IsTangentFrame(const ModelVertex &vertex) noexcept
    {
        const auto &n = vertex.normal;
        const auto &t = vertex.tangent;
        return (t[3] == 1 || t[3] == -1) && IsUnit(std::span(t).first<3>()) &&
               std::abs(n[0] * t[0] + n[1] * t[1] + n[2] * t[2]) <= 0.002f;
    }

    /// @brief 以同一正比例缩放一个向量, 缩放值由当前 Primitive 的最大幅度推导.
    template<std::size_t N>
    static std::array<float, N> Scaled(std::array<float, N> value, double scale) noexcept
    {
        for (auto &component : value)
        {
            component = static_cast<float>(component * scale);
        }
        return value;
    }

    /// @brief C 回调只借用已验证的数据. 等比缩放限制库内部 float32 中间值, 不改变切线方向.
    struct TangentInput
    {
        std::span<const ModelVertex> vertices;
        std::span<const std::uint32_t> indices;
        std::span<std::array<float, 4>> output;
        std::uint32_t uv;
        double positionScale;
        double uvScale;

        [[nodiscard]] const ModelVertex &At(std::size_t corner) const noexcept { return vertices[indices[corner]]; }
        [[nodiscard]] auto Position(std::size_t corner) const noexcept { return Scaled(At(corner).position, positionScale); }
        [[nodiscard]] auto Uv(std::size_t corner) const noexcept { return Scaled(At(corner).uv[uv], uvScale); }

        [[nodiscard]] auto WeldKey(std::uint32_t corner) const noexcept
        {
            return std::tuple(Position(corner), At(corner).normal, Uv(corner));
        }
    };

    /// @brief 恢复本次同步 C 回调借用的适配上下文, 不持有跨调用状态.
    static TangentInput &Input(const SMikkTSpaceContext *context) noexcept
    {
        return *static_cast<TangentInput *>(context->m_pUserData);
    }

    /// @brief 将 MikkTSpace 三角面号和角点号转换成输入 indices 的线性位置.
    static std::size_t Corner(int face, int vertex) noexcept
    {
        return static_cast<std::size_t>(face) * 3 + static_cast<std::size_t>(vertex);
    }

    /// @brief 以二的幂次将最大幅度缩到单位量级, 减少等比缩放附加的舍入.
    static double ScaleFor(float magnitude) noexcept
    {
        int exponent = 0;
        std::frexp(magnitude, &exponent);
        return std::ldexp(1.0, -exponent);
    }

    /// @brief 检查固定 C 实现能以 float32 归一化两个方向. 防止其退化回退被当成真实切线.
    static bool HasTangentBasis(const TangentInput &input, std::size_t corner) noexcept
    {
        const auto a = input.Uv(corner);
        const auto b = input.Uv(corner + 1);
        const auto c = input.Uv(corner + 2);
        const auto u1 = b[0] - a[0];
        const auto v1 = b[1] - a[1];
        const auto u2 = c[0] - a[0];
        const auto v2 = c[1] - a[1];
        if (std::abs(u1 * v2 - v1 * u2) <= (std::numeric_limits<float>::min)())
        {
            return false;
        }
        const auto p = input.Position(corner);
        const auto q = input.Position(corner + 1);
        const auto r = input.Position(corner + 2);
        if (p == q || p == r || q == r)
        {
            return false;
        }
        float tangentSquared = 0;
        float bitangentSquared = 0;
        for (std::size_t axis = 0; axis < p.size(); ++axis)
        {
            const auto e1 = q[axis] - p[axis];
            const auto e2 = r[axis] - p[axis];
            const auto tangent = e1 * v2 - e2 * v1;
            const auto bitangent = e2 * u1 - e1 * u2;
            tangentSquared += tangent * tangent;
            bitangentSquared += bitangent * bitangent;
        }
        return tangentSquared > 0 && bitangentSquared > 0;
    }

    /// @brief 固定实现会沿焊接扇区递归, 单组还会保留最多平方级的子组索引.
    static void CheckMikkScratch(const TangentInput &input, std::string_view location, std::size_t limit,
                                 std::stop_token stop)
    {
        const auto corners = input.indices.size();
        // 包含适配层角点数组和 C 库按三角形、角点以及固定散列表申请的线性空间.
        constexpr std::uint64_t BytesPerCorner = 160;
        constexpr std::uint64_t HashBytes = 3 * 2048 * sizeof(int);
        const auto linear = BytesPerCorner * corners + HashBytes;
        if (linear > limit || corners > (std::numeric_limits<int>::max)() / 4)
        {
            Reject(AssetError::ModelByteLimit, location, "Tangent generation exceeds the model geometry budget");
        }

        std::vector<std::uint32_t> order(corners);
        std::iota(order.begin(), order.end(), std::uint32_t{0});
        std::ranges::sort(order, [&input](auto a, auto b) { return input.WeldKey(a) < input.WeldKey(b); });
        CheckStop(stop);
        std::size_t maxFan = 1;
        std::size_t run = 1;
        for (std::size_t i = 1; i < order.size(); ++i)
        {
            run = input.WeldKey(order[i]) == input.WeldKey(order[i - 1]) ? run + 1 : 1;
            maxFan = (std::max)(maxFan, run);
        }
        // 1024 层给线程默认的 1 MiB 栈保留调用空间. 较大扇区不能直接进入该 C 实现.
        if (maxFan > 1024 || maxFan * maxFan * sizeof(int) + 64 * maxFan > limit - linear)
        {
            Reject(AssetError::ModelByteLimit, location, "Welded tangent fan exceeds the geometry work budget");
        }
    }

    /// @brief 确定与固定切线算法相同的坐标数值域.
    static TangentInput MakeInput(std::span<const ModelVertex> vertices, std::span<const std::uint32_t> indices,
                                  std::uint32_t uv, std::stop_token stop)
    {
        float positionMagnitude = 0;
        float uvMagnitude = 0;
        for (std::size_t i = 0; i < indices.size(); ++i)
        {
            if (i % 4096 == 0)
            {
                CheckStop(stop);
            }
            const auto &vertex = vertices[indices[i]];
            for (auto x : vertex.position)
            {
                positionMagnitude = (std::max)(positionMagnitude, std::abs(x));
            }
            for (auto x : vertex.uv[uv])
            {
                uvMagnitude = (std::max)(uvMagnitude, std::abs(x));
            }
        }
        return {vertices, indices, {}, uv, ScaleFor(positionMagnitude), ScaleFor(uvMagnitude)};
    }

    std::vector<std::array<float, 4>>
    GenerateTangents(std::span<const ModelVertex> vertices, std::span<const std::uint32_t> indices,
                     std::uint32_t uv, std::string_view location, std::size_t scratchLimit, std::stop_token stop)
    {
        auto input = MakeInput(vertices, indices, uv, stop);
        CheckMikkScratch(input, location, scratchLimit, stop);
        for (std::size_t i = 0; i < indices.size(); i += 3)
        {
            if (i % (4096 * 3) == 0)
            {
                CheckStop(stop);
            }
            if (!HasTangentBasis(input, i))
            {
                Reject(AssetError::UnsupportedModel, location, "Triangle cannot form a tangent frame in the effective UV set");
            }
        }

        std::vector<std::array<float, 4>> tangents(indices.size());
        input.output = tangents;
        // m_get*/m_setTSpaceBasic 的拼写来自上游 C 协议. 回调仅访问调用期间存活的已验证 span.
        // 接收角点结果而不写回原共享顶点, 分裂和索引重建由 Geometry 统一完成.
        SMikkTSpaceInterface callbacks{};
        callbacks.m_getNumFaces = [](const SMikkTSpaceContext *ctx) noexcept
        {
            return static_cast<int>(Input(ctx).indices.size() / 3);
        };
        callbacks.m_getNumVerticesOfFace = [](const SMikkTSpaceContext *, int) noexcept { return 3; };
        callbacks.m_getPosition = [](const SMikkTSpaceContext *ctx, float *out, int face, int vertex) noexcept
        {
            std::ranges::copy(Input(ctx).Position(Corner(face, vertex)), out);
        };
        callbacks.m_getNormal = [](const SMikkTSpaceContext *ctx, float *out, int face, int vertex) noexcept
        {
            std::ranges::copy(Input(ctx).At(Corner(face, vertex)).normal, out);
        };
        callbacks.m_getTexCoord = [](const SMikkTSpaceContext *ctx, float *out, int face, int vertex) noexcept
        {
            std::ranges::copy(Input(ctx).Uv(Corner(face, vertex)), out);
        };
        callbacks.m_setTSpaceBasic = [](const SMikkTSpaceContext *ctx, const float *tangent, float sign,
                                        int face, int vertex) noexcept
        {
            Input(ctx).output[Corner(face, vertex)] = {tangent[0], tangent[1], tangent[2], sign};
        };
        const SMikkTSpaceContext context{&callbacks, &input};
        if (!genTangSpaceDefault(&context))
        {
            Reject(AssetError::UnsupportedModel, location, "MikkTSpace could not generate tangents");
        }
        CheckStop(stop);
        for (std::size_t i = 0; i < tangents.size(); ++i)
        {
            if (i % 4096 == 0)
            {
                CheckStop(stop);
            }
            auto vertex = input.At(i);
            vertex.tangent = tangents[i];
            if (!IsTangentFrame(vertex))
            {
                Reject(AssetError::UnsupportedModel, location, "MikkTSpace did not produce a usable tangent frame");
            }
        }
        return tangents;
    }
}

namespace Physara::Asset
{
    std::optional<AssetFailure> ValidateNormalUvFrame(const ModelPrimitive &primitive, std::uint32_t uv,
                                                      std::string_view location)
    {
        try
        {
            if (primitive.indices.empty())
            {
                Gltf::Reject(AssetError::UnsupportedModel, location, "Cannot generate tangents without triangles");
            }
            // 使用实际角点结果校验投影后的单位框架, 保留生成器的失败原因.
            (void)Gltf::GenerateTangents(primitive.vertices, primitive.indices, uv, location, StaticTangentWorkLimit, {});
            return std::nullopt;
        }
        catch (const Gltf::ImportFailure &failure)
        {
            return failure.error;
        }
    }
}
