#pragma once

/// @file
/// @brief 环境导入作业的 EXR 解码入口. 输入字节与资产记录的源摘要来自同一次读取.

#include "AssetManager.hpp"
#include "ByteBudget.hpp"

namespace Physara::Asset
{
    /// @brief 在 worker 内把 RGB(A) 平面转成紧密的线性 RGB32F. 标定随像素单独返回.
    /// @param imageLimit 单次导入的字节额度, 合计扫描块偏移表, 解码平面与紧密 RGB 输出.
    /// @param budget 与其它图像版本共用的保有额度. 成功后由像素的最后一个读者归还.
    /// @return 拥有像素的版本载荷或导入失败原因. 分配异常由外层资产作业接收.
    [[nodiscard]] std::variant<EnvironmentImage, AssetFailure>
    DecodeEnvironment(std::span<const std::byte> bytes, std::size_t imageLimit,
                      const std::shared_ptr<ByteBudget> &budget);
}
