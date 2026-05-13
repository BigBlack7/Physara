#pragma once

#include <cstdint>
#include <memory>

#include <glm/vec4.hpp>

#include <Engine/Renderer/FrameData.hpp>
#include <Engine/RHI/Pipeline/RHIFramebuffer.hpp>
#include <Engine/RHI/Pipeline/RHIRenderPassDesc.hpp>
#include <Engine/RHI/Resource/RHITexture.hpp>

namespace Physara::RHI
{
    class RHIDevice;
}

namespace Physara::Engine
{
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

        void ResizeViewport(std::uint32_t width, std::uint32_t height);
        void BeginFrame(const RenderView &view, float deltaTimeSeconds = 0.f);
        void Render(const RenderView &view, float deltaTimeSeconds = 0.f);
        void RenderClear();

        void SetClearColor(const glm::vec4 &color) { m_ClearColor = color; }
        [[nodiscard]] const glm::vec4 &GetClearColor() const { return m_ClearColor; }

        [[nodiscard]] RHI::RHITexture *GetSceneColorTexture() const { return m_SceneColor.get(); }
        [[nodiscard]] std::uint32_t GetViewportWidth() const { return m_ViewportWidth; }
        [[nodiscard]] std::uint32_t GetViewportHeight() const { return m_ViewportHeight; }
        [[nodiscard]] const FrameData &GetFrameData() const { return m_FrameData; }
        [[nodiscard]] bool HasValidRenderTarget() const;

    private:
        void RecreateRenderTarget();

    private:
        RHI::RHIDevice *m_Device{nullptr};
        RHI::RHIRenderPassDesc m_RenderPassDesc{};
        std::unique_ptr<RHI::RHITexture> m_SceneColor{};
        std::unique_ptr<RHI::RHIFramebuffer> m_Framebuffer{};
        FrameData m_FrameData{};
        glm::vec4 m_ClearColor{0.09f, 0.12f, 0.11f, 1.f};
        std::uint64_t m_FrameIndex{0};
        std::uint32_t m_ViewportWidth{0};
        std::uint32_t m_ViewportHeight{0};
    };
}