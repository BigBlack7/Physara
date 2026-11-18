#include "PunctualLights.hpp"

/// @file
/// @brief 按生成布局编码完整可见灯表, 再为本次颜色 Pass 选择 GPU 缓冲.
/// 非空表在安全帧槽内更新, 空表由设备共享. 上传字节交给图, 接受状态由提交方确认.

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <limits>
#include <string_view>

#include "../Materials/StaticGpu.hpp"

namespace Physara::Renderer::Detail
{
    using LightKind = Asset::ModelLightKind;

    // GPU 可将非正规数清为零. 灯的正强度和显式半径都需要保留其非零语义.
    static constexpr float MinimumNormal = std::numeric_limits<float>::min();

    /// @brief 只将落在 FP32 有限范围内的值写入目标. 保留正值语义的约束由字段施加.
    static bool StoreFloat(float &output, double value) noexcept
    {
        constexpr double max = std::numeric_limits<float>::max();
        if (!(value >= -max && value <= max))
        {
            return false;
        }
        output = static_cast<float>(value);
        return true;
    }

    bool StoreGpuVector(std::array<float, 3> &output, std::span<const double, 3> input) noexcept
    {
        for (std::size_t axis = 0; axis < input.size(); ++axis)
        {
            if (!StoreFloat(output[axis], input[axis]))
            {
                return false;
            }
        }
        return true;
    }

    std::array<float, 3> GpuPositionLow(std::span<const double, 3> position,
                                       std::span<const float, 3> high) noexcept
    {
        std::array<float, 3> low{};
        for (std::size_t axis = 0; axis < low.size(); ++axis)
        {
            // 差值在 double 中取得, 其绝对值至多为位置窄化的舍入误差.
            low[axis] = static_cast<float>(position[axis] - static_cast<double>(high[axis]));
        }
        return low;
    }

    /// @brief 以弦长平方表示余弦过渡, 并检查 FP32 方向求值有足够的过渡分辨率.
    static bool EncodeSpotCone(StaticAbi::ForwardLight &row, const Scene::Light &light) noexcept
    {
        // (2 * sin(theta / 2))^2 等于 2 * (1 - cos(theta)). 近轴时先求半角正弦可保留端点间距.
        const double inner = 2 * std::sin(light.innerConeAngle * 0.5);
        const double outer = 2 * std::sin(light.outerConeAngle * 0.5);
        row.innerChordSquared = static_cast<float>(inner * inner);
        row.outerChordSquared = static_cast<float>(outer * outer);

        // 为方向编码、归一化及方向差估计 8 个 FP32 epsilon 的弦长误差余量.
        // 弦长误差 d 对平方值的影响至多 (2 * outer + d) * d, 再计入端点窄化.
        // 端点间距至少取该估计的 128 倍, 约束的是 Shader 平方前的归一化插值系数.
        constexpr double epsilon = std::numeric_limits<float>::epsilon();
        constexpr double chordError = 8 * epsilon;
        const double error = (2 * outer + chordError) * chordError + epsilon * outer * outer;
        return static_cast<double>(row.outerChordSquared) - row.innerChordSquared >= 128 * error;
    }

    /// @brief 编码 Scene 已校验的线性 RGB 和光度值. 空串表示成功, 其余结果是首个失败字段.
    static std::string_view EncodeLight(StaticAbi::ForwardLight &row, const LightRecord &record) noexcept
    {
        const auto &light = record.light;
        // 方向灯保留 lux, 局部灯保留 candela. 世界位置用米, 方向沿节点的局部 -Z 发射轴.
        row.color = light.color;
        if (!StoreFloat(row.intensity, light.intensity) || (light.intensity > 0 && row.intensity < MinimumNormal))
        {
            return "intensity";
        }
        if (light.type != LightKind::Directional)
        {
            if (!StoreGpuVector(row.positionHigh, record.position))
            {
                return "position";
            }
            row.positionLow = GpuPositionLow(record.position, row.positionHigh);
        }
        if (light.type != LightKind::Point && !StoreGpuVector(row.direction, record.direction))
        {
            return "direction";
        }
        // 零专用于缺省范围. 显式半径须在 GPU 上保留正值.
        if (light.range && (!StoreFloat(row.range, *light.range) || row.range < MinimumNormal))
        {
            return "range";
        }
        if (light.type == LightKind::Spot && !EncodeSpotCone(row, light))
        {
            return "spot cone transition precision";
        }
        return {};
    }

    std::variant<EncodedLightSet, Core::BoundedText> EncodeLightSet(
        std::span<const LightRecord> lights, std::span<const std::uint32_t> indices)
    {
        if (indices.size() > StaticAbi::LightRowsCapacity)
        {
            return Core::BoundedText("Visible light set exceeds the Forward light table capacity");
        }
        // 先确定局部段的起点. 第二遍用两个独立写游标保持各段原序, 任一行失败即放弃整表.
        const auto count = static_cast<std::uint32_t>(std::ranges::count_if(indices, [&](auto index)
        {
            assert(index < lights.size() && lights[index].visible);
            return lights[index].light.type == LightKind::Directional;
        }));
        EncodedLightSet table{std::vector<StaticAbi::ForwardLight>(StaticAbi::LightRowsCapacity), count,
                             static_cast<std::uint32_t>(indices.size()) - count};
        std::size_t directional = 0;
        std::size_t local = count;
        for (const auto index : indices)
        {
            const auto &record = lights[index];
            auto &row = table.rows[record.light.type == LightKind::Directional ? directional++ : local++];
            if (const auto field = EncodeLight(row, record); !field.empty())
            {
                Core::BoundedText error("Forward light entity slot ");
                char entity[12];
                const auto end = std::to_chars(entity, entity + sizeof(entity), record.entity.index).ptr;
                error.Append(std::string_view(entity, end));
                error.Append(" has an unrepresentable ");
                error.Append(field);
                return error;
            }
        }
        return table;
    }

    /// @brief 按生成块的字节数和行距分配 SSBO. 此时内容未定义, 分配失败时局部 owner 归还额度.
    static std::shared_ptr<LightTable> CreateLightTable(ForwardDevice &device)
    {
        constexpr auto bytes = sizeof(StaticAbi::LightRowsData);
        device.Reserve(bytes);
        auto table = std::make_shared<LightTable>(device.capacity, bytes);
        const RHI::BufferDesc desc{.byteSize = bytes, .structStride = sizeof(StaticAbi::ForwardLight),
            .debugName = "Forward light table", .initialState = BufferBoundary, .keepInitialState = true};
        table->buffer = device.device.CreateBuffer(desc);
        if (!table->buffer)
        {
            Reject(ViewSettingReason::ScenePreparationFailed, "Cannot allocate the Forward light table");
        }
        return table;
    }

    /// @brief 共享已接受的全零块. 未接受的旧轮次可能已有 GPU 写入前缀, 新轮次另建存储.
    static std::shared_ptr<LightTable> EmptyLightTable(ForwardDevice &device, UploadBatch &uploads)
    {
        if (!device.emptyLights || !device.Current(*device.emptyLights))
        {
            auto table = CreateLightTable(device);
            const std::vector<StaticAbi::ForwardLight> rows(StaticAbi::LightRowsCapacity);
            uploads.Add(table, table->buffer, std::as_bytes(std::span(rows)));
            device.emptyLights = std::move(table);
        }
        return device.emptyLights;
    }

    void PrepareLightTable(ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view,
                           bool usesLighting)
    {
        frame.lightLayout = device.programs.lightLayout;
        if (!usesLighting || view.lights.empty())
        {
            frame.activeLights = EmptyLightTable(device, frame.uploads);
            device.Use(*frame.activeLights);
            return;
        }

        auto &table = frame.lightTable;
        // Scene 身份区分文档, 内容代区分同索引的灯值, 有序索引区分相机裁剪结果.
        // 位置高低分量来自场景世界坐标. 只改相机且可见索引一致时, 已确认的灯表仍可作为图的已初始化输入.
        if (!table || !table->accepted || table->scene != view.scene.scene ||
            table->contentRevision != view.scene.contentRevision || !std::ranges::equal(table->indices, view.lights))
        {
            auto encoded = EncodeLightSet(view.scene.lights, view.lights);
            if (const auto *error = std::get_if<Core::BoundedText>(&encoded))
            {
                throw ForwardFailure{ViewSettingReason::SceneLightTableRejected, *error};
            }
            if (!table)
            {
                table = CreateLightTable(device);
            }
            // 保留存储供安全重试. 对应图未接受时, 这份新编码不具备缓存资格.
            table->accepted = false;
            const auto &packed = std::get<EncodedLightSet>(encoded);
            frame.uploads.Add(table, table->buffer, std::as_bytes(std::span(packed.rows)));
            table->scene = view.scene.scene;
            table->contentRevision = view.scene.contentRevision;
            table->indices.assign(view.lights.begin(), view.lights.end());
            table->directionalCount = packed.directionalCount;
            table->localCount = packed.localCount;
        }
        frame.activeLights = table;
        device.Use(*frame.activeLights);
    }
}
