#include "PunctualLights.hpp"

/// @file
/// @brief 将世界灯编码为设备缓存行, 然后按该 View 的可见顺序组装方向灯段与局部灯段.
/// 非空表在安全帧槽内更新, 空表由设备共享. 上传字节交给图, 接受状态由提交方确认.

#include <algorithm>
#include <bit>
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

    /// @brief 逐轴保留双精度灯位的 53 位有效数. FP32 高位携带符号和指数, 同轴的 uint 保存尾部 29 位.
    /// @return 高位及拆出的两项尾数均可作为正规 FP32 或零交付时为 true. 失败时调用方放弃整行.
    static bool EncodeLightPosition(StaticAbi::ForwardLight &row, std::span<const double, 3> position) noexcept
    {
        static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
        constexpr int HeadDigits = std::numeric_limits<float>::digits;
        constexpr int SourceDigits = std::numeric_limits<double>::digits;
        constexpr int TailDigits = SourceDigits - HeadDigits;
        constexpr int ResidualDigits = TailDigits - HeadDigits;
        constexpr auto tailMask = (std::uint64_t{1} << TailDigits) - 1;
        constexpr auto residualMask = (std::uint32_t{1} << ResidualDigits) - 1;
        constexpr double max = std::numeric_limits<float>::max();
        for (std::size_t axis = 0; axis < position.size(); ++axis)
        {
            const double value = position[axis];
            if (!(value >= -max && value <= max) || (value != 0 && std::abs(value) < MinimumNormal))
            {
                return false;
            }
            // 截断有效数后再转换, 高位的符号和指数也用于还原同符号的低位.
            const auto bits = std::bit_cast<std::uint64_t>(value);
            const auto high = static_cast<float>(std::bit_cast<double>(bits & ~tailMask));
            const auto tail = static_cast<std::uint32_t>(bits & tailMask);
            int exponent = 0;
            std::frexp(high, &exponent);
            const auto supported = [exponent](std::uint32_t digits, int offset) noexcept {
                return digits == 0 || std::ldexp(static_cast<double>(digits), exponent - offset) >= MinimumNormal;
            };
            // Shader 可清零非正规数. 写入前按 GPU 拆分检查两个非零余量的实际量级.
            if (!supported(tail >> ResidualDigits, 2 * HeadDigits) ||
                !supported(tail & residualMask, SourceDigits))
            {
                return false;
            }
            row.positionHigh[axis] = high;
            row.positionTail[axis] = tail;
        }
        return true;
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
        if (light.type != LightKind::Directional && !EncodeLightPosition(row, record.position))
        {
            return "position encoding";
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

    std::uint32_t CountDirectionalLights(std::span<const LightRecord> lights,
                                         std::span<const std::uint32_t> indices) noexcept
    {
        return static_cast<std::uint32_t>(std::ranges::count_if(indices, [&](auto index)
        {
            assert(index < lights.size() && lights[index].visible);
            return lights[index].light.type == LightKind::Directional;
        }));
    }

    /// @brief 按方向灯和局部灯两段组装定长表. readRow 接收冻结场景下标, 将编码写到游标选中的 GPU 行.
    template <class ReadRow>
    static std::variant<EncodedLightSet, Core::BoundedText> AssembleLightSet(
        std::span<const LightRecord> lights, std::span<const std::uint32_t> indices, ReadRow readRow)
    {
        if (indices.size() > StaticAbi::LightRowsCapacity)
        {
            return Core::BoundedText("Visible light set exceeds the Forward light table capacity");
        }
        // 先确定局部段的起点. 第二遍用两个独立写游标保持各段原序, 任一行失败即放弃整表.
        const auto count = CountDirectionalLights(lights, indices);
        EncodedLightSet table{std::vector<StaticAbi::ForwardLight>(StaticAbi::LightRowsCapacity), count,
                             static_cast<std::uint32_t>(indices.size()) - count};
        LightRowCursor cursor{.local = count};
        for (const auto index : indices)
        {
            const auto &record = lights[index];
            auto &row = table.rows[cursor.Next(record.light.type)];
            if (const auto field = readRow(row, index); !field.empty())
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

    std::variant<EncodedLightSet, Core::BoundedText> EncodeLightSet(
        std::span<const LightRecord> lights, std::span<const std::uint32_t> indices)
    {
        return AssembleLightSet(lights, indices, [&](StaticAbi::ForwardLight &row, std::uint32_t index)
        {
            return EncodeLight(row, lights[index]);
        });
    }

    /// @brief 需要组表时整理世界灯行. 换 Scene 后清空缓存, 同一 Scene 换结构时按完整实体身份保留存活行.
    static void RefreshWorldLightCache(ForwardDevice &device, const SceneFrame &scene)
    {
        if (device.encodedLightScene != scene.scene)
        {
            device.encodedLights.clear();
            device.encodedLightScene = scene.scene;
        }
        else if (device.encodedLightStructure != scene.structureRevision)
        {
            decltype(device.encodedLights) live;
            for (const auto &light : scene.lights)
            {
                live.insert(device.encodedLights.extract({light.entity.index, light.entity.generation}));
            }
            device.encodedLights.swap(live);
        }
        device.encodedLightStructure = scene.structureRevision;
    }

    /// @brief 同灯的同版编码可由两路共享. 数值拒绝只在当前 View 实际选中该灯时返回.
    static std::variant<EncodedLightSet, Core::BoundedText> PrepareWorldLights(
        ForwardDevice &device, ForwardFrameData &frame, const PreparedView &view)
    {
        RefreshWorldLightCache(device, view.scene);
        return AssembleLightSet(view.scene.lights, view.lights, [&](StaticAbi::ForwardLight &row, std::uint32_t index)
        {
            const auto &record = view.scene.lights[index];
            auto [entry, inserted] = device.encodedLights.try_emplace(
                std::pair{record.entity.index, record.entity.generation});
            auto &encoded = entry->second;
            if (inserted || encoded.revision != record.lightRevision)
            {
                ++frame.encodings.lights;
                encoded = {.row = {}, .revision = record.lightRevision, .error = nullptr};
                encoded.error = EncodeLight(encoded.row, record).data();
            }
            row = encoded.row;
            return encoded.error ? std::string_view(encoded.error) : std::string_view{};
        });
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
        if (!usesLighting || view.lights.empty())
        {
            frame.activeLights = EmptyLightTable(device, frame.uploads);
            device.Use(*frame.activeLights);
            return;
        }

        auto &table = frame.lightTable;
        const auto source = [&](std::uint32_t index)
        {
            const auto &record = view.scene.lights[index];
            return LightSource{record.entity, record.lightRevision};
        };
        // 将本次紧凑索引映射回实体和数据代后逐项比较. 结构换代后同一下标可能属于另一盏灯.
        if (!table || !std::ranges::equal(table->sources, view.lights, {}, std::identity{}, source))
        {
            auto encoded = PrepareWorldLights(device, frame, view);
            if (const auto *error = std::get_if<Core::BoundedText>(&encoded))
            {
                throw ForwardFailure{ViewSettingReason::SceneLightTableRejected, *error};
            }
            if (!table)
            {
                table = CreateLightTable(device);
            }
            // 来源序列准备失败时保留旧配对, 只有完整的新 CPU 表才可重新上传.
            std::vector<LightSource> sources;
            sources.reserve(view.lights.size());
            for (const auto index : view.lights)
            {
                sources.push_back(source(index));
            }
            table->accepted = false;
            auto &packed = std::get<EncodedLightSet>(encoded);
            table->sources = std::move(sources);
            table->packed = std::move(packed.rows);
            table->directionalCount = packed.directionalCount;
            table->localCount = packed.localCount;
        }
        if (!table->accepted)
        {
            // 表的 CPU 字节随槽保留. 上次写入若被裁剪或只有失败前缀, 此次仍需提供完整生产者.
            frame.uploads.Add(table, table->buffer, std::as_bytes(std::span(table->packed)),
                              GraphUploadSource::ForwardStatic, device.preparingView);
        }
        frame.activeLights = table;
        device.Use(*frame.activeLights);
    }
}
