#include "Froxel.hpp"

/// @file
/// @brief 用稳定 View 投影构造 CPU Froxel, 按着色灯行顺序生成有界压紧列表.
/// 先构造查询参数及保守光体跨度, 再计数、核算区间和填充. 任一阶段拒绝时放弃本轮载荷.

#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <limits>
#include <new>
#include <span>

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../Prepare/ViewFrustum.hpp"
#include "../Prepare/ViewPreparation.hpp"
#include "LightVolume.hpp"
#include "PunctualLights.hpp"

namespace Physara::Renderer
{
    std::uint32_t FroxelRequest::IndexBudget() const noexcept
    {
        constexpr auto capacity = static_cast<std::uint32_t>(Shader::Generated::StaticForward::FroxelIndicesCapacity);
        return std::min(indexLimit.value_or(capacity), capacity);
    }

    /// @brief 将 FP32 坐标映射为可排序的整数, 保留次正规值并把正负零归到同一位置.
    /// 零外缘用此顺序比较坐标与抖动量. 随后的稳定区间比较同时排除非有限样点.
    static std::int32_t OrderedPixel(float value) noexcept
    {
        const auto bits = std::bit_cast<std::uint32_t>(value);
        const auto magnitude = static_cast<std::int32_t>(bits & 0x7fffffffu);
        return bits < 0x80000000u ? magnitude : -magnitude;
    }

    std::optional<std::uint32_t> QueryFroxelCell(const Shader::Generated::StaticForward::FroxelView &grid,
                                                 std::array<float, 2> renderPixel, float viewDepth) noexcept
    {
        // 先按 FP32 位型拒绝负的非零视深, 与会将次正规值视为零的 GPU 比较保持一致.
        if (std::bit_cast<std::uint32_t>(viewDepth) > 0x80000000u ||
            !(viewDepth >= grid.depthEdges.front().value && viewDepth <= grid.depthEdges[grid.sliceCount].value))
        {
            return std::nullopt;
        }
        std::array<std::uint32_t, 2> tile;
        for (std::size_t axis = 0; axis < tile.size(); ++axis)
        {
            const auto origin = grid.renderRect[axis];
            const float relative = renderPixel[axis] - static_cast<float>(origin);
            // 零外缘要求去抖动前的坐标至少等于抖动量. 原点为零时保留输入位型,
            // 避免设备在减零或比较时丢失次正规值. 有留边的负坐标仍按稳定边界判断.
            if (grid.stableMinimum[axis] == 0 &&
                OrderedPixel(origin == 0 ? renderPixel[axis] : relative) < OrderedPixel(grid.jitterPixels[axis]))
            {
                return std::nullopt;
            }
            const float pixel = relative - grid.jitterPixels[axis];
            if (!(pixel >= grid.stableMinimum[axis] && pixel <= grid.stableMaximum[axis]))
            {
                return std::nullopt;
            }
            const auto interior = static_cast<std::uint32_t>(
                std::clamp(pixel, 0.0f, static_cast<float>(grid.renderRect[axis + 2])));
            tile[axis] = std::min(interior / grid.tileExtent[axis], grid.tileCount[axis] - 1);
        }
        // 只搜索内部边界. 大于样点的第一个边界之前有几条内界, 就属于第几层.
        const auto firstInterior = grid.depthEdges.begin() + 1;
        const auto layer = std::upper_bound(firstInterior, grid.depthEdges.begin() + grid.sliceCount, viewDepth,
            [](float depth, const auto &edge) { return depth < edge.value; });
        const auto z = static_cast<std::uint32_t>(layer - firstInterior);
        return (z * grid.tileCount[1] + tile[1]) * grid.tileCount[0] + tile[0];
    }
}

namespace Physara::Renderer::Detail
{
    // 在 FP32 坐标变换、距离窗及方向归一化的精度边界外才剔除候选.
    static constexpr double FloatSlack = 32 * std::numeric_limits<float>::epsilon();
    static constexpr auto DepthEdgeCapacity = std::tuple_size_v<decltype(StaticAbi::FroxelView::depthEdges)>;
    static_assert(StaticAbi::FroxelGlobalLightsCapacity >= StaticAbi::LightRowsCapacity);

    /// @brief 在已检查的 FP32 值域内向外舍入, 使发布的边界包含源范围.
    static float RoundOut(double value, bool upper) noexcept
    {
        const auto rounded = static_cast<float>(value);
        if (upper ? rounded < value : rounded > value)
        {
            const auto direction = upper ? std::numeric_limits<float>::infinity() :
                                           -std::numeric_limits<float>::infinity();
            return std::nextafter(rounded, direction);
        }
        return rounded;
    }

    /// @brief 按相机类型生成层边界. 端点保守覆盖稳定范围, 挤在同一 FP32 值的分层不可查询.
    static bool PackDepthEdges(const CameraSnapshot &camera, StaticAbi::FroxelView &grid) noexcept
    {
        if (camera.cullingFar > std::numeric_limits<float>::max())
        {
            return false;
        }
        const double near = camera.nearPlane;
        const double far = camera.cullingFar;
        const auto slices = grid.sliceCount;
        grid.depthEdges[0].value = RoundOut(near, false);
        grid.depthEdges[slices].value = RoundOut(far, true);
        // GPU 可清零非正规值. 正交的精确零近面单独允许, 其余边界必须能稳定参加比较.
        if ((!camera.orthographic && grid.depthEdges[0].value < std::numeric_limits<float>::min()) ||
            !std::isfinite(grid.depthEdges[slices].value))
        {
            return false;
        }
        if (grid.depthEdges[0].value < std::numeric_limits<float>::min())
        {
            grid.depthEdges[0].value = 0;
        }
        const double low = camera.orthographic ? near : std::log2(near);
        const double high = camera.orthographic ? far : std::log2(far);
        for (std::uint32_t i = 1; i < slices; ++i)
        {
            const auto edge = std::lerp(low, high, static_cast<double>(i) / slices);
            const auto depth = camera.orthographic ? edge : std::exp2(edge);
            if (!(depth <= std::numeric_limits<float>::max()))
            {
                return false;
            }
            grid.depthEdges[i].value = static_cast<float>(depth);
        }
        for (std::uint32_t i = 1; i <= slices; ++i)
        {
            const auto depth = grid.depthEdges[i].value;
            if (!(depth >= std::numeric_limits<float>::min() && depth > grid.depthEdges[i - 1].value))
            {
                return false;
            }
        }
        return true;
    }

    /// @brief 把当前 View 的有效矩形和稳定边界装入生成布局, 先检查网格尺寸再分配任何数组.
    static FroxelStatus PackGrid(const PreparedView &view, const FroxelRequest &request,
                                  const std::array<double, 2> &sideLimit, StaticAbi::FroxelView &grid) noexcept
    {
        if (request.tileExtent.width == 0 || request.tileExtent.height == 0 ||
            request.sliceCount == 0 || request.sliceCount >= DepthEdgeCapacity)
        {
            return FroxelStatus::InvalidGrid;
        }
        const auto &rect = view.layouts.render.rect;
        const auto &camera = *view.camera;
        grid.renderRect = {rect.x, rect.y, rect.extent.width, rect.extent.height};
        grid.tileExtent = {request.tileExtent.width, request.tileExtent.height};
        grid.sliceCount = request.sliceCount;
        grid.orthographic = camera.orthographic ? 1u : 0u;
        // 只为片元查询保存相机相对变换. 透视和正交共用稳定投影的两个轴向系数.
        // 投影尺度或相机高位超出这里的编码范围时, 保持零尺度以触发完整灯退路.
        // cameraLow 保存眼点减去高位后的 FP32 近似. CPU 分箱仍使用快照中的双精度眼点.
        constexpr auto FloatLimit = std::numeric_limits<float>::max();
        constexpr auto FloatMin = std::numeric_limits<float>::min();
        const std::array origin{camera.worldFromView[12], camera.worldFromView[13], camera.worldFromView[14]};
        const auto scaleX = camera.stableClipFromView[0];
        const auto scaleY = camera.stableClipFromView[5];
        if (scaleX >= FloatMin && scaleX <= FloatLimit && scaleY >= FloatMin && scaleY <= FloatLimit &&
            StoreGpuVector(grid.cameraHigh, origin))
        {
            grid.cameraLow = GpuPositionLow(origin, grid.cameraHigh);
            for (std::size_t column = 0; column < grid.viewBasis.size(); ++column)
            {
                const auto base = column * 4;
                grid.viewBasis[column].value = {static_cast<float>(camera.viewFromWorld[base]),
                    static_cast<float>(camera.viewFromWorld[base + 1]),
                    static_cast<float>(camera.viewFromWorld[base + 2])};
            }
            grid.projectionScale = {static_cast<float>(scaleX), static_cast<float>(scaleY)};
        }

        // Shader 先把像素限制在矩形内再转回整数 tile. 画布端点需能在 FP32 中精确表示.
        constexpr auto ExactPixels = 1u << std::numeric_limits<float>::digits;
        std::uint64_t cells = grid.sliceCount;
        for (std::size_t axis = 0; axis < grid.tileExtent.size(); ++axis)
        {
            const auto extent = grid.renderRect[axis + 2];
            if (std::uint64_t(grid.renderRect[axis]) + extent > ExactPixels)
            {
                return FroxelStatus::UnrepresentableGrid;
            }
            const auto guard = (sideLimit[axis] - 1) * (static_cast<double>(extent) * 0.5);
            const double upper = extent + guard;
            const auto jitter = camera.jitterPixels[axis];
            if (!(upper <= std::numeric_limits<float>::max() &&
                  std::abs(jitter) <= std::numeric_limits<float>::max()))
            {
                return FroxelStatus::UnrepresentableGrid;
            }
            grid.stableMinimum[axis] = RoundOut(-guard, false);
            grid.stableMaximum[axis] = RoundOut(upper, true);
            grid.jitterPixels[axis] = static_cast<float>(jitter);
            grid.tileCount[axis] = 1 + (extent - 1) / grid.tileExtent[axis];
            cells *= grid.tileCount[axis];
        }
        if (cells > StaticAbi::FroxelHeadersCapacity)
        {
            return FroxelStatus::HeaderCapacity;
        }
        return PackDepthEdges(camera, grid) ? FroxelStatus::Ready : FroxelStatus::UnrepresentableGrid;
    }

    /// @brief 一个平面网格轴的归属边界和用于相交的闭区间. 像素随轴从左或上向外增长.
    struct TileAxis
    {
        std::vector<double> edges;
        std::vector<std::array<CullingPlane, 2>> planes;
        std::uint32_t extent;
        double scale;
        double pixelSlack;
    };

    /// @brief 稳定像素边界映到裁剪半空间. 视空间使用右、下、正向视深作为三轴.
    static CullingPlane TilePlane(glm::length_t axis, double ndc, double scale, bool orthographic) noexcept
    {
        glm::dvec3 normal(0);
        if (orthographic)
        {
            normal[axis] = 1;
            return {normal, -ndc / scale};
        }
        const auto length = std::hypot(scale, ndc);
        normal[axis] = scale / length;
        normal.z = -ndc / length;
        return {normal, 0};
    }

    /// @brief 预备一个轴的 tile 外缘和平面, 为像素变换与交界查询留出 FP32 余量.
    static TileAxis BuildTileAxis(const CameraSnapshot &camera, const StaticAbi::FroxelView &grid,
                                  glm::length_t axis)
    {
        const auto i = static_cast<std::size_t>(axis);
        const auto extent = grid.renderRect[i + 2];
        const auto count = grid.tileCount[i];
        const auto scale = camera.stableClipFromView[i * 5];
        const double slack = FloatSlack * (static_cast<double>(grid.renderRect[i]) + grid.stableMaximum[i]);
        TileAxis tiles{.extent = extent, .scale = scale, .pixelSlack = slack};
        tiles.edges.reserve(count + 1);
        tiles.edges.push_back(grid.stableMinimum[i]);
        for (std::uint32_t tile = 1; tile < count; ++tile)
        {
            tiles.edges.push_back(static_cast<double>(tile * grid.tileExtent[i]));
        }
        tiles.edges.push_back(grid.stableMaximum[i]);
        tiles.planes.reserve(count);
        for (std::uint32_t tile = 0; tile < count; ++tile)
        {
            const auto low = (tiles.edges[tile] - slack) * 2 / extent - 1;
            const auto high = (tiles.edges[tile + 1] + slack) * 2 / extent - 1;
            const auto left = TilePlane(axis, low, scale, camera.orthographic);
            const auto right = TilePlane(axis, high, scale, camera.orthographic);
            tiles.planes.push_back({left, {-right.normal, -right.offset}});
        }
        return tiles;
    }

    /// @brief 候选格的半开下标区间 [first, last). 与用于相切判定的闭合几何边界分别处理.
    struct CellSpan
    {
        std::size_t first;
        std::size_t last;
    };

    /// @brief 由保守闭区间取得可能相交的半开 cell 下标范围. 相切保留在两侧.
    static CellSpan IntersectingCells(std::span<const double> edges, double low, double high) noexcept
    {
        const auto upperEdges = edges.begin() + 1;
        const auto lowerEnd = edges.end() - 1;
        const auto first = std::lower_bound(upperEdges, edges.end(), low) - upperEdges;
        const auto last = std::upper_bound(edges.begin(), lowerEnd, high) - edges.begin();
        return {static_cast<std::size_t>(first), static_cast<std::size_t>(last)};
    }

    /// @brief 将球的轴向包围区间投到稳定像素. 透视除法只使用已裁入网格的正向视深.
    static CellSpan ProjectTiles(const TileAxis &tiles, double center, double bound, double near, double far,
                                  bool orthographic) noexcept
    {
        double low = center - bound;
        double high = center + bound;
        if (!orthographic)
        {
            low /= low < 0 ? near : far;
            high /= high > 0 ? near : far;
        }
        low = (low * tiles.scale + 1) * (tiles.extent * 0.5);
        high = (high * tiles.scale + 1) * (tiles.extent * 0.5);
        if (!std::isfinite(low) || !std::isfinite(high))
        {
            return {0, tiles.planes.size()};
        }
        return IntersectingCells(tiles.edges, low - tiles.pixelSlack, high + tiles.pixelSlack);
    }

    /// @brief 变换后聚光影响体的单位轴和外角. 空 cone 表示完整影响球.
    struct SpotCone
    {
        glm::dvec3 axis;
        double cosine;
        double sine;
    };

    /// @brief 单灯两遍分箱共用的视空间影响体. 三轴跨度限制遍历, centerSlack 是米制位置误差余量.
    /// lightRow 已计入方向灯前缀, 填写候选时可直接与阴影引用配对.
    struct LocalVolume
    {
        glm::dvec3 center;
        double radius;
        double centerSlack;
        std::optional<SpotCone> cone;
        CellSpan x;
        CellSpan y;
        CellSpan z;
        std::uint32_t lightRow;
    };

    /// @brief 将有限灯转成右、下、正向视深空间, 使用球的投影限制搜寻范围.
    static LocalVolume MakeVolume(const LightRecord &record, std::uint32_t lightRow,
                                   const std::array<glm::dvec3, 3> &axes, const glm::dvec3 &origin,
                                   const std::array<TileAxis, 2> &tiles, std::span<const double> depths,
                                   double cameraSlack, bool orthographic) noexcept
    {
        LocalVolume light{.radius = *record.light.range * (1 + FloatSlack), .lightRow = lightRow};
        const auto position = glm::make_vec3(record.position.data());
        double transformSlack = 0;
        for (glm::length_t axis = 0; axis < 3; ++axis)
        {
            const auto terms = PlaneTerms(position, origin, axes[static_cast<std::size_t>(axis)]);
            light.center[axis] = (terms.x + terms.y) + terms.z;
            constexpr double Rounding = 32 * std::numeric_limits<double>::epsilon();
            transformSlack += Rounding * std::abs(terms.x) + Rounding * std::abs(terms.y) +
                Rounding * std::abs(terms.z);
        }
        light.centerSlack = cameraSlack + transformSlack +
                            FloatSlack * std::hypot(light.center.x, light.center.y, light.center.z);
        // 可着色的局部灯表保存完整源位置, 因此灯位与 CPU 影响体使用同一中心.
        if (!std::isfinite(light.centerSlack))
        {
            // 极端相对变换无法证明任何分离. 保持候选范围, 避免非有限值进入区间查找.
            light.center = glm::dvec3(0);
            light.centerSlack = std::numeric_limits<double>::infinity();
        }
        if (record.light.type == Asset::ModelLightKind::Spot)
        {
            // 放宽平方弦长所对应的方向比较. 外锥接近半球时也保留放宽后的方向界.
            const auto angle = record.light.outerConeAngle + FloatSlack;
            const auto worldAxis = glm::make_vec3(record.direction.data());
            const auto axis = glm::normalize(glm::dvec3(glm::dot(worldAxis, axes[0]),
                                                        glm::dot(worldAxis, axes[1]),
                                                        glm::dot(worldAxis, axes[2])));
            light.cone = SpotCone{axis, std::cos(angle), std::sin(angle)};
        }
        const double bound = light.radius + light.centerSlack;
        const double near = std::max(depths.front(), light.center.z - bound);
        const double far = std::min(depths.back(), light.center.z + bound);
        if (near <= far)
        {
            light.z = IntersectingCells(depths, near, far);
            light.x = ProjectTiles(tiles[0], light.center.x, bound, near, far, orthographic);
            light.y = ProjectTiles(tiles[1], light.center.y, bound, near, far, orthographic);
        }
        return light;
    }

    /// @brief 只用可证明的半空间分离剔除. 聚光支持距离乘光照半径, 中心误差在各方向独立放宽.
    static bool OutsideLight(const CullingPlane &plane, const LocalVolume &light) noexcept
    {
        double reach = light.radius;
        if (light.cone)
        {
            const auto &cone = *light.cone;
            reach *= SupportSpot(plane.normal, cone.axis, cone.cosine, cone.sine).distance + FloatSlack;
        }
        return Outside(plane, light.center, glm::dvec3(0), reach + light.centerSlack);
    }

    /// @brief 按同一几何判定枚举一个灯的所有成员, 计数和填充共用判定与遍历顺序.
    template <class Visit>
    static void VisitCells(const LocalVolume &light, const std::array<TileAxis, 2> &tiles,
                           std::span<const double> depths, const Visit &visit)
    {
        for (std::size_t z = light.z.first; z < light.z.last; ++z)
        {
            if (light.cone && (OutsideLight({{0, 0, 1}, -depths[z]}, light) ||
                               OutsideLight({{0, 0, -1}, depths[z + 1]}, light)))
            {
                continue;
            }
            for (std::size_t y = light.y.first; y < light.y.last; ++y)
            {
                const auto &planesY = tiles[1].planes[y];
                if (OutsideLight(planesY[0], light) || OutsideLight(planesY[1], light))
                {
                    continue;
                }
                for (std::size_t x = light.x.first; x < light.x.last; ++x)
                {
                    const auto &planesX = tiles[0].planes[x];
                    if (!OutsideLight(planesX[0], light) && !OutsideLight(planesX[1], light))
                    {
                        visit((z * tiles[1].planes.size() + y) * tiles[0].planes.size() + x);
                    }
                }
            }
        }
    }

    /// @brief 首遍按灯统计候选, 按扁平格序分配有限索引, 再按原灯行顺序填充获准区间.
    /// 全局局部灯只在首遍登记. 零预算也须保留它们, 并区分空格与有候选的溢出格.
    static void PackLightLists(const PreparedView &view, std::uint32_t indexBudget, FroxelLightLists &result)
    {
        auto &grid = result.grid;
        const auto &camera = *view.camera;
        result.headers.resize(std::size_t(grid.tileCount[0]) * grid.tileCount[1] * grid.sliceCount);
        if (view.lights.size() == grid.directionalCount)
        {
            return;
        }

        std::array<double, DepthEdgeCapacity> depthStorage;
        const auto depths = std::span(depthStorage).first(grid.sliceCount + 1);
        for (std::size_t i = 0; i < depths.size(); ++i)
        {
            depths[i] = grid.depthEdges[i].value;
        }
        const std::array tiles{BuildTileAxis(camera, grid, 0), BuildTileAxis(camera, grid, 1)};
        const auto basis = glm::make_mat4(camera.worldFromView.data());
        const std::array axes{glm::dvec3(basis[0]), -glm::dvec3(basis[1]), -glm::dvec3(basis[2])};
        const glm::dvec3 origin(basis[3]);
        double cameraSlack = 0;
        if (grid.projectionScale[0] != 0)
        {
            // 这里计入眼点上传时丢失的米数. 片元随后还要用这个眼点与插值后的表面作差.
            // 各世界分量的绝对余差之和界定旋转后的距离, 同时放宽球跨度和聚光半空间判定.
            for (glm::length_t axis = 0; axis < 3; ++axis)
            {
                const auto index = static_cast<std::size_t>(axis);
                cameraSlack += std::abs((origin[axis] - grid.cameraHigh[index]) - grid.cameraLow[index]);
            }
            cameraSlack *= 1 + FloatSlack;
        }
        // 遍历可见灯时每盏先取正式行号, 再按 range 分流. 全局灯也占有一行.
        LightRowCursor cursor{.local = grid.directionalCount};
        std::vector<LocalVolume> finiteLights;
        finiteLights.reserve(view.lights.size() - grid.directionalCount);
        for (const auto index : view.lights)
        {
            const auto &record = view.scene.lights[index];
            const auto row = static_cast<std::uint32_t>(cursor.Next(record.light.type));
            if (record.light.type == Asset::ModelLightKind::Directional)
            {
                continue;
            }
            if (record.light.range)
            {
                const auto &light = finiteLights.emplace_back(MakeVolume(record, row, axes, origin, tiles, depths,
                                                                          cameraSlack, camera.orthographic));
                VisitCells(light, tiles, depths, [&](std::size_t cell)
                {
                    ++result.headers[cell].count;
                });
            }
            else
            {
                result.globalLights.push_back({row});
            }
        }

        // 完整接纳一格才划出区间. 溢出格保留后续格可用的额度, 由 Shader 遍历完整局部灯表.
        std::uint32_t offset = 0;
        for (auto &header : result.headers)
        {
            if (header.count > indexBudget - offset)
            {
                header = {.overflow = 1};
                ++result.overflowCellCount;
                continue;
            }
            header.offset = offset;
            offset += header.count;
            header.count = 0;
        }
        result.indices.resize(offset);
        if (offset != 0)
        {
            // 复走相同候选, 只填获准的区间. 灯行顺序与计数阶段一致.
            for (const auto &light : finiteLights)
            {
                VisitCells(light, tiles, depths, [&](std::size_t cell)
                {
                    auto &header = result.headers[cell];
                    if (header.overflow == 0)
                    {
                        const auto index = header.offset + header.count++;
                        assert(index < result.indices.size());
                        result.indices[index].lightRow = light.lightRow;
                    }
                });
            }
        }
        grid.indexCount = offset;
        grid.globalCount = static_cast<std::uint32_t>(result.globalLights.size());
    }

    std::variant<std::unique_ptr<FroxelLightLists>, FroxelStatus> BuildFroxelLightLists(
        const PreparedView &view, const FroxelRequest &request, std::array<double, 2> sideLimit,
        std::uint32_t directionalCount)
    {
        StaticAbi::FroxelView grid{};
        if (const auto status = PackGrid(view, request, sideLimit, grid); status != FroxelStatus::Ready)
        {
            return status;
        }
        if (view.lights.size() > StaticAbi::LightRowsCapacity)
        {
            return FroxelStatus::LightCapacity;
        }
        try
        {
            auto result = std::make_unique<FroxelLightLists>();
            result->grid = grid;
            result->grid.directionalCount = directionalCount;
            PackLightLists(view, request.IndexBudget(), *result);
            return result;
        }
        catch (const std::bad_alloc &)
        {
            return FroxelStatus::StorageUnavailable;
        }
    }
}
