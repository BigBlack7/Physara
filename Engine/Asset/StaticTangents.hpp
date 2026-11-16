#pragma once

/// @file
/// @brief 在已验证的静态几何上为另一有效 UV 集生成切线及接缝索引.

#include "ModelAsset.hpp"

namespace Physara::Asset
{
    /// @brief 使用与导入相同的 MikkTSpace 角点算法和拆点规则, 保持原 Primitive 不变.
    /// @pre primitive 是已发布的非空三角形, uv 为 0 或 1 且该集合存在.
    /// @return 含新顶点和索引的几何, 或明确的切线/工作空间错误. 仅在同步准备时借用源.
    /// @param workLimit 本次切线生成与拆点的工作字节上限, 不包含已发布源资产的保有容量.
    /// @throws std::bad_alloc 普通 CPU 分配失败交给 Renderer 的准备边界处理.
    [[nodiscard]] std::variant<ModelPrimitive, AssetFailure> BuildNormalUvGeometry(
        const ModelPrimitive &primitive, std::uint32_t uv, std::size_t workLimit);
}
