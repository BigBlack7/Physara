#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <Engine/Resource/Handle.hpp>

namespace Physara::Engine
{
    struct AssetRecordInfo
    {
        UntypedResourceHandle handle{};
        std::string normalizedPath{};
        std::string typeName{};
        std::uint32_t referenceCount{0};
        bool loaded{false};
    };

    class AssetManager final
    {
    public:
        AssetManager() = default;
        explicit AssetManager(std::filesystem::path assetsRoot);

        void SetAssetsRoot(std::filesystem::path assetsRoot);
        [[nodiscard]] const std::filesystem::path &GetAssetsRoot() const { return m_AssetsRoot; }

        [[nodiscard]] std::string NormalizePath(const std::filesystem::path &path) const;

        template <typename T>
        [[nodiscard]] ResourceHandle<T> GetHandle(const std::filesystem::path &path)
        {
            return Typed<T>(GetOrCreateRecord(typeid(T), NormalizePath(path)));
        }

        // 注册已加载的资源
        template <typename T>
        [[nodiscard]] ResourceHandle<T> RegisterAsset(const std::filesystem::path &path, std::shared_ptr<T> resource)
        {
            AssetRecord &record = GetOrCreateRecord(typeid(T), NormalizePath(path));
            record.resource = std::move(resource);
            return Typed<T>(record);
        }

        // 获取并增加引用计数-路径获取
        template <typename T>
        [[nodiscard]] ResourceHandle<T> Acquire(const std::filesystem::path &path)
        {
            AssetRecord &record = GetOrCreateRecord(typeid(T), NormalizePath(path));
            ++record.referenceCount;
            return Typed<T>(record);
        }

        // 获取并增加引用计数-句柄获取
        template <typename T>
        bool Acquire(ResourceHandle<T> handle)
        {
            AssetRecord *record = FindRecord(handle);
            if (record == nullptr || record->type != std::type_index(typeid(T)))
            {
                return false;
            }

            ++record->referenceCount;
            return true;
        }

        template <typename T>
        bool Release(ResourceHandle<T> handle)
        {
            AssetRecord *record = FindRecord(handle);
            if (record == nullptr || record->type != std::type_index(typeid(T)) || record->referenceCount == 0)
            {
                return false;
            }

            --record->referenceCount; // 释放引用计数
            return true;
        }

        template <typename T>
        [[nodiscard]] std::shared_ptr<T> Get(ResourceHandle<T> handle) const
        {
            const AssetRecord *record = FindRecord(handle);
            if (record == nullptr || record->type != std::type_index(typeid(T)) || !record->resource)
            {
                return {};
            }

            return std::static_pointer_cast<T>(record->resource);
        }

        template <typename T>
        [[nodiscard]] std::shared_ptr<T> GetByPath(const std::filesystem::path &path) const
        {
            const AssetKey key{std::type_index(typeid(T)), NormalizePath(path)};
            const auto it = m_PathToIndex.find(key);
            if (it == m_PathToIndex.end())
            {
                return {};
            }

            const AssetRecord &record = m_Records[it->second];
            return record.resource ? std::static_pointer_cast<T>(record.resource) : std::shared_ptr<T>{};
        }

        template <typename T>
        [[nodiscard]] bool IsAlive(ResourceHandle<T> handle) const
        {
            const AssetRecord *record = FindRecord(handle);
            return record != nullptr && record->type == std::type_index(typeid(T));
        }

        template <typename T>
        [[nodiscard]] std::uint32_t GetReferenceCount(ResourceHandle<T> handle) const
        {
            const AssetRecord *record = FindRecord(handle);
            return record != nullptr && record->type == std::type_index(typeid(T)) ? record->referenceCount : 0;
        }

        [[nodiscard]] std::vector<AssetRecordInfo> GetCachedAssets() const;
        void ClearUnusedResources();
        void Clear();

    private:
        struct AssetKey
        {
            std::type_index type{typeid(void)};
            std::string normalizedPath{};

            [[nodiscard]] bool operator==(const AssetKey &other) const
            {
                return type == other.type && normalizedPath == other.normalizedPath;
            }
        };

        struct AssetKeyHash
        {
            std::size_t operator()(const AssetKey &key) const noexcept;
        };

        struct AssetRecord
        {
            AssetKey key{};
            ResourceId id{InvalidResourceId};
            ResourceGeneration generation{1};
            std::uint32_t referenceCount{0};
            std::shared_ptr<void> resource{};
        };

        AssetRecord &GetOrCreateRecord(std::type_index type, std::string normalizedPath);
        [[nodiscard]] AssetRecord *FindRecord(UntypedResourceHandle handle);
        [[nodiscard]] const AssetRecord *FindRecord(UntypedResourceHandle handle) const;

        template <typename T>
        [[nodiscard]] static ResourceHandle<T> Typed(const AssetRecord &record)
        {
            return ResourceHandle<T>{record.id, record.generation};
        }

    private:
        std::filesystem::path m_AssetsRoot{};
        ResourceId m_NextId{1};
        std::unordered_map<AssetKey, std::size_t, AssetKeyHash> m_PathToIndex{};
        std::vector<AssetRecord> m_Records{};
    };
}