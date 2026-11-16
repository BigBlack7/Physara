#pragma once

/// @file
/// @brief 单个模型作业的组合入口, 连接元数据选择、依赖指纹和拥有型候选构建.

#include "ByteBudget.hpp"

#include <Asset/ModelAsset.hpp>
#include <Core/ResourceIO.hpp>

namespace Physara::Asset::Gltf
{
    /// @brief 完整模型候选, 或相同输入时返回空共享引用. 失败和取消由 worker 边界统一转换.
    /// 候选未分配 AssetId 或 ContentRevision, 只交给 AssetManager 的 owner 裁决.
    /// @pre source/path/root/limits 在调用期间有效, source 已由 Core 限额读取, 两个共享预算非空.
    /// @note previous 只用于已发布版本的内容比较. 失败不返回部分记录, 局部候选和预留容量随展开释放.
    [[nodiscard]] std::shared_ptr<ModelRecord>
    DecodeModel(const Core::ResourceBytes &source, const std::filesystem::path &root,
                const Core::ResourcePath &path, ModelImportOptions options, const ImportLimits &limits,
                const std::optional<Core::Sha256Digest> &previous, const std::shared_ptr<ByteBudget> &pixels,
                const std::shared_ptr<ByteBudget> &geometry, std::stop_token stop);
}
