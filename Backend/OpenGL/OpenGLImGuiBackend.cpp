#include "OpenGLImGuiBackend.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>

#include <Backend/OpenGL/OpenGLCommandList.hpp>
#include <Backend/OpenGL/OpenGLTexture.hpp>
#include <Engine/Core/Log.hpp>
#include <Engine/RHI/Command/RHICommandList.hpp>
#include <Engine/RHI/Core/RHIDevice.hpp>

#include <glad/glad.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui.h>

namespace Physara::RHI
{
    OpenGLImGuiBackend::~OpenGLImGuiBackend()
    {
        Shutdown();
    }

    bool OpenGLImGuiBackend::Initialize(RHIDevice *device, void *windowHandle)
    {
        // Editor只依赖IImGuiBackend; 具体imgui_impl_glfw适配器和OpenGL UI渲染器封装在Backend内
        m_Device = device;

        if (m_Initialized)
        {
            PHYSARA_CORE_WARN("OpenGLImGuiBackend already initialized.");
            return true;
        }

        if (windowHandle == nullptr)
        {
            PHYSARA_CORE_ERROR("OpenGLImGuiBackend::Initialize failed: windowHandle is null.");
            return false;
        }

        if (ImGui::GetCurrentContext() == nullptr)
        {
            // 当前后端在没有外部ImGui context时负责创建并在Shutdown销毁
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            m_OwnsContext = true;
        }
        else
        {
            m_OwnsContext = false;
        }

        ImGuiIO &io = ImGui::GetIO();
        // Docking是编辑器工作台的基础能力; 键盘导航用于后续工具窗口和弹窗
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        if (!ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow *>(windowHandle), true))
        {
            PHYSARA_CORE_ERROR("ImGui_ImplGlfw_InitForOpenGL failed.");
            if (m_OwnsContext)
            {
                ImGui::DestroyContext();
                m_OwnsContext = false;
            }
            return false;
        }

        io.BackendRendererName = "Physara_OpenGLImGuiBackend";
        io.BackendRendererUserData = this;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
        GLint maxTextureSize = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        ImGuiPlatformIO &platformIO = ImGui::GetPlatformIO();
        platformIO.Renderer_TextureMaxWidth = maxTextureSize;
        platformIO.Renderer_TextureMaxHeight = maxTextureSize;
        platformIO.DrawCallback_ResetRenderState = ResetRenderStateCallback;

        if (!CreateDeviceObjects())
        {
            PHYSARA_CORE_ERROR("OpenGLImGuiBackend::CreateDeviceObjects failed.");
            DestroyDeviceObjects();
            io.BackendRendererName = nullptr;
            io.BackendRendererUserData = nullptr;
            io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
            platformIO.Renderer_TextureMaxWidth = 0;
            platformIO.Renderer_TextureMaxHeight = 0;
            platformIO.DrawCallback_ResetRenderState = nullptr;
            ImGui_ImplGlfw_Shutdown();
            if (m_OwnsContext)
            {
                ImGui::DestroyContext();
                m_OwnsContext = false;
            }
            return false;
        }

        m_Initialized = true;
        return true;
    }

    void OpenGLImGuiBackend::BeginFrame()
    {
        if (!m_Initialized)
        {
            return;
        }

        ImGui_ImplGlfw_NewFrame();
    }

    void OpenGLImGuiBackend::EndFrame()
    {
        if (!m_Initialized)
        {
            return;
        }

        ImGui::Render();
    }

    void OpenGLImGuiBackend::ResetRenderStateCallback(const ImDrawList *, const ImDrawCmd *)
    {
    }

    GLuint OpenGLImGuiBackend::CompileShader(GLenum type, const char *source)
    {
        const GLuint shader = glCreateShader(type);
        if (shader == 0)
        {
            return 0;
        }

        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint status = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status == GL_TRUE)
        {
            return shader;
        }

        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 1)
        {
            std::vector<char> log(static_cast<std::size_t>(logLength));
            glGetShaderInfoLog(shader, logLength, nullptr, log.data());
            PHYSARA_CORE_ERROR("OpenGLImGuiBackend shader compile failed: {}", log.data());
        }
        glDeleteShader(shader);
        return 0;
    }

    bool OpenGLImGuiBackend::CreateShaderProgram()
    {
        static constexpr const char *kVertexShaderSource = R"GLSL(
            #version 460 core
            layout(location = 0) in vec2 InPosition;
            layout(location = 1) in vec2 InUV;
            layout(location = 2) in vec4 InColor;
            layout(location = 0) out vec2 FragUV;
            layout(location = 1) out vec4 FragColor;
            uniform mat4 Projection;
            void main()
            {
                FragUV = InUV;
                FragColor = InColor;
                gl_Position = Projection * vec4(InPosition.xy, 0.0, 1.0);
            }
        )GLSL";

        static constexpr const char *kFragmentShaderSource = R"GLSL(
            #version 460 core
            layout(location = 0) in vec2 FragUV;
            layout(location = 1) in vec4 FragColor;
            layout(location = 0) out vec4 OutColor;
            uniform sampler2D Texture;
            void main()
            {
                OutColor = FragColor * texture(Texture, FragUV);
            }
        )GLSL";

        const GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, kVertexShaderSource);
        const GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSource);
        if (vertexShader == 0 || fragmentShader == 0)
        {
            if (vertexShader != 0)
            {
                glDeleteShader(vertexShader);
            }
            if (fragmentShader != 0)
            {
                glDeleteShader(fragmentShader);
            }
            return false;
        }

        const GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        GLint status = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status != GL_TRUE)
        {
            GLint logLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 1)
            {
                std::vector<char> log(static_cast<std::size_t>(logLength));
                glGetProgramInfoLog(program, logLength, nullptr, log.data());
                PHYSARA_CORE_ERROR("OpenGLImGuiBackend program link failed: {}", log.data());
            }
            glDeleteProgram(program);
            return false;
        }

        m_ShaderProgram = program;
        m_TextureLocation = glGetUniformLocation(program, "Texture");
        m_ProjectionLocation = glGetUniformLocation(program, "Projection");
        return m_TextureLocation >= 0 && m_ProjectionLocation >= 0;
    }

    bool OpenGLImGuiBackend::CreateDeviceObjects()
    {
        if (m_ShaderProgram != 0)
        {
            return true;
        }

        DestroyDeviceObjects();
        if (!CreateShaderProgram())
        {
            DestroyDeviceObjects();
            return false;
        }

        glCreateBuffers(1, &m_VertexBuffer);
        glCreateBuffers(1, &m_IndexBuffer);
        glCreateVertexArrays(1, &m_VertexArray);
        glVertexArrayVertexBuffer(m_VertexArray, 0, m_VertexBuffer, 0, sizeof(ImDrawVert));
        glVertexArrayElementBuffer(m_VertexArray, m_IndexBuffer);
        glEnableVertexArrayAttrib(m_VertexArray, 0);
        glEnableVertexArrayAttrib(m_VertexArray, 1);
        glEnableVertexArrayAttrib(m_VertexArray, 2);
        glVertexArrayAttribBinding(m_VertexArray, 0, 0);
        glVertexArrayAttribBinding(m_VertexArray, 1, 0);
        glVertexArrayAttribBinding(m_VertexArray, 2, 0);
        glVertexArrayAttribFormat(m_VertexArray, 0, 2, GL_FLOAT, GL_FALSE, offsetof(ImDrawVert, pos));
        glVertexArrayAttribFormat(m_VertexArray, 1, 2, GL_FLOAT, GL_FALSE, offsetof(ImDrawVert, uv));
        glVertexArrayAttribFormat(m_VertexArray, 2, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(ImDrawVert, col));
        glCreateSamplers(1, &m_LinearSampler);
        glSamplerParameteri(m_LinearSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glSamplerParameteri(m_LinearSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(m_LinearSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(m_LinearSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        if (m_VertexBuffer == 0 || m_IndexBuffer == 0 || m_VertexArray == 0 || m_LinearSampler == 0)
        {
            DestroyDeviceObjects();
            return false;
        }
        return true;
    }

    void OpenGLImGuiBackend::DestroyDeviceObjects()
    {
        if (ImGui::GetCurrentContext() != nullptr)
        {
            for (ImTextureData *texture : ImGui::GetPlatformIO().Textures)
            {
                if (texture != nullptr && texture->TexID != ImTextureID_Invalid)
                {
                    DestroyImGuiTexture(*texture);
                }
            }
        }
        if (m_LinearSampler != 0)
        {
            glDeleteSamplers(1, &m_LinearSampler);
            m_LinearSampler = 0;
        }
        if (m_VertexArray != 0)
        {
            glDeleteVertexArrays(1, &m_VertexArray);
            m_VertexArray = 0;
        }
        if (m_VertexBuffer != 0)
        {
            glDeleteBuffers(1, &m_VertexBuffer);
            m_VertexBuffer = 0;
        }
        if (m_IndexBuffer != 0)
        {
            glDeleteBuffers(1, &m_IndexBuffer);
            m_IndexBuffer = 0;
        }
        if (m_ShaderProgram != 0)
        {
            glDeleteProgram(m_ShaderProgram);
            m_ShaderProgram = 0;
        }
        m_TextureLocation = -1;
        m_ProjectionLocation = -1;
        m_BoundTexture = 0;
        m_VertexBufferCapacity = 0;
        m_IndexBufferCapacity = 0;
        m_VertexStaging.clear();
        m_IndexStaging.clear();
    }

    void OpenGLImGuiBackend::UpdateTextureRequests(ImDrawData &drawData)
    {
        if (drawData.Textures == nullptr)
        {
            return;
        }

        for (ImTextureData *texture : *drawData.Textures)
        {
            if (texture != nullptr && texture->Status != ImTextureStatus_OK)
            {
                UpdateImGuiTexture(*texture);
            }
        }
    }

    void OpenGLImGuiBackend::UpdateImGuiTexture(ImTextureData &texture)
    {
        if (texture.Status == ImTextureStatus_WantCreate)
        {
            if (texture.Format != ImTextureFormat_RGBA32 && texture.Format != ImTextureFormat_Alpha8)
            {
                PHYSARA_CORE_ERROR("OpenGLImGuiBackend unsupported ImGui texture format: {}", static_cast<int>(texture.Format));
                return;
            }

            const GLenum sourceFormat = texture.Format == ImTextureFormat_Alpha8 ? GL_RED : GL_RGBA;
            const GLenum internalFormat = texture.Format == ImTextureFormat_Alpha8 ? GL_R8 : GL_RGBA8;
            GLuint glTexture = 0;
            glCreateTextures(GL_TEXTURE_2D, 1, &glTexture);
            glTextureStorage2D(glTexture, 1, internalFormat, texture.Width, texture.Height);
            glTextureSubImage2D(glTexture, 0, 0, 0, texture.Width, texture.Height, sourceFormat, GL_UNSIGNED_BYTE, texture.GetPixels());
            glTextureParameteri(glTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(glTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(glTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(glTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            if (texture.Format == ImTextureFormat_Alpha8)
            {
                const GLint swizzle[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
                glTextureParameteriv(glTexture, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
            }
            texture.SetTexID(static_cast<ImTextureID>(glTexture));
            texture.SetStatus(ImTextureStatus_OK);
            return;
        }

        if (texture.Status == ImTextureStatus_WantUpdates)
        {
            const GLuint glTexture = static_cast<GLuint>(texture.TexID);
            const GLenum sourceFormat = texture.Format == ImTextureFormat_Alpha8 ? GL_RED : GL_RGBA;
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, texture.Width);
            for (const ImTextureRect &rect : texture.Updates)
            {
                glTextureSubImage2D(glTexture, 0, rect.x, rect.y, rect.w, rect.h,
                                    sourceFormat, GL_UNSIGNED_BYTE, texture.GetPixelsAt(rect.x, rect.y));
            }
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            texture.SetStatus(ImTextureStatus_OK);
            return;
        }

        if (texture.Status == ImTextureStatus_WantDestroy && texture.UnusedFrames > 0)
        {
            DestroyImGuiTexture(texture);
        }
    }

    void OpenGLImGuiBackend::DestroyImGuiTexture(ImTextureData &texture)
    {
        if (texture.TexID != ImTextureID_Invalid)
        {
            GLuint glTexture = static_cast<GLuint>(texture.TexID);
            glDeleteTextures(1, &glTexture);
        }
        texture.BackendUserData = nullptr;
        texture.SetTexID(ImTextureID_Invalid);
        texture.SetStatus(ImTextureStatus_Destroyed);
    }

    void OpenGLImGuiBackend::SetupRenderState(const ImDrawData &drawData, const DrawDimensions &dimensions)
    {
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_PRIMITIVE_RESTART);
        glEnable(GL_SCISSOR_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glViewport(0, 0, dimensions.framebufferWidth, dimensions.framebufferHeight);

        const float left = drawData.DisplayPos.x;
        const float right = drawData.DisplayPos.x + drawData.DisplaySize.x;
        const float top = drawData.DisplayPos.y;
        const float bottom = drawData.DisplayPos.y + drawData.DisplaySize.y;
        const float projection[4][4] = {
            {2.0f / (right - left), 0.0f, 0.0f, 0.0f},
            {0.0f, 2.0f / (top - bottom), 0.0f, 0.0f},
            {0.0f, 0.0f, -1.0f, 0.0f},
            {(right + left) / (left - right), (top + bottom) / (bottom - top), 0.0f, 1.0f}};

        glUseProgram(m_ShaderProgram);
        glUniform1i(m_TextureLocation, 0);
        glUniformMatrix4fv(m_ProjectionLocation, 1, GL_FALSE, &projection[0][0]);
        glBindSampler(0, m_LinearSampler);
        glBindVertexArray(m_VertexArray);
        m_BoundTexture = 0;
    }

    void OpenGLImGuiBackend::UploadDrawData(const ImDrawData &drawData)
    {
        const std::size_t vertexBytes = static_cast<std::size_t>(drawData.TotalVtxCount) * sizeof(ImDrawVert);
        const std::size_t indexBytes = static_cast<std::size_t>(drawData.TotalIdxCount) * sizeof(ImDrawIdx);
        m_VertexStaging.resize(vertexBytes);
        m_IndexStaging.resize(indexBytes);

        unsigned char *vertexDst = m_VertexStaging.data();
        unsigned char *indexDst = m_IndexStaging.data();
        for (int listIndex = 0; listIndex < drawData.CmdListsCount; ++listIndex)
        {
            const ImDrawList *drawList = drawData.CmdLists[listIndex];
            if (drawList == nullptr)
            {
                continue;
            }
            const std::size_t listVertexBytes = static_cast<std::size_t>(drawList->VtxBuffer.Size) * sizeof(ImDrawVert);
            const std::size_t listIndexBytes = static_cast<std::size_t>(drawList->IdxBuffer.Size) * sizeof(ImDrawIdx);
            if (listVertexBytes > 0)
            {
                std::memcpy(vertexDst, drawList->VtxBuffer.Data, listVertexBytes);
                vertexDst += listVertexBytes;
            }
            if (listIndexBytes > 0)
            {
                std::memcpy(indexDst, drawList->IdxBuffer.Data, listIndexBytes);
                indexDst += listIndexBytes;
            }
        }

        if (vertexBytes > m_VertexBufferCapacity)
        {
            m_VertexBufferCapacity = vertexBytes + vertexBytes / 2;
            glNamedBufferData(m_VertexBuffer, static_cast<GLsizeiptr>(m_VertexBufferCapacity), nullptr, GL_STREAM_DRAW);
        }
        if (indexBytes > m_IndexBufferCapacity)
        {
            m_IndexBufferCapacity = indexBytes + indexBytes / 2;
            glNamedBufferData(m_IndexBuffer, static_cast<GLsizeiptr>(m_IndexBufferCapacity), nullptr, GL_STREAM_DRAW);
        }
        if (vertexBytes > 0)
        {
            glNamedBufferSubData(m_VertexBuffer, 0, static_cast<GLsizeiptr>(vertexBytes), m_VertexStaging.data());
        }
        if (indexBytes > 0)
        {
            glNamedBufferSubData(m_IndexBuffer, 0, static_cast<GLsizeiptr>(indexBytes), m_IndexStaging.data());
        }
    }

    void OpenGLImGuiBackend::RenderCommandLists(const ImDrawData &drawData, const DrawDimensions &dimensions)
    {
        const ImVec2 clipOffset = drawData.DisplayPos;
        const ImVec2 clipScale = drawData.FramebufferScale;
        const GLenum indexType = sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
        std::size_t globalVertexOffset = 0;
        std::size_t globalIndexOffset = 0;
        int lastScissorX = -1;
        int lastScissorY = -1;
        int lastScissorWidth = -1;
        int lastScissorHeight = -1;

        for (int listIndex = 0; listIndex < drawData.CmdListsCount; ++listIndex)
        {
            const ImDrawList *drawList = drawData.CmdLists[listIndex];
            if (drawList == nullptr)
            {
                continue;
            }
            for (int commandIndex = 0; commandIndex < drawList->CmdBuffer.Size; ++commandIndex)
            {
                const ImDrawCmd *command = &drawList->CmdBuffer[commandIndex];
                if (command->UserCallback != nullptr)
                {
                    if (command->UserCallback == ImGui::GetPlatformIO().DrawCallback_ResetRenderState)
                    {
                        SetupRenderState(drawData, dimensions);
                    }
                    else
                    {
                        command->UserCallback(drawList, command);
                    }
                    continue;
                }

                const ImVec2 clipMin((command->ClipRect.x - clipOffset.x) * clipScale.x,
                                     (command->ClipRect.y - clipOffset.y) * clipScale.y);
                const ImVec2 clipMax((command->ClipRect.z - clipOffset.x) * clipScale.x,
                                     (command->ClipRect.w - clipOffset.y) * clipScale.y);
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                {
                    continue;
                }

                const int scissorX = static_cast<int>(clipMin.x);
                const int scissorY = static_cast<int>(static_cast<float>(dimensions.framebufferHeight) - clipMax.y);
                const int scissorWidth = static_cast<int>(clipMax.x - clipMin.x);
                const int scissorHeight = static_cast<int>(clipMax.y - clipMin.y);
                if (scissorX != lastScissorX || scissorY != lastScissorY ||
                    scissorWidth != lastScissorWidth || scissorHeight != lastScissorHeight)
                {
                    glScissor(scissorX, scissorY, scissorWidth, scissorHeight);
                    lastScissorX = scissorX;
                    lastScissorY = scissorY;
                    lastScissorWidth = scissorWidth;
                    lastScissorHeight = scissorHeight;
                }

                const GLuint texture = static_cast<GLuint>(command->GetTexID());
                if (texture == 0)
                {
                    continue;
                }
                if (texture != m_BoundTexture)
                {
                    glBindTextureUnit(0, texture);
                    m_BoundTexture = texture;
                }

                const std::size_t indexOffset = globalIndexOffset + command->IdxOffset;
                const GLint baseVertex = static_cast<GLint>(globalVertexOffset + command->VtxOffset);
                glDrawElementsBaseVertex(GL_TRIANGLES, static_cast<GLsizei>(command->ElemCount), indexType,
                                         reinterpret_cast<const void *>(indexOffset * sizeof(ImDrawIdx)), baseVertex);
            }
            globalVertexOffset += static_cast<std::size_t>(drawList->VtxBuffer.Size);
            globalIndexOffset += static_cast<std::size_t>(drawList->IdxBuffer.Size);
        }
    }

    void OpenGLImGuiBackend::RenderDrawData()
    {
        m_LastRenderStatistics.Reset();
        if (!m_Initialized)
        {
            return;
        }

        if (ImDrawData *drawData = ImGui::GetDrawData())
        {
            const auto start = std::chrono::steady_clock::now();
            m_LastRenderStatistics.drawLists = static_cast<std::uint32_t>(drawData->CmdListsCount);
            m_LastRenderStatistics.vertexCount = static_cast<std::uint32_t>(drawData->TotalVtxCount);
            m_LastRenderStatistics.indexCount = static_cast<std::uint32_t>(drawData->TotalIdxCount);
            for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex)
            {
                if (drawData->CmdLists[listIndex] != nullptr)
                {
                    m_LastRenderStatistics.drawCommands += static_cast<std::uint32_t>(drawData->CmdLists[listIndex]->CmdBuffer.Size);
                }
            }

            const bool debugGroup = GLAD_GL_VERSION_4_3 && glPushDebugGroup != nullptr && glPopDebugGroup != nullptr;
            if (debugGroup)
            {
                glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "EditorUI");
            }

            const DrawDimensions dimensions{
                static_cast<int>(drawData->DisplaySize.x * drawData->FramebufferScale.x),
                static_cast<int>(drawData->DisplaySize.y * drawData->FramebufferScale.y)};

            if (dimensions.framebufferWidth > 0 && dimensions.framebufferHeight > 0)
            {
                if (m_ShaderProgram == 0 && !CreateDeviceObjects())
                {
                    PHYSARA_CORE_ERROR("OpenGLImGuiBackend::CreateDeviceObjects failed during RenderDrawData.");
                }
                else
                {
                    UpdateTextureRequests(*drawData);
                    UploadDrawData(*drawData);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    SetupRenderState(*drawData, dimensions);
                    RenderCommandLists(*drawData, dimensions);
                }
            }

            if (debugGroup)
            {
                glPopDebugGroup();
            }

            if (m_Device != nullptr)
            {
                if (RHICommandList *commandList = m_Device->GetCommandList())
                {
                    if (auto *openGLCommandList = dynamic_cast<OpenGLCommandList *>(commandList))
                    {
                        openGLCommandList->InvalidateImGuiState();
                    }
                    else
                    {
                        commandList->InvalidateExternalState();
                    }
                }
            }

            const auto end = std::chrono::steady_clock::now();
            m_LastRenderStatistics.renderCpuMs = std::chrono::duration<float, std::milli>(end - start).count();
        }
    }

    ImGuiTextureHandle OpenGLImGuiBackend::CreateTextureRGBA(std::uint32_t width, std::uint32_t height, const void *pixels)
    {
        if (!m_Initialized || pixels == nullptr || width == 0 || height == 0)
        {
            return 0;
        }

        // 纹理创建和销毁接口由Backend实现, Editor只持有ImGuiTextureHandle, 通过Backend上传像素数据并获取对应的ImGuiTextureHandle, 后续UI渲染时将该handle传回ImGui作为纹理ID使用
        GLuint texture = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &texture);
        glTextureStorage2D(texture, 1, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        glTextureSubImage2D(texture, 0, 0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                            GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        return static_cast<ImGuiTextureHandle>(texture);
    }

    void OpenGLImGuiBackend::DestroyTexture(ImGuiTextureHandle texture)
    {
        if (texture == 0)
        {
            return;
        }

        const GLuint glTexture = static_cast<GLuint>(texture);
        glDeleteTextures(1, &glTexture);
    }

    ImGuiTextureHandle OpenGLImGuiBackend::GetTextureHandle(RHITexture *texture)
    {
        if (texture == nullptr)
        {
            return 0;
        }

        auto *glTexture = static_cast<OpenGLTexture *>(texture);
        return static_cast<ImGuiTextureHandle>(glTexture->GetGLID());
    }

    void OpenGLImGuiBackend::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        DestroyDeviceObjects();
        ImGuiIO &io = ImGui::GetIO();
        io.BackendRendererName = nullptr;
        io.BackendRendererUserData = nullptr;
        io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
        ImGuiPlatformIO &platformIO = ImGui::GetPlatformIO();
        platformIO.Renderer_TextureMaxWidth = 0;
        platformIO.Renderer_TextureMaxHeight = 0;
        platformIO.DrawCallback_ResetRenderState = nullptr;
        ImGui_ImplGlfw_Shutdown();

        if (m_OwnsContext && ImGui::GetCurrentContext() != nullptr)
        {
            ImGui::DestroyContext();
        }

        m_Initialized = false;
        m_OwnsContext = false;
        m_Device = nullptr;
        m_LastRenderStatistics.Reset();
    }
}