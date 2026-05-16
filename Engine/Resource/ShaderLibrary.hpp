#pragma once

#include <filesystem>
#include <memory>
#include <unordered_map>

#include <Engine/Resource/Types/Shader.hpp>

namespace Physara::RHI
{
    class RHIDevice;
}

namespace Physara::Engine
{
    struct ShaderProgramDesc
    {
        std::string debugName{};
        std::filesystem::path vertexPath{};
        std::filesystem::path fragmentPath{};
        std::filesystem::path computePath{};
        ShaderFeatureMask featureMask{ShaderFeature::None};
        std::vector<ShaderDefine> defines{};
    };

    class ShaderLibrary final
    {
    public:
        ShaderLibrary() = default;
        explicit ShaderLibrary(RHI::RHIDevice *device);

        void SetDevice(RHI::RHIDevice *device);
        [[nodiscard]] ShaderVariant *GetVariant(const ShaderProgramDesc &desc);
        [[nodiscard]] ShaderVariant *FindVariant(const ShaderProgramDesc &desc);

        void InvalidateAll();
        void InvalidatePath(const std::filesystem::path &path);
        void ReloadChanged();

        [[nodiscard]] std::size_t GetCachedVariantCount() const { return m_Cache.size(); }

    private:
        [[nodiscard]] ShaderVariantKey BuildKey(const ShaderProgramDesc &desc) const;
        [[nodiscard]] std::unique_ptr<ShaderVariant> CompileVariant(const ShaderVariantKey &key);
        [[nodiscard]] bool IsVariantDirty(const ShaderVariant &variant) const;
        [[nodiscard]] static std::filesystem::file_time_type GetLastWriteTime(const std::filesystem::path &path);

    private:
        RHI::RHIDevice *m_Device{nullptr};
        std::unordered_map<ShaderVariantKey, std::unique_ptr<ShaderVariant>> m_Cache{};
        std::unordered_map<std::string, std::filesystem::file_time_type> m_DependencyWriteTimes{};
    };
}