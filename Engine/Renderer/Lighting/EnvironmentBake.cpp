#include "EnvironmentBake.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

/// @file
/// @brief 以同一环境空间约定生成 cube、GGX mips 和 SH9. 积分留在 CPU 冷路径, 按行合作取消.

namespace Physara::Renderer::Detail
{
    using Color3 = glm::dvec3;
    constexpr double Pi = std::numbers::pi_v<double>;
    constexpr std::uint32_t InputMipCount = 9;

    std::span<const EnvironmentTexel> CubeLevel::Face(std::uint32_t face) const noexcept
    {
        const auto count = std::size_t(edge) * edge;
        return std::span(pixels).subspan(count * face, count);
    }

    /// @brief 以 OpenGL 4.6 表 8.19 的主轴与 s/t 正方向表达贴面及逻辑行序.
    struct FaceAxes
    {
        glm::dvec3 major;
        glm::dvec3 s;
        glm::dvec3 t;
    };

    static constexpr std::array<FaceAxes, 6> Faces{{
        {{1, 0, 0}, {0, 0, -1}, {0, -1, 0}},
        {{-1, 0, 0}, {0, 0, 1}, {0, -1, 0}},
        {{0, 1, 0}, {1, 0, 0}, {0, 0, 1}},
        {{0, -1, 0}, {1, 0, 0}, {0, 0, -1}},
        {{0, 0, 1}, {1, 0, 0}, {0, -1, 0}},
        {{0, 0, -1}, {-1, 0, 0}, {0, -1, 0}}
    }};

    /// @brief 从连续 texel 中心生成方向. 边缘外坐标可用于定位相邻面.
    static glm::dvec3 Direction(std::uint32_t face, double x, double y, std::uint32_t edge) noexcept
    {
        const auto &axes = Faces[face];
        const double s = (2.0 * x + 1.0) / edge - 1.0;
        const double t = (2.0 * y + 1.0) / edge - 1.0;
        return axes.major + s * axes.s + t * axes.t;
    }

    /// @brief 主轴确定唯一立方体面, x/y 是该面的连续 texel 中心坐标.
    struct FacePosition
    {
        std::uint32_t face;
        double x;
        double y;
    };

    /// @brief 将方向投到主面. 结果中的整数位置对应 texel 中心.
    static FacePosition Locate(const glm::dvec3 &d, std::uint32_t edge) noexcept
    {
        const auto a = glm::abs(d);
        const auto axis = a.x >= a.y && a.x >= a.z ? 0 : (a.y >= a.z ? 1 : 2);
        const auto face = std::uint32_t(2 * axis + (d[axis] < 0.0));
        const auto &axes = Faces[face];
        const auto factor = 0.5 * edge / a[axis];
        const double center = 0.5 * (edge - 1.0);
        return {face, glm::dot(d, axes.s) * factor + center, glm::dot(d, axes.t) * factor + center};
    }

    static Color3 Rgb(const EnvironmentTexel &sample) noexcept
    {
        return {sample[0], sample[1], sample[2]};
    }

    static EnvironmentTexel ToTexel(const Color3 &rgb) noexcept
    {
        return {float(rgb.x), float(rgb.y), float(rgb.z), 1.0f};
    }

    /// @brief 一次分配该 mip 的六面, Face 按固定面序切出连续像素.
    static std::shared_ptr<CubeLevel> NewLevel(std::uint32_t edge)
    {
        return std::make_shared<CubeLevel>(edge, std::vector<EnvironmentTexel>(std::size_t(edge) * edge * 6));
    }

    /// @brief 取双线性核的一个样本. 核越界时由相邻方向决定取样面, 避免每个面的独立夹边.
    static Color3 Fetch(const CubeLevel &level, std::uint32_t face, int x, int y) noexcept
    {
        const auto edge = int(level.edge);
        if (x < 0 || x >= edge || y < 0 || y >= edge)
        {
            const auto adjacent = Locate(Direction(face, x, y, level.edge), level.edge);
            face = adjacent.face;
            x = std::clamp(int(adjacent.x + 0.5), 0, edge - 1);
            y = std::clamp(int(adjacent.y + 0.5), 0, edge - 1);
        }
        return Rgb(level.Face(face)[std::size_t(y) * level.edge + std::size_t(x)]);
    }

    /// @brief 在一个输入 mip 双线性取样, 方向不要求单位长度.
    static Color3 SampleCube(const CubeLevel &level, const glm::dvec3 &direction) noexcept
    {
        const auto p = Locate(direction, level.edge);
        const auto left = int(std::floor(p.x));
        const auto top = int(std::floor(p.y));
        return glm::mix(glm::mix(Fetch(level, p.face, left, top), Fetch(level, p.face, left + 1, top), p.x - left),
                        glm::mix(Fetch(level, p.face, left, top + 1), Fetch(level, p.face, left + 1, top + 1),
                                 p.x - left),
                        p.y - top);
    }

    /// @brief 将单位方向映射到顶部起行的等距柱状采样. 经度跨缝, 纬度夹在末行.
    static Color3 SampleSource(const Asset::EnvironmentImage &image, const glm::dvec3 &d) noexcept
    {
        const auto pixels = image.Pixels();
        const auto width = int(image.width);
        const auto height = int(image.height);
        const double x = (0.5 + std::atan2(d.x, d.z) / (2.0 * Pi)) * width - 0.5;
        const double y = (0.5 - std::asin(std::clamp(d.y, -1.0, 1.0)) / Pi) * height - 0.5;
        const auto left = int(std::floor(x));
        const auto top = int(std::floor(y));
        const auto load = [&](int column, int row) {
            const auto wrapped = column < 0 ? width - 1 : (column == width ? 0 : column);
            const auto offset = 3 * (std::size_t(std::clamp(row, 0, height - 1)) * image.width +
                std::size_t(wrapped));
            return Color3(pixels[offset], pixels[offset + 1], pixels[offset + 2]);
        };
        return glm::mix(glm::mix(load(left, top), load(left + 1, top), x - left),
                        glm::mix(load(left, top + 1), load(left + 1, top + 1), x - left), y - top);
    }

    /// @brief 作业按面和行写完整数据, 取消时不给调用方部分子资源.
    template <typename Sample>
    static std::shared_ptr<const CubeLevel> RenderCube(std::uint32_t edge, std::stop_token stop, Sample sample)
    {
        auto level = NewLevel(edge);
        for (std::uint32_t face = 0; face != Faces.size(); ++face)
        {
            for (std::uint32_t y = 0; y != edge; ++y)
            {
                if (stop.stop_requested())
                {
                    return {};
                }
                for (std::uint32_t x = 0; x != edge; ++x)
                {
                    level->pixels[(std::size_t(face) * edge + y) * edge + x] = ToTexel(sample(face, x, y));
                }
            }
        }
        return level;
    }

    /// @brief Hammersley 的根倒序, 和均匀方位角组成各向同性固定样本序列.
    static double RadicalInverse(std::uint32_t bits) noexcept
    {
        bits = (bits << 16) | (bits >> 16);
        bits = ((bits & 0x55555555u) << 1) | ((bits >> 1) & 0x55555555u);
        bits = ((bits & 0x33333333u) << 2) | ((bits >> 2) & 0x33333333u);
        bits = ((bits & 0x0f0f0f0fu) << 4) | ((bits >> 4) & 0x0f0f0f0fu);
        bits = ((bits & 0x00ff00ffu) << 8) | ((bits >> 8) & 0x00ff00ffu);
        return double(bits) / 4294967296.0;
    }

    /// @brief 从 GGX NDF 重要性采样半角, 输入 alpha 是 p².
    static glm::dvec3 GgxHalf(std::uint32_t index, std::uint32_t count, double alpha) noexcept
    {
        const double u = RadicalInverse(index);
        const double a2 = alpha * alpha;
        const double z = std::sqrt((1.0 - u) / (1.0 + (a2 - 1.0) * u));
        const double r = std::sqrt(1.0 - z * z);
        const double phi = 2.0 * Pi * index / count;
        return {r * std::cos(phi), r * std::sin(phi), z};
    }

    /// @brief 同一目标 mip 的所有 texel 复用切线空间方向与归一权重.
    /// level 和 fraction 指向抗走样源链上的三线性采样位置.
    struct FilterSample
    {
        glm::dvec3 direction;
        double weight;
        std::uint32_t level;
        double fraction;
    };

    /// @brief 一层的切线空间 GGX 积分权重与源 footprint. 目标粗糙度由 Filament LOD 的反函数给出.
    static std::vector<FilterSample> FilterSamples(std::uint32_t mip)
    {
        const double p = 1.0 - std::sqrt(1.0 - double(mip) / (EnvironmentMipCount - 1));
        const double alpha = p * p;
        const double a2 = alpha * alpha;
        const auto count = 1024u << (mip - 1);
        constexpr double texelAngle = (4.0 * Pi) / (6.0 * EnvironmentEdge * EnvironmentEdge);
        std::vector<FilterSample> samples;
        samples.reserve(count);
        double weight = 0;
        for (std::uint32_t index = 0; index != count; ++index)
        {
            const auto h = GgxHalf(index, count, alpha);
            const double noL = 2.0 * h.z * h.z - 1.0;
            if (noL > 0.0)
            {
                const double d = 1.0 + (a2 - 1.0) * h.z * h.z;
                const double pdf = a2 / (4.0 * Pi * d * d);
                const double lod = std::clamp(0.5 * std::log2(4.0 / (double(count) * pdf * texelAngle)),
                                              0.0, double(InputMipCount - 1));
                const auto level = static_cast<std::uint32_t>(lod);
                samples.push_back({{2.0 * h.z * h.x, 2.0 * h.z * h.y, noL}, noL, level, lod - level});
                weight += noL;
            }
        }
        for (auto &sample : samples)
        {
            sample.weight /= weight;
        }
        return samples;
    }

    /// @brief 角点原函数用于求立方体 texel 精确立体角.
    static double SolidAngle(double x, double y) noexcept
    {
        return std::atan2(x * y, std::sqrt(x * x + y * y + 1.0));
    }

    /// @brief 保留三带实 SH. 系数预乘第二份基函数归一化, 着色只重建多项式.
    static std::array<EnvironmentTexel, 9> BuildSh(const CubeLevel &base, std::stop_token stop)
    {
        constexpr std::array normalizedSquared{0.25 / Pi, 0.75 / Pi, 0.75 / Pi, 0.75 / Pi, 3.75 / Pi,
                                               3.75 / Pi, 0.3125 / Pi, 3.75 / Pi, 0.9375 / Pi};
        constexpr std::array convolution{1.0, 2.0 / 3, 2.0 / 3, 2.0 / 3, 0.25, 0.25, 0.25, 0.25, 0.25};
        std::array<Color3, 9> values{};
        values.fill(Color3(0));
        const double step = 2.0 / base.edge;
        for (std::uint32_t face = 0; face != Faces.size(); ++face)
        {
            for (std::uint32_t y = 0; y != base.edge; ++y)
            {
                if (stop.stop_requested())
                {
                    return {};
                }
                const double y0 = y * step - 1.0;
                for (std::uint32_t x = 0; x != base.edge; ++x)
                {
                    const double x0 = x * step - 1.0;
                    const double angle = SolidAngle(x0 + step, y0 + step) - SolidAngle(x0, y0 + step) -
                                         SolidAngle(x0 + step, y0) + SolidAngle(x0, y0);
                    const auto n = glm::normalize(Direction(face, x, y, base.edge));
                    const auto value = angle * Rgb(base.Face(face)[std::size_t(y) * base.edge + x]);
                    const std::array basis{1.0, n.y, n.z, n.x, n.y * n.x, n.y * n.z,
                                           3.0 * n.z * n.z - 1.0, n.z * n.x, n.x * n.x - n.y * n.y};
                    for (std::size_t term = 0; term != basis.size(); ++term)
                    {
                        values[term] += value * basis[term];
                    }
                }
            }
        }
        std::array<EnvironmentTexel, 9> result{};
        for (std::size_t term = 0; term != values.size(); ++term)
        {
            result[term] = ToTexel(values[term] * normalizedSquared[term] * convolution[term]);
            if (!std::ranges::all_of(result[term], [](float value) { return std::isfinite(value); }))
            {
                throw Core::BoundedText("Environment SH exceeds finite FP32");
            }
            result[term][3] = 0;
        }
        return result;
    }

    std::shared_ptr<const BakedEnvironment> BakeEnvironment(const Asset::EnvironmentImage &source, bool lighting,
        std::shared_ptr<const CubeLevel> base, std::stop_token stop)
    {
        auto products = std::make_shared<BakedEnvironment>();
        products->base = std::move(base);
        if (!products->base)
        {
            products->base = RenderCube(EnvironmentEdge, stop, [&](auto face, auto x, auto y) {
                return SampleSource(source, glm::normalize(Direction(face, x, y, EnvironmentEdge)));
            });
        }
        if (!products->base || stop.stop_requested())
        {
            return {};
        }
        if (!lighting)
        {
            return products;
        }
        products->sh = BuildSh(*products->base, stop);
        if (stop.stop_requested())
        {
            return {};
        }

        // 普通缩小链仅作为卷积的采样输入. 输出层的内容来自独立的 GGX 半球积分.
        std::array<std::shared_ptr<const CubeLevel>, InputMipCount> sourceLevels{products->base};
        for (std::uint32_t mip = 1; mip != sourceLevels.size(); ++mip)
        {
            const auto &previous = *sourceLevels[mip - 1];
            sourceLevels[mip] = RenderCube(previous.edge / 2, stop, [&](auto face, auto x, auto y) {
                const auto pixels = previous.Face(face);
                const auto top = std::size_t(2 * y) * previous.edge + 2 * x;
                return 0.25 * (Rgb(pixels[top]) + Rgb(pixels[top + 1]) +
                               Rgb(pixels[top + previous.edge]) + Rgb(pixels[top + previous.edge + 1]));
            });
            if (!sourceLevels[mip])
            {
                return {};
            }
        }
        // 每个目标 texel 将同一组入射方向转到自己的切线框架, 然后按样本覆盖的立体角查源链.
        for (std::uint32_t mip = 1; mip != EnvironmentMipCount; ++mip)
        {
            const auto samples = FilterSamples(mip);
            const auto edge = EnvironmentEdge >> mip;
            products->prefiltered[mip - 1] = RenderCube(edge, stop, [&](auto face, auto x, auto y) {
                const auto n = glm::normalize(Direction(face, x, y, edge));
                const auto axis = std::abs(n.z) < 0.999 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0);
                const auto tangent = glm::normalize(glm::cross(axis, n));
                const auto bitangent = glm::cross(n, tangent);
                Color3 color(0);
                for (const auto &sample : samples)
                {
                    const auto l = sample.direction.x * tangent + sample.direction.y * bitangent +
                                   sample.direction.z * n;
                    auto radiance = SampleCube(*sourceLevels[sample.level], l);
                    if (sample.fraction > 0.0)
                    {
                        radiance = glm::mix(radiance, SampleCube(*sourceLevels[sample.level + 1], l),
                                           sample.fraction);
                    }
                    color += sample.weight * radiance;
                }
                return color;
            });
            if (!products->prefiltered[mip - 1])
            {
                return {};
            }
        }
        products->lighting = true;
        return products;
    }

    /// @brief Filament 多次散射表的 R/G, 分别积累带 Schlick 权重和令 Fresnel 为 1 的方向反照率.
    static glm::dvec2 IntegrateGgx(const glm::dvec3 &view, double alpha, std::span<const glm::dvec3> halves)
    {
        glm::dvec2 result(0);
        const double a2 = alpha * alpha;
        const double viewRoot = std::sqrt(a2 + (1.0 - a2) * view.z * view.z);
        for (const auto &h : halves)
        {
            const double voH = std::clamp(glm::dot(view, h), 0.0, 1.0);
            const double noL = 2.0 * voH * h.z - view.z;
            if (noL > 0.0)
            {
                const double root = std::sqrt(a2 + (1.0 - a2) * noL * noL);
                const double weight = 0.5 * noL * (voH / h.z) / (noL * viewRoot + view.z * root);
                const double f = 1.0 - voH;
                result += glm::dvec2(weight * f * f * f * f * f, weight);
            }
        }
        return result * (4.0 / double(halves.size()));
    }

    /// @brief 固定粗糙度下的半角和 Charlie 分布值, 整行 NoV 样点共用.
    struct ClothSample
    {
        glm::dvec3 half;
        double distribution;
    };

    /// @brief 固定 Filament cloth 通道使用均匀半角、Charlie 分布及 Ashikhmin 可见度.
    static double IntegrateCloth(const glm::dvec3 &view, std::span<const ClothSample> samples)
    {
        double result = 0;
        for (const auto &sample : samples)
        {
            const double voH = std::clamp(glm::dot(view, sample.half), 0.0, 1.0);
            const double noL = 2.0 * voH * sample.half.z - view.z;
            if (noL > 0.0)
            {
                result += sample.distribution * noL * voH / (4.0 * (noL + view.z - noL * view.z));
            }
        }
        return result * (8.0 * Pi / double(samples.size()));
    }

    std::shared_ptr<const DfgTable> BakeDfg(std::stop_token stop)
    {
        auto table = std::make_shared<DfgTable>();
        table->pixels.resize(DfgEdge * DfgEdge);
        std::array<glm::dvec3, 1024> ggx;
        std::vector<ClothSample> cloth(4096);
        for (std::uint32_t y = 0; y != DfgEdge; ++y)
        {
            // y 对应感知粗糙度 p, x 对应 NoV. 固定一行后两个积分可以分别复用其半角序列.
            const double p = (y + 0.5) / DfgEdge;
            const double alpha = p * p;
            for (std::uint32_t i = 0; i != ggx.size(); ++i)
            {
                ggx[i] = GgxHalf(i, std::uint32_t(ggx.size()), alpha);
            }
            for (std::uint32_t i = 0; i != cloth.size(); ++i)
            {
                const double z = 1.0 - RadicalInverse(i);
                const double r2 = 1.0 - z * z;
                const double r = std::sqrt(r2);
                const double phi = 2.0 * Pi * i / double(cloth.size());
                cloth[i] = {{r * std::cos(phi), r * std::sin(phi), z},
                            (2.0 + 1.0 / alpha) * std::pow(r2, 0.5 / alpha) / (2.0 * Pi)};
            }
            for (std::uint32_t x = 0; x != DfgEdge; ++x)
            {
                if (stop.stop_requested())
                {
                    return {};
                }
                const double noV = (x + 0.5) / DfgEdge;
                const glm::dvec3 view(std::sqrt(1.0 - noV * noV), 0.0, noV);
                const auto rg = IntegrateGgx(view, alpha, ggx);
                const double b = IntegrateCloth(view, cloth);
                // 积分误差只在物理方向反照率的区间内量化, 以半精度发布后再次确认能量分母.
                const auto g = glm::packHalf1x16(float(std::min(1.0, rg.y)));
                if (!std::isfinite(rg.x) || !std::isfinite(rg.y) || !std::isfinite(b) ||
                    !(glm::unpackHalf1x16(g) > 0.0f))
                {
                    throw Core::BoundedText("Invalid environment DFG coefficients");
                }
                table->pixels[y * DfgEdge + x] = {glm::packHalf1x16(float(std::clamp(rg.x, 0.0, std::min(1.0, rg.y)))),
                                                g, glm::packHalf1x16(float(b)), glm::packHalf1x16(1.0f)};
            }
        }
        return table;
    }
}
