#include "SourceVerification.hpp"

/// @file
/// @brief 逐个读取同一固定版本的源. 只保留当前文件的字节, 上界来自已接受的快照.

#include <array>

#include <Core/ResourceIO.hpp>

namespace Physara::Asset
{
    /// @brief 将失败归因到实际参与导入的文件, 返回自持诊断供文档操作组合.
    static SourceVerification Failure(const std::filesystem::path &path, SourceMatch match,
                                      std::string_view description)
    {
        const auto name = path.generic_u8string();
        Core::BoundedText text(std::string_view(reinterpret_cast<const char *>(name.data()), name.size()));
        text.Append(": ");
        text.Append(description);
        return {match, text};
    }

    /// @brief 以旧快照长度为读取上界, 变长可直接判为 Changed, 其它完整输入用摘要比较.
    static SourceVerification VerifyFile(const std::filesystem::path &root, const std::filesystem::path &source,
                                         Core::Sha256Digest fingerprint, std::size_t limit,
                                         std::span<const std::wstring_view> allowed, std::stop_token stop)
    {
        const auto path = Core::ResourcePath::Parse(source, allowed);
        if (const auto *error = std::get_if<Core::ResourceFailure>(&path))
        {
            return Failure(source, SourceMatch::Unavailable, error->description.View());
        }
        const auto read = Core::ReadResourceFile(root, std::get<Core::ResourcePath>(path), limit, stop);
        if (const auto *error = std::get_if<Core::ResourceFailure>(&read))
        {
            const auto match = error->reason == Core::ResourceError::ByteLimit ? SourceMatch::Changed :
                SourceMatch::Unavailable;
            return Failure(source, match, error->description.View());
        }
        if (std::get<Core::ResourceBytes>(read).fingerprint != fingerprint)
        {
            return Failure(source, SourceMatch::Changed, "Source bytes differ from the fixed version");
        }
        return {SourceMatch::Unchanged, {}};
    }

    SourceVerification VerifySource(const std::filesystem::path &root, const ModelRecord &model, std::stop_token stop)
    {
        if (model.inputFiles.empty())
        {
            return Failure(model.source, SourceMatch::Unavailable, "Model source evidence is missing");
        }
        constexpr std::array<std::wstring_view, 1> Allowed{L"Models"};
        for (const auto &file : model.inputFiles)
        {
            auto status = VerifyFile(root, file.source, file.fingerprint, file.byteCount, Allowed, stop);
            if (status.result != SourceMatch::Unchanged)
            {
                return status;
            }
        }
        return {SourceMatch::Unchanged, {}};
    }

    SourceVerification VerifySource(const std::filesystem::path &root, const ImageRecord &image, std::stop_token stop)
    {
        constexpr std::array<std::wstring_view, 2> Allowed{L"Textures", L"Editor"};
        return VerifyFile(root, image.source, image.sourceFingerprint, image.sourceByteCount, Allowed, stop);
    }

    SourceVerification VerifySource(const std::filesystem::path &root, const EnvironmentRecord &image,
                                    std::stop_token stop)
    {
        constexpr std::array<std::wstring_view, 1> Allowed{L"Environments"};
        return VerifyFile(root, image.source, image.sourceFingerprint, image.sourceByteCount, Allowed, stop);
    }
}
