#pragma once

#include <Engine/RHI/Core/IImGuiBackend.hpp>

#include <cstddef>
#include <vector>

#include <glad/glad.h>

struct ImDrawData;
struct ImDrawList;
struct ImDrawCmd;
struct ImTextureData;

namespace Physara::RHI
{
    class OpenGLCommandList;

    class OpenGLImGuiBackend final : public IImGuiBackend
    {
    public:
        OpenGLImGuiBackend() = default;
        ~OpenGLImGuiBackend() override;

        bool Initialize(RHIDevice *device, void *windowHandle) override;
        void BeginFrame() override;
        void EndFrame() override;
        void RenderDrawData() override;
        ImGuiTextureHandle CreateTextureRGBA(std::uint32_t width, std::uint32_t height, const void *pixels) override;
        void DestroyTexture(ImGuiTextureHandle texture) override;
        ImGuiTextureHandle GetTextureHandle(RHITexture *texture) override;
        [[nodiscard]] ImGuiRenderStatistics GetLastRenderStatistics() const override { return m_LastRenderStatistics; }
        void Shutdown() override;

    private:
        struct DrawDimensions
        {
            int framebufferWidth{0};
            int framebufferHeight{0};
        };

        bool CreateDeviceObjects();
        bool CreateShaderProgram();
        void DestroyDeviceObjects();
        void UpdateTextureRequests(ImDrawData &drawData);
        void UpdateImGuiTexture(ImTextureData &texture);
        void DestroyImGuiTexture(ImTextureData &texture);
        void SetupRenderState(const ImDrawData &drawData, const DrawDimensions &dimensions);
        void UploadDrawData(const ImDrawData &drawData);
        void RenderCommandLists(const ImDrawData &drawData, const DrawDimensions &dimensions);
        [[nodiscard]] OpenGLCommandList *GetOpenGLCommandList() const;
        static void ResetRenderStateCallback(const ImDrawList *, const ImDrawCmd *);
        static GLuint CompileShader(GLenum type, const char *source);

        RHIDevice *m_Device{nullptr};
        ImGuiRenderStatistics m_LastRenderStatistics{};
        GLuint m_ShaderProgram{0};
        GLint m_TextureLocation{-1};
        GLint m_ProjectionLocation{-1};
        GLuint m_VertexArray{0};
        GLuint m_VertexBuffer{0};
        GLuint m_IndexBuffer{0};
        GLuint m_LinearSampler{0};
        GLuint m_BoundTexture{0};
        std::size_t m_VertexBufferCapacity{0};
        std::size_t m_IndexBufferCapacity{0};
        std::vector<unsigned char> m_VertexStaging{};
        std::vector<unsigned char> m_IndexStaging{};
        bool m_Initialized{false};
        bool m_OwnsContext{false};
    };
}