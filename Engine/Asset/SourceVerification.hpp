#pragma once

/// @file
/// @brief 在文件操作边界核对固定 CPU 版本的真实输入, 无需请求、发布或重新解码.

#include <stop_token>

#include <Asset/ModelAsset.hpp>

namespace Physara::Asset
{
    /// @brief Changed 表示字节或长度已变化, Unavailable 表示本次未取得可比较的完整读快照.
    enum class SourceMatch : std::uint8_t
    {
        Unchanged,
        Changed,
        Unavailable
    };

    /// @brief 源核对结果及首个失败定位, 不改变被核对的 CPU 版本或 AssetManager 状态.
    struct SourceVerification
    {
        SourceMatch result;
        Core::BoundedText detail;
    };

    /// @brief 同步 worker 入口. 使用资源根与不可变记录, 结果包含首个不匹配文件的定位.
    /// @note 模型依次核对主文件和实际依赖, 普通图像和环境分别在各自目录核对单份源字节.
    /// 证明只对应各次受控读快照, 不锁定之后的外部修改. 取消归入 Unavailable.
    [[nodiscard]] SourceVerification VerifySource(const std::filesystem::path &root, const ModelRecord &model,
                                                  std::stop_token stop = {});
    [[nodiscard]] SourceVerification VerifySource(const std::filesystem::path &root, const ImageRecord &image,
                                                  std::stop_token stop = {});
    [[nodiscard]] SourceVerification VerifySource(const std::filesystem::path &root, const EnvironmentRecord &image,
                                                  std::stop_token stop = {});
}
