#pragma once

/// @file
/// @brief 将一个 View 选出的灯编码成 StaticForward 接口的只读记录.
/// 输入借用同一份冻结场景, 结果拥有整块上传数据. 可见性由 ViewWork 决定.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

#include <Core/Error.hpp>
#include <StaticForward/Data.hpp>

#include "../Prepare/SceneFrame.hpp"

namespace Physara::Renderer::Detail
{
    namespace StaticAbi = Shader::Generated::StaticForward;

    /// @brief 按同一可见序列填写灯表和阴影引用表. 两次遍历各自把 local 初始化为方向灯总数.
    /// 阴影预算只决定行内的组引用, 被拒灯仍占用自己的行号.
    struct LightRowCursor
    {
        std::size_t directional{};
        std::size_t local{};

        [[nodiscard]] std::size_t Next(Asset::ModelLightKind kind) noexcept
        {
            return kind == Asset::ModelLightKind::Directional ? directional++ : local++;
        }
    };

    /// @brief rows 覆盖生成的定长 SSBO, 前 directionalCount 行为方向灯, 随后 localCount 行为点光或聚光.
    /// 两段内分别保留可见索引顺序. 尾部置零以便整块初始化, 着色只遍历两段的有效行.
    struct EncodedLightSet
    {
        std::vector<StaticAbi::ForwardLight> rows;
        std::uint32_t directionalCount;
        std::uint32_t localCount;
    };

    /// @brief 按分量将世界空间的位置, 位移或方向舍入到 FP32.
    /// @return 任一分量越出有限范围时返回 false, 先前分量可能已写入, 调用方须放弃本候选.
    [[nodiscard]] bool StoreGpuVector(std::array<float, 3> &output, std::span<const double, 3> input) noexcept;

    /// @brief 在 double 中计算世界位置减去高位的米制残差, 再逐分量舍入为 FP32.
    /// @pre high 是同一 position 各分量的有限 FP32 编码. 两部分共用场景世界坐标原点.
    [[nodiscard]] std::array<float, 3> GpuPositionLow(std::span<const double, 3> position,
                                                       std::span<const float, 3> high) noexcept;

    /// @brief 通过 GPU 编码边界检查整份灯集合, 保留原光度单位和可选范围.
    /// @pre indices 来自同一次成功的 View 准备, 借用的 SceneFrame 在调用期间有效.
    /// @return 成功时拥有全部行. 超容量或字段不可编码时返回有界原因, 单灯错误附带实体槽和字段名.
    /// @throws std::bad_alloc 行存储分配失败, 由 Forward 准备入口转换为本轮失败.
    [[nodiscard]] std::variant<EncodedLightSet, Core::BoundedText> EncodeLightSet(
        std::span<const LightRecord> lights, std::span<const std::uint32_t> indices);
}
