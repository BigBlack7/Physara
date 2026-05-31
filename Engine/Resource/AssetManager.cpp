#include "AssetManager.hpp"

#include <utility>

#include <Engine/Resource/AssetPath.hpp>
#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Engine
{
    AssetManager::AssetManager(std::filesystem::path assetsRoot)
    {
        SetAssetsRoot(std::move(assetsRoot));
    }

    void AssetManager::SetAssetsRoot(std::filesystem::path assetsRoot)
    {
        m_AssetsRoot = assetsRoot.empty() ? std::filesystem::path{} : Platform::FileSystem::NormalizePath(assetsRoot.string());
    }

    std::string AssetManager::NormalizePath(const std::filesystem::path &path) const
    {
        return AssetPath::NormalizeAssetPath(path);
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
            if (record.referenceCount == 0 && record.resource != nullptr)
            {
                record.resource.reset();
                ++record.generation;
                if (record.generation == InvalidResourceGeneration)
                {
                    ++record.generation;
                }
            }
        }
    }

    void AssetManager::Clear()
    {
        m_PathToIndex.clear();
        m_IdToIndex.clear();
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
        m_IdToIndex.emplace(m_Records[index].id, index);
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

        const auto found = m_IdToIndex.find(handle.id);
        if (found == m_IdToIndex.end() || found->second >= m_Records.size())
        {
            return nullptr;
        }

        const AssetRecord &record = m_Records[found->second];
        return record.generation == handle.generation ? &record : nullptr;
    }
}