#pragma once

/// @file
/// @brief 在已验证的静态几何上为另一有效 UV 集生成切线及接缝索引.

#include "ModelAsset.hpp"

#include <optional>
#include <string_view>

namespace Physara::Asset
{
    /// @brief 实例切换 UV 时, 领域校验和渲染准备共用的切线工作空间上界.
    inline constexpr std::size_t StaticTangentWorkLimit = 64 * 1024 * 1024;

    /// @brief 用正式角点生成器检查有效 UV 的单位切线框架, 供实例发布前校验.
    /// @pre uv 存在于已发布的 primitive.
    /// @return 缺少可用框架时返回带槽位置的错误. 临时角点不会加入场景或 GPU 几何缓存.
    /// @throws std::bad_alloc 普通分配失败由场景编辑或文档候选入口处理.
    [[nodiscard]] std::optional<AssetFailure> ValidateNormalUvFrame(const ModelPrimitive &primitive, std::uint32_t uv,
                                                                   std::string_view location);

    /// @brief 选择源切线或为指定有效 UV 构造切线. 缺席用于可选的第二框架.
    enum class TangentSource : std::int8_t { Absent = -2, Original = -1, Uv0 = 0, Uv1 = 1 };

    /// @brief 基底和清漆接缝并集上的独立几何. 清漆附加流按相同顶点索引寻址.
    struct NormalUvGeometry
    {
        ModelPrimitive primitive;
        std::vector<std::array<float, 4>> coatTangents;
    };

    /// @brief 使用与导入相同的角点算法, 按实际需要的框架联合拆点.
    /// @pre primitive 为已发布的非空三角形. 所选 UV 均存在, Original 复用固定顶点内已有的切线.
    /// @return 自有几何或明确的切线/工作空间错误. 源几何仅在本次同步准备中借用.
    /// @param workLimit 本次切线生成与拆点的工作字节上限, 不包含已发布源资产的保有容量.
    /// @throws std::bad_alloc 普通 CPU 分配失败交给 Renderer 的准备边界处理.
    [[nodiscard]] std::variant<NormalUvGeometry, AssetFailure> BuildNormalUvGeometry(
        const ModelPrimitive &primitive, TangentSource primary, TangentSource secondary, std::size_t workLimit,
        std::string_view primaryLocation);
}
