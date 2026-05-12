#include "AssetManager.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <utility>

namespace Physara::Engine
{
    namespace Internal
    {
        std::string ToLower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
                           { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        std::string NormalizeGeneric(const std::filesystem::path &path)
        {
            return path.lexically_normal().generic_string();
        }

        bool StartsWithPath(std::string_view path, std::string_view prefix)
        {
            if (prefix.empty())
            {
                return false;
            }

            if (path == prefix)
            {
                return true;
            }

            const std::string prefixWithSlash = std::string(prefix) + "/";
            return path.rfind(prefixWithSlash, 0) == 0;
        }
    }

    AssetManager::AssetManager(std::filesystem::path assetsRoot)
    {
        SetAssetsRoot(std::move(assetsRoot));
    }

    void AssetManager::SetAssetsRoot(std::filesystem::path assetsRoot)
    {
        m_AssetsRoot = assetsRoot.lexically_normal();
    }

    std::string AssetManager::NormalizePath(const std::filesystem::path &path) const
    {
        std::filesystem::path normalized = path.lexically_normal();

        if (!m_AssetsRoot.empty())
        {
            const std::string root = Internal::NormalizeGeneric(m_AssetsRoot);
            const std::string candidate = Internal::NormalizeGeneric(normalized);

            if (Internal::StartsWithPath(candidate, root))
            {
                std::error_code error{};
                std::filesystem::path relative = std::filesystem::relative(normalized, m_AssetsRoot, error);
                if (!error && !relative.empty() && relative != ".")
                {
                    normalized = relative.lexically_normal();
                }
            }
            else if (!normalized.is_absolute())
            {
                const auto firstPart = normalized.begin();
                if (firstPart != normalized.end() && Internal::ToLower(firstPart->generic_string()) == "assets")
                {
                    std::filesystem::path stripped;
                    bool skipFirst = true;
                    for (const std::filesystem::path &part : normalized)
                    {
                        if (skipFirst)
                        {
                            skipFirst = false;
                            continue;
                        }
                        stripped /= part;
                    }
                    normalized = stripped.lexically_normal();
                }
            }
        }

        return Internal::ToLower(Internal::NormalizeGeneric(normalized));
    }

    std::vector<AssetRecordInfo> AssetManager::GetCachedAssets() const
    {
        std::vector<AssetRecordInfo> result;
        result.reserve(m_Records.size());

        for (const AssetRecord &record : m_Records)
        {
            result.push_back(AssetRecordInfo{
                UntypedResourceHandle{record.id, record.generation},
                record.key.normalizedPath,
                record.key.type.name(),
                record.referenceCount,
                record.resource != nullptr});
        }

        return result;
    }

    void AssetManager::ClearUnusedResources()
    {
        for (AssetRecord &record : m_Records)
        {
            if (record.referenceCount == 0)
            {
                record.resource.reset();
            }
        }
    }

    void AssetManager::Clear()
    {
        m_PathToIndex.clear();
        m_Records.clear();
        m_NextId = 1;
    }

    std::size_t AssetManager::AssetKeyHash::operator()(const AssetKey &key) const noexcept
    {
        const std::size_t typeHash = key.type.hash_code();
        const std::size_t pathHash = std::hash<std::string>{}(key.normalizedPath);
        return typeHash ^ (pathHash + 0x9e3779b97f4a7c15ull + (typeHash << 6u) + (typeHash >> 2u));
    }

    AssetManager::AssetRecord &AssetManager::GetOrCreateRecord(std::type_index type, std::string normalizedPath)
    {
        AssetKey key{type, std::move(normalizedPath)};
        const auto it = m_PathToIndex.find(key);
        if (it != m_PathToIndex.end())
        {
            return m_Records[it->second];
        }

        AssetRecord record{};
        record.key = std::move(key);
        record.id = m_NextId++;

        m_Records.push_back(std::move(record));
        const std::size_t index = m_Records.size() - 1u;
        m_PathToIndex.emplace(m_Records[index].key, index);
        return m_Records[index];
    }

    AssetManager::AssetRecord *AssetManager::FindRecord(UntypedResourceHandle handle)
    {
        const AssetManager *self = this;
        return const_cast<AssetRecord *>(self->FindRecord(handle));
    }

    const AssetManager::AssetRecord *AssetManager::FindRecord(UntypedResourceHandle handle) const
    {
        if (!handle)
        {
            return nullptr;
        }

        for (const AssetRecord &record : m_Records)
        {
            if (record.id == handle.id && record.generation == handle.generation)
            {
                return &record;
            }
        }

        return nullptr;
    }
}