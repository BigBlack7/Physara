#pragma once

/// @file
/// @brief Asset 私有解码接口和像素额度. 额度与拥有引用共享寿命, 可由 worker 或最终读者退还.

#include "AssetManager.hpp"
#include "ByteBudget.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <variant>

namespace Physara::Asset
{
    using PixelBudget = ByteBudget;

    /// @brief 将受控快照解码成拥有型 RGBA8, 有效字节占用额度直到最后一个读者释放.
    /// 支持标准 PNG 的至多 8 位样本和 8 位 JPEG; CgBI 明确返回 UnsupportedEncoding.
    /// @pre budget 非空, bytes 仅在本次调用期间借用, imageLimit 是单张输出字节上限.
    /// @return 成功值独立拥有像素; 格式、布局或预算失败不交付部分图像, 已预留额度自动退还.
    /// @note 不读取文件或推进资产状态. C++ 分配异常由 ImportAsset 的 worker 边界转换.
    [[nodiscard]] std::variant<Rgba8Image, AssetFailure>
    DecodeImage(std::span<const std::byte> bytes, std::size_t imageLimit, const std::shared_ptr<PixelBudget> &budget);
}
