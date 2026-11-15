#include "ContentHash.hpp"

/// @file
/// @brief 在一次摘要计算的作用域内管理 CNG provider、hash 和存储.

#include <limits>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>

namespace Physara::Core
{
    /// @brief CNG hash 借用存储, 须先销毁 handle 再释放存储.
    struct HashHandles final
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        // 显式 count 构造允许 MSVC Debug proxy 的分配异常向上传播, 避开默认构造的 noexcept.
        std::vector<unsigned char> storage = std::vector<unsigned char>(0);

        HashHandles() = default;
        HashHandles(const HashHandles &) = delete;
        HashHandles &operator=(const HashHandles &) = delete;

        ~HashHandles()
        {
            if (hash)
            {
                BCryptDestroyHash(hash);
            }
            if (algorithm)
            {
                BCryptCloseAlgorithmProvider(algorithm, 0);
            }
        }
    };

    std::variant<Sha256Digest, Sha256Error> ComputeSha256(std::span<const std::byte> bytes)
    {
        if (bytes.size() > std::numeric_limits<ULONG>::max())
        {
            return Sha256Error::InputTooLarge;
        }

        HashHandles handles;
        ULONG objectBytes = 0;
        ULONG written = 0;
        if (BCryptOpenAlgorithmProvider(&handles.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
            BCryptGetProperty(handles.algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes),
                              sizeof(objectBytes), &written, 0) < 0)
        {
            return Sha256Error::InitializationFailed;
        }

        // CNG 借用对象存储直至 hash 销毁, HashHandles 的析构体先释放句柄再析构成员.
        handles.storage.resize(objectBytes);
        Sha256Digest digest{};
        if (BCryptCreateHash(handles.algorithm, &handles.hash, handles.storage.data(), objectBytes,
                             nullptr, 0, 0) < 0 ||
            (!bytes.empty() && BCryptHashData(handles.hash,
                reinterpret_cast<PUCHAR>(const_cast<std::byte *>(bytes.data())),
                static_cast<ULONG>(bytes.size()), 0) < 0) ||
            BCryptFinishHash(handles.hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
        {
            return Sha256Error::ComputationFailed;
        }

        return digest;
    }
}
