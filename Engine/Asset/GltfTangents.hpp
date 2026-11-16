#pragma once

/// @file
/// @brief 固定 MikkTSpace 的角点适配及框架数值校验, 不决定顶点共享或修改源几何.

#include "GltfDocument.hpp"

#include <Asset/ModelAsset.hpp>

namespace Physara::Asset::Gltf
{
    /// @brief 为有效三角形生成角点切线. 返回顺序与 indices 一致, 原 UV 保持不变.
    /// @pre 索引非空且按三角形分组, 位置/法线/指定 UV 已验证, 索引均在 vertices 内.
    /// @param location 调用方提供的字段定位, 用于导入 JSON 或运行期覆盖的有界错误正文.
    /// @note scratchLimit 是扣除几何工作存储后的余额. 超限或无可用框架抛出导入失败, 取消单独传播.
    [[nodiscard]] std::vector<std::array<float, 4>>
    GenerateTangents(std::span<const ModelVertex> vertices, std::span<const std::uint32_t> indices,
                     std::uint32_t uv, std::string_view location, std::size_t scratchLimit, std::stop_token stop);

    /// @brief 按导入容差检查单位长度, 非有限结果不通过.
    [[nodiscard]] bool IsUnit(std::span<const float, 3> direction) noexcept;
    /// @brief 在 normal 已验证的前提下核对单位切线、正交性和精确的 +/-1 handedness.
    [[nodiscard]] bool IsTangentFrame(const ModelVertex &vertex) noexcept;
}
