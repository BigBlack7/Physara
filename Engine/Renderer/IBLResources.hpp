#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <mutex>
#include <utility>

#include <glm/vec4.hpp>

#include <Engine/Renderer/Precompute/IBLPrecompute.hpp>
#include <Engine/RHI/Resource/RHITexture.hpp>

namespace Physara::RHI
{
    class RHIDevice;
}

namespace Physara::Engine
{
    class IBLResources final
    {
    public:
        void Reset();
        void Invalidate();
        bool Ensure(RHI::RHIDevice *device, const std::filesystem::path &environmentPath);

        [[nodiscard]] bool IsReady() const { return m_Ready; }
        [[nodiscard]] std::uint32_t GetSpecularMipCount() const { return m_SpecularMipCount; }
        [[nodiscard]] const std::array<glm::vec4, 9> &GetIrradianceSH() const { return m_IrradianceSH; }
        [[nodiscard]] RHI::RHITexture *GetSpecularTexture() const { return m_SpecularTexture.get(); }
        [[nodiscard]] RHI::RHITexture *GetBRDFLut() const { return m_BRDFLut.get(); }

    private:
        struct PendingPrecompute
        {
            explicit PendingPrecompute(std::filesystem::path requestedPath)
                : path(std::move(requestedPath))
            {
            }

            std::filesystem::path path{};
            std::mutex mutex{};
            std::shared_ptr<IBLPrecomputeResult> previewResult{};
            std::shared_ptr<IBLPrecomputeResult> finalResult{};
            bool previewFinished{false};
            bool finalFinished{false};
        };

        bool Upload(RHI::RHIDevice *device, const IBLPrecomputeResult &result);
        void ReleaseGPUResources();
        void StartPrecompute(const std::filesystem::path &environmentPath);

    private:
        std::filesystem::path m_LoadedEnvironmentPath{};
        std::shared_ptr<PendingPrecompute> m_PendingPrecompute{};
        bool m_UsingPreview{false};
        std::unique_ptr<RHI::RHITexture> m_SpecularTexture{};
        std::unique_ptr<RHI::RHITexture> m_BRDFLut{};
        std::array<glm::vec4, 9> m_IrradianceSH{};
        std::uint32_t m_SpecularMipCount{0};
        bool m_Ready{false};
    };
}