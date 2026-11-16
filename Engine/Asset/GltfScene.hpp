#pragma once

/// @file
/// @brief 选择并验证静态层级. 写入的 mesh/camera/light 引用暂为源下标.

#include "GltfDocument.hpp"

#include <Asset/ModelAsset.hpp>

namespace Physara::Asset::Gltf
{
    /// @brief 选择默认/显式 scene, 校验静态层级并写入源节点和报告, 保留源根及子节点顺序.
    /// @pre record 是新的未发布候选. 失败可能留下部分局部结果, 调用方必须整体丢弃.
    /// @note 此阶段仅重映射 children/roots, mesh/camera/light 暂存源下标供后续表构建.
    void SelectScene(const Json &root, ModelImportOptions options, std::stop_token stop, ModelRecord &record);
}
