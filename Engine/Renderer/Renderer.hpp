#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

#include <glm/vec4.hpp>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/Renderer/Passes/ForwardOpaquePass.hpp>
#include <Engine/Renderer/Passes/ForwardTransparentPass.hpp>
#include <Engine/Renderer/Passes/PostProcessPass.hpp>
#include <Engine/Renderer/Passes/SkyboxPass.hpp>
#include <Engine/Renderer/PipelineStateCache.hpp>
#include <Engine/Renderer/RenderProxy.hpp>
#include <Engine/Renderer/RenderGraph/RenderGraph.hpp>
#include <Engine/Renderer/RendererCapture.hpp>
#include <Engine/Resource/ShaderLibrary.hpp>
#include <Engine/RHI/Pipeline/RHIFramebuffer.hpp>
#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>
#include <Engine/RHI/Resource/RHITexture.hpp>

namespace Physara::RHI
{
    class RHIDevice;
}

namespace Physara::Engine
{
    class AssetManager;
    class Scene;

    class Renderer final
    {
    public:
        Renderer() = default;
        explicit Renderer(RHI::RHIDevice *device);
        ~Renderer();

        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;

        void Initialize(RHI::RHIDevice *device);
        void Shutdown();
        void SetAssetManager(AssetManager *assetManager) { m_AssetManager = assetManager; }

        void ResizeViewport(std::uint32_t width, std::uint32_t height);
        void BeginFrame(const RenderView &view, float deltaTimeSeconds = 0.f);
        void Render(const RenderView &view, float deltaTimeSeconds = 0.f);
        void RenderScene(Scene &scene, const RenderView &view, float deltaTimeSeconds = 0.f);
        void RenderClear();
        CaptureResult CaptureCurrentView(const CaptureDesc &desc);
        void RequestCapture(const CaptureDesc &desc);

        void SetClearColor(const glm::vec4 &color) { m_ClearColor = color; }
        [[nodiscard]] const glm::vec4 &GetClearColor() const { return m_ClearColor; }
        void SetEnvironmentMapPath(std::filesystem::path path);
        void SetSkyboxEnabled(bool enabled) { m_SkyboxEnabled = enabled; }
        void SetSkyboxExposureCompensation(float ev) { m_SkyboxExposureCompensation = ev; }
        void SetPostProcessSettings(const PostProcessSettings &settings) { m_PostProcessSettings = settings; }
        [[nodiscard]] const std::filesystem::path &GetEnvironmentMapPath() const { return m_EnvironmentMapPath; }

        [[nodiscard]] RHI::RHITexture *GetSceneColorTexture() const { return m_SceneColor.get(); }
        [[nodiscard]] std::uint32_t GetViewportWidth() const { return m_ViewportWidth; }
        [[nodiscard]] std::uint32_t GetViewportHeight() const { return m_ViewportHeight; }
        [[nodiscard]] const FrameData &GetFrameData() const { return m_FrameData; }
        [[nodiscard]] const RenderProxy &GetRenderProxy() const { return m_RenderProxy; }
        [[nodiscard]] bool HasValidRenderTarget() const;

    private:
        void RecreateRenderTarget();
        void BuildRenderGraph();
        void ExecuteForwardTransparentPass(RenderGraphContext &context);
        void ProcessPendingCapture();

    private:
        RHI::RHIDevice *m_Device{nullptr};
        AssetManager *m_AssetManager{nullptr};
        RHI::RHIRenderPassDesc m_RenderPassDesc{};
        RHI::RHIRenderPassDesc m_SkyboxRenderPassDesc{};
        RHI::RHIRenderPassDesc m_FinalRenderPassDesc{};
        std::unique_ptr<RHI::RHITexture> m_SceneHDRColor{};
        std::unique_ptr<RHI::RHITexture> m_SceneColor{};
        std::unique_ptr<RHI::RHITexture> m_SceneDepth{};
        std::unique_ptr<RHI::RHIFramebuffer> m_Framebuffer{};
        std::unique_ptr<RHI::RHIFramebuffer> m_FinalFramebuffer{};
        RenderGraph m_RenderGraph{};
        RenderProxy m_RenderProxy{};
        ShaderLibrary m_ShaderLibrary{};
        PipelineStateCache m_PipelineStateCache{};
        ForwardOpaquePass m_ForwardOpaquePass{};
        ForwardTransparentPass m_ForwardTransparentPass{};
        SkyboxPass m_SkyboxPass{};
        PostProcessPass m_PostProcessPass{};
        FrameData m_FrameData{};
        std::optional<CaptureDesc> m_PendingCapture{};
        glm::vec4 m_ClearColor{0.09f, 0.12f, 0.11f, 1.f};
        std::filesystem::path m_EnvironmentMapPath{};
        PostProcessSettings m_PostProcessSettings{};
        float m_SkyboxExposureCompensation{0.f};
        bool m_SkyboxEnabled{true};
        std::uint64_t m_FrameIndex{0};
        std::uint32_t m_ViewportWidth{0};
        std::uint32_t m_ViewportHeight{0};
    };
}