#include "ShaderLibrary.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <Engine/Core/Log.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>
#include <Engine/Resource/Loaders/ShaderLoader.hpp>
#include <Platform/FileSystem/FileSystem.hpp>

namespace Physara::Engine
{
    namespace ShaderLibraryDetail
    {
        std::string NormalizeAssetPath(const std::filesystem::path &path)
        {
            return Platform::FileSystem::NormalizeForCompare(Platform::FileSystem::ToAssetsRelativePath(path.string()));
        }

        bool ContainsDependency(const std::vector<std::string> &dependencies, const std::string &path)
        {
            return std::find(dependencies.begin(), dependencies.end(), path) != dependencies.end();
        }

        void AppendUniqueDependencies(std::vector<std::string> &target, const std::vector<std::string> &source)
        {
            for (const std::string &dependency : source)
            {
                if (!ContainsDependency(target, dependency))
                {
                    target.push_back(dependency);
                }
            }
        }
    }

    ShaderLibrary::ShaderLibrary(RHI::RHIDevice *device)
        : m_Device(device)
    {
    }

    void ShaderLibrary::SetDevice(RHI::RHIDevice *device)
    {
        if (m_Device == device)
        {
            return;
        }

        m_Device = device;
        InvalidateAll();
    }

    ShaderVariant *ShaderLibrary::GetVariant(const ShaderProgramDesc &desc)
    {
        ShaderVariantKey key = BuildKey(desc);
        const auto cached = m_Cache.find(key);
        if (cached != m_Cache.end())
        {
            return cached->second.get();
        }

        std::unique_ptr<ShaderVariant> variant = CompileVariant(key);
        if (variant == nullptr)
        {
            return nullptr;
        }

        ShaderVariant *result = variant.get();
        m_Cache.emplace(std::move(key), std::move(variant));
        return result;
    }

    ShaderVariant *ShaderLibrary::FindVariant(const ShaderProgramDesc &desc)
    {
        const ShaderVariantKey key = BuildKey(desc);
        const auto cached = m_Cache.find(key);
        return cached != m_Cache.end() ? cached->second.get() : nullptr;
    }

    void ShaderLibrary::InvalidateAll()
    {
        m_Cache.clear();
        m_DependencyWriteTimes.clear();
    }

    void ShaderLibrary::InvalidatePath(const std::filesystem::path &path)
    {
        const std::string normalizedPath = ShaderLibraryDetail::NormalizeAssetPath(path);
        for (auto it = m_Cache.begin(); it != m_Cache.end();)
        {
            if (ShaderLibraryDetail::ContainsDependency(it->second->dependencies, normalizedPath))
            {
                it = m_Cache.erase(it);
            }
            else
            {
                ++it;
            }
        }

        m_DependencyWriteTimes.erase(normalizedPath);
    }

    void ShaderLibrary::ReloadChanged()
    {
        for (auto it = m_Cache.begin(); it != m_Cache.end();)
        {
            if (IsVariantDirty(*it->second))
            {
                PHYSARA_CORE_INFO("Shader variant invalidated by changed dependency: {}", it->first.debugName);
                it = m_Cache.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    ShaderVariantKey ShaderLibrary::BuildKey(const ShaderProgramDesc &desc) const
    {
        ShaderVariantKey key{};
        key.debugName = desc.debugName;
        key.vertexPath = ShaderLibraryDetail::NormalizeAssetPath(desc.vertexPath);
        key.fragmentPath = ShaderLibraryDetail::NormalizeAssetPath(desc.fragmentPath);
        key.computePath = ShaderLibraryDetail::NormalizeAssetPath(desc.computePath);
        key.featureMask = desc.featureMask;
        key.defines = desc.defines;
        std::sort(key.defines.begin(), key.defines.end(), [](const ShaderDefine &lhs, const ShaderDefine &rhs)
                  {
                      if (lhs.name == rhs.name)
                      {
                          return lhs.value < rhs.value;
                      }
                      return lhs.name < rhs.name;
                  });
        return key;
    }

    std::unique_ptr<ShaderVariant> ShaderLibrary::CompileVariant(const ShaderVariantKey &key)
    {
        if (m_Device == nullptr)
        {
            PHYSARA_CORE_ERROR("ShaderLibrary cannot compile '{}' without an RHIDevice.", key.debugName);
            return nullptr;
        }

        const bool isCompute = !key.computePath.empty();
        if (isCompute && (!key.vertexPath.empty() || !key.fragmentPath.empty()))
        {
            PHYSARA_CORE_ERROR("Shader variant '{}' cannot mix compute and graphics stages.", key.debugName);
            return nullptr;
        }
        if (!isCompute && (key.vertexPath.empty() || key.fragmentPath.empty()))
        {
            PHYSARA_CORE_ERROR("Graphics shader variant '{}' requires vertex and fragment stages.", key.debugName);
            return nullptr;
        }

        auto variant = std::make_unique<ShaderVariant>();
        variant->key = key;

        try
        {
            if (isCompute)
            {
                ShaderSource source = ShaderLoader::Load({RHI::ShaderStage::Compute, key.computePath, key.featureMask, key.defines});
                variant->computeShader = m_Device->CreateShader(RHI::ShaderStage::Compute, source.source);
                ShaderLibraryDetail::AppendUniqueDependencies(variant->dependencies, source.dependencies);
            }
            else
            {
                ShaderSource vertexSource = ShaderLoader::Load({RHI::ShaderStage::Vertex, key.vertexPath, key.featureMask, key.defines});
                ShaderSource fragmentSource = ShaderLoader::Load({RHI::ShaderStage::Fragment, key.fragmentPath, key.featureMask, key.defines});

                variant->vertexShader = m_Device->CreateShader(RHI::ShaderStage::Vertex, vertexSource.source);
                variant->fragmentShader = m_Device->CreateShader(RHI::ShaderStage::Fragment, fragmentSource.source);
                ShaderLibraryDetail::AppendUniqueDependencies(variant->dependencies, vertexSource.dependencies);
                ShaderLibraryDetail::AppendUniqueDependencies(variant->dependencies, fragmentSource.dependencies);
            }
        }
        catch (const std::exception &error)
        {
            PHYSARA_CORE_ERROR("Failed to load shader variant '{}': {}", key.debugName, error.what());
            return nullptr;
        }

        if (!variant->IsValid())
        {
            PHYSARA_CORE_ERROR("Shader variant '{}' compiled to invalid shader stage.", key.debugName);
            return nullptr;
        }

        for (const std::string &dependency : variant->dependencies)
        {
            m_DependencyWriteTimes[dependency] = GetLastWriteTime(dependency);
        }

        PHYSARA_CORE_INFO("Compiled shader variant '{}'.", key.debugName);
        return variant;
    }

    bool ShaderLibrary::IsVariantDirty(const ShaderVariant &variant) const
    {
        for (const std::string &dependency : variant.dependencies)
        {
            const auto cachedTime = m_DependencyWriteTimes.find(dependency);
            if (cachedTime == m_DependencyWriteTimes.end() || cachedTime->second != GetLastWriteTime(dependency))
            {
                return true;
            }
        }

        return false;
    }

    std::filesystem::file_time_type ShaderLibrary::GetLastWriteTime(const std::filesystem::path &path)
    {
        const std::string resolvedPath = Platform::FileSystem::ResolvePath(ShaderLibraryDetail::NormalizeAssetPath(path));
        std::error_code error{};
        const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(resolvedPath, error);
        return error ? std::filesystem::file_time_type{} : writeTime;
    }
}