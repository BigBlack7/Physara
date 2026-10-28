#include "Backend.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <memory>
#include <new>
#include <thread>
#include <utility>

#include "Submission.hpp"

namespace Physara::RHI::OpenGL
{
    OpenGLDevice::OpenGLDevice(const DeviceDesc &desc) noexcept : m_Desc(desc)
    {
    }

    OpenGLDevice::~OpenGLDevice()
    {
        if (m_Phase != Phase::Stopped || m_DeletionHead != nullptr || m_UploadCapacity != 0 ||
            m_InternallyRetainedWrappers != 0 || (m_Submissions && m_Submissions->count != 0) ||
            std::any_of(m_Packets.begin(), m_Packets.end(), [](const auto &packet) { return packet != nullptr; }))
        {
            // 原生 owner 必须先显式收拢; Stopped 后只剩 CPU 状态, 最后引用可跨线程释放.
            std::terminate();
        }
    }

    void OpenGLDevice::Report(const char *operation, const char *reason, const char *resource, GLenum nativeError,
                              std::source_location location) const noexcept
    {
        char message[1024]{};
        std::snprintf(message, sizeof(message), "OpenGL.%s: %s; resource=%s; native=0x%x [%s:%u]", operation, reason,
                      resource ? resource : "<none>", nativeError, location.file_name(), location.line());
        m_Desc.messageCallback->Message(MessageSeverity::Error, message);
    }

    bool OpenGLDevice::RequireContext(const char *operation, bool allowFaulted) const noexcept
    {
        if (std::this_thread::get_id() != m_OwnerThread)
        {
            Report(operation, "InvalidState: wrong owner thread");
            return false;
        }
        if (m_Phase == Phase::Stopped || (!allowFaulted && m_Phase != Phase::Active))
        {
            Report(operation, "InvalidState: device is stopped or faulted");
            return false;
        }
        if (!m_Desc.context.isCurrent(m_Desc.context.user))
        {
            Report(operation, "InvalidState: owning context is not current");
            return false;
        }
        if (!m_NativeUsable)
        {
            Report(operation, "InvalidState: context is unusable");
            return false;
        }
        return true;
    }

    void OpenGLDevice::NativeFailure(const char *operation, GLenum error, const char *resource,
                                     std::source_location location) noexcept
    {
        if (m_FirstNativeError == GL_NO_ERROR)
        {
            m_FirstNativeError = error;
        }
        m_Phase = Phase::Faulted;
        m_WorkFailed = true;
        if (error == GL_CONTEXT_LOST)
        {
            m_NativeUsable = false;
        }
        else if (glGetGraphicsResetStatus != nullptr && glGetGraphicsResetStatus() != GL_NO_ERROR)
        {
            m_NativeUsable = false;
        }
        Report(operation, "NativeFailure", resource, error, location);
    }

    bool OpenGLDevice::CheckNative(const char *operation, GLuint object, const char *label,
                                   std::source_location location) noexcept
    {
        const auto error = glGetError();
        if (error != GL_NO_ERROR)
        {
            char resource[256]{};
            std::snprintf(resource, sizeof(resource), "native-name=%u label=%s", object, label ? label : "<unnamed>");
            NativeFailure(operation, error, resource, location);
            return false;
        }
        return true;
    }

    bool OpenGLDevice::BeginNative(const char *operation, const char *label, std::source_location location) noexcept
    {
        return RequireContext(operation) && CheckNative(operation, 0, label, location);
    }

    bool OpenGLDevice::Initialize()
    {
        // 列出实际使用的入口; 版本标志存在不证明驱动返回了所有函数地址.
        struct Entry
        {
            bool available;
            const char *name;
        };
        const Entry entries[] = {{glGetError != nullptr, "glGetError"},
                                 {glGetIntegerv != nullptr, "glGetIntegerv"},
                                 {glGetFloatv != nullptr, "glGetFloatv"},
                                 {glGetInternalformativ != nullptr, "glGetInternalformativ"},
                                 {glGetGraphicsResetStatus != nullptr, "glGetGraphicsResetStatus"},
                                 {glClipControl != nullptr, "glClipControl"},
                                 {glCreateBuffers != nullptr, "glCreateBuffers"},
                                 {glNamedBufferStorage != nullptr, "glNamedBufferStorage"},
                                 {glDeleteBuffers != nullptr, "glDeleteBuffers"},
                                 {glCreateTextures != nullptr, "glCreateTextures"},
                                 {glTextureStorage1D != nullptr, "glTextureStorage1D"},
                                 {glTextureStorage2D != nullptr, "glTextureStorage2D"},
                                 {glTextureStorage3D != nullptr, "glTextureStorage3D"},
                                 {glTextureStorage2DMultisample != nullptr, "glTextureStorage2DMultisample"},
                                 {glTextureStorage3DMultisample != nullptr, "glTextureStorage3DMultisample"},
                                 {glTextureParameteri != nullptr, "glTextureParameteri"},
                                 {glTextureParameteriv != nullptr, "glTextureParameteriv"},
                                 {glDeleteTextures != nullptr, "glDeleteTextures"},
                                 {glGenTextures != nullptr, "glGenTextures"},
                                 {glTextureView != nullptr, "glTextureView"},
                                 {glCreateSamplers != nullptr, "glCreateSamplers"},
                                 {glSamplerParameteri != nullptr, "glSamplerParameteri"},
                                 {glSamplerParameterf != nullptr, "glSamplerParameterf"},
                                 {glSamplerParameterfv != nullptr, "glSamplerParameterfv"},
                                 {glDeleteSamplers != nullptr, "glDeleteSamplers"},
                                 {glCreateFramebuffers != nullptr, "glCreateFramebuffers"},
                                 {glNamedFramebufferTexture != nullptr, "glNamedFramebufferTexture"},
                                 {glNamedFramebufferTextureLayer != nullptr, "glNamedFramebufferTextureLayer"},
                                 {glNamedFramebufferDrawBuffer != nullptr, "glNamedFramebufferDrawBuffer"},
                                 {glNamedFramebufferDrawBuffers != nullptr, "glNamedFramebufferDrawBuffers"},
                                 {glNamedFramebufferReadBuffer != nullptr, "glNamedFramebufferReadBuffer"},
                                 {glCheckNamedFramebufferStatus != nullptr, "glCheckNamedFramebufferStatus"},
                                 {glDeleteFramebuffers != nullptr, "glDeleteFramebuffers"},
                                 {glNamedBufferSubData != nullptr, "glNamedBufferSubData"},
                                 {glCopyNamedBufferSubData != nullptr, "glCopyNamedBufferSubData"},
                                 {glMapNamedBufferRange != nullptr, "glMapNamedBufferRange"},
                                 {glUnmapNamedBuffer != nullptr, "glUnmapNamedBuffer"},
                                 {glBindBuffer != nullptr, "glBindBuffer"},
                                 {glPixelStorei != nullptr, "glPixelStorei"},
                                 {glTextureSubImage1D != nullptr, "glTextureSubImage1D"},
                                 {glTextureSubImage2D != nullptr, "glTextureSubImage2D"},
                                 {glTextureSubImage3D != nullptr, "glTextureSubImage3D"},
                                 {glFenceSync != nullptr, "glFenceSync"},
                                 {glClientWaitSync != nullptr, "glClientWaitSync"},
                                 {glDeleteSync != nullptr, "glDeleteSync"},
                                 {glIsSync != nullptr, "glIsSync"},
                                 {glFlush != nullptr, "glFlush"},
                                 {glFinish != nullptr, "glFinish"}};
        for (const auto &entry : entries)
        {
            if (!entry.available)
            {
                Report("CreateDevice", "Unsupported: missing required entry", entry.name);
                return false;
            }
        }
        if (!BeginNative("CreateDevice"))
        {
            return false;
        }
        m_Submissions = std::make_unique<SubmissionState>();
        glGetIntegerv(GL_MIN_MAP_BUFFER_ALIGNMENT, &m_Caps.minMapAlignment);
        if (!CheckNative("CreateDevice.MapAlignment") || m_Caps.minMapAlignment <= 0 ||
            (m_Caps.minMapAlignment & (m_Caps.minMapAlignment - 1)) != 0)
        {
            Report("CreateDevice", "Unsupported: invalid minimum map alignment");
            return false;
        }
        GLint major = 0;
        GLint minor = 0;
        GLint profile = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
        if (!CheckNative("CreateDevice.Version"))
        {
            return false;
        }
        if (major < 4 || minor < 0 || (major == 4 && minor < 6) || (profile & GL_CONTEXT_CORE_PROFILE_BIT) == 0 ||
            (profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) != 0)
        {
            Report("CreateDevice", "Unsupported: OpenGL 4.6 Core is required");
            return false;
        }
        if (!BuildCaps(*this, m_Caps))
        {
            return false;
        }
        glClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
        GLint origin = 0;
        GLint depth = 0;
        glGetIntegerv(GL_CLIP_ORIGIN, &origin);
        glGetIntegerv(GL_CLIP_DEPTH_MODE, &depth);
        if (!CheckNative("CreateDevice.ClipControl"))
        {
            return false;
        }
        if (origin != GL_UPPER_LEFT || depth != GL_ZERO_TO_ONE)
        {
            Report("CreateDevice", "NativeFailure: clip convention was not established");
            return false;
        }
        return true;
    }

    bool OpenGLDevice::DeleteNative(NativeDeletion &record) noexcept
    {
        if (!m_NativeUsable)
        {
            return false;
        }
        if (record.name != 0)
        {
            const GLuint name = record.name;
            switch (record.kind)
            {
            case NativeKind::Buffer:
                glDeleteBuffers(1, &record.name);
                break;
            case NativeKind::Texture:
                glDeleteTextures(1, &record.name);
                break;
            case NativeKind::Sampler:
                glDeleteSamplers(1, &record.name);
                break;
            case NativeKind::Framebuffer:
                glDeleteFramebuffers(1, &record.name);
                break;
            }
            record.name = 0;
            constexpr const char *Kinds[] = {"Buffer", "Texture", "Sampler", "Framebuffer"};
            if (!CheckNative("DeleteNative", name, Kinds[std::size_t(record.kind)]))
            {
                return false;
            }
        }
        // FBO 已解除原生附件引用, 才撤销该 FBO 自己拥有的 view.
        for (std::uint32_t index = 0; index < record.viewCount && m_NativeUsable; ++index)
        {
            glDeleteTextures(1, &record.views[index]);
            if (!CheckNative("DeleteNative.View", record.views[index]))
            {
                return false;
            }
        }
        record.viewCount = 0;
        return true;
    }

    void OpenGLDevice::ReleaseAllocation(std::unique_ptr<NativeDeletion> record, bool published) noexcept
    {
        if (!record || !record->HasObjects())
        {
            return;
        }
        if (!published && std::this_thread::get_id() == m_OwnerThread && m_NativeUsable &&
            m_Desc.context.isCurrent(m_Desc.context.user))
        {
            static_cast<void>(DeleteNative(*record));
            return;
        }
        // 节点已在创建前分配. 最后一个 CPU Release 只转移所有权, 不调用 GL.
        try
        {
            std::lock_guard lock(m_DeletionMutex);
            auto *node = record.release();
            if (m_DeletionTail != nullptr)
            {
                m_DeletionTail->next = node;
            }
            else
            {
                m_DeletionHead = node;
            }
            m_DeletionTail = node;
        }
        catch (...)
        {
            std::terminate();
        }
    }

    void OpenGLDevice::Drain(bool nativeAvailable) noexcept
    {
        for (;;)
        {
            NativeDeletion *chain = nullptr;
            try
            {
                std::lock_guard lock(m_DeletionMutex);
                chain = m_DeletionHead;
                m_DeletionHead = nullptr;
                m_DeletionTail = nullptr;
            }
            catch (...)
            {
                std::terminate();
            }
            if (chain == nullptr)
            {
                return;
            }
            // 不递归析构链, 不持锁调用驱动或诊断目标.
            while (chain != nullptr)
            {
                std::unique_ptr<NativeDeletion> record(chain);
                chain = record->next;
                if (nativeAvailable && m_NativeUsable)
                {
                    nativeAvailable = DeleteNative(*record);
                }
            }
        }
    }



    DeviceHandle CreateDevice(const DeviceDesc &desc, std::uint32_t *nativeError)
    {
        if (nativeError != nullptr)
        {
            *nativeError = 0;
        }
        if (desc.messageCallback == nullptr || desc.context.user == nullptr || desc.context.isCurrent == nullptr)
        {
            if (desc.messageCallback != nullptr)
            {
                desc.messageCallback->Message(MessageSeverity::Error,
                                              "OpenGL.CreateDevice: InvalidArgument: missing context access");
            }
            return nullptr;
        }
        if (!desc.context.isCurrent(desc.context.user))
        {
            desc.messageCallback->Message(MessageSeverity::Error,
                                          "OpenGL.CreateDevice: InvalidState: owning context is not current");
            return nullptr;
        }
        try
        {
            const auto deleter = [](OpenGLDevice *device)
            {
                // 工厂失败还未发布任何资源, 外部 owner 随后负责原生环境退出.
                static_cast<void>(device->Shutdown(ShutdownMode::ContextUnavailable, nullptr));
                delete device;
            };
            std::unique_ptr<OpenGLDevice, decltype(deleter)> device(new OpenGLDevice(desc), deleter);
            if (!device->Initialize())
            {
                if (nativeError != nullptr)
                {
                    *nativeError = device->GetFirstNativeError();
                }
                return nullptr;
            }
            return DeviceHandle::Create(device.release());
        }
        catch (const std::bad_alloc &)
        {
            desc.messageCallback->Message(MessageSeverity::Error, "OpenGL.CreateDevice: CPU allocation failed");
            return nullptr;
        }
    }

    bool ShutdownDevice(RHI::Device *device, ShutdownMode mode, std::uint32_t *nativeError)
    {
        if (nativeError != nullptr)
        {
            *nativeError = 0;
        }
        auto *native = dynamic_cast<OpenGLDevice *>(device);
        return native != nullptr && native->Shutdown(mode, nativeError);
    }

    GraphicsAPI OpenGLDevice::GetGraphicsAPI()
    {
        return GraphicsAPI::OpenGL;
    }

    MessageCallback *OpenGLDevice::GetMessageCallback()
    {
        return m_Desc.messageCallback;
    }

    bool OpenGLDevice::QueryFeatureSupport(Feature, void *, size_t)
    {
        static_cast<void>(RequireContext("QueryFeatureSupport"));
        return false;
    }

    FormatSupport OpenGLDevice::QueryFormatSupport(Format format)
    {
        if (!RequireContext("QueryFormatSupport"))
        {
            return FormatSupport::None;
        }
        return GetFormatCaps(format, TextureDimension::Texture2D).support;
    }

    const FormatCaps &OpenGLDevice::GetFormatCaps(Format format, TextureDimension dimension) const noexcept
    {
        static const FormatCaps Unsupported;
        if (format <= Format::UNKNOWN || format >= Format::COUNT || dimension <= TextureDimension::Unknown ||
            dimension > TextureDimension::Texture3D)
        {
            return Unsupported;
        }
        return m_Caps.formats[std::size_t(format)][std::size_t(dimension)];
    }


    bool ValidInitialState(ResourceStates state, bool keepInitialState) noexcept
    {
        // 与通用 GL profile 一致保留普通状态元数据, 不把它解释为当前已交付的执行能力.
        const auto Allowed = ResourceStates::Common | ResourceStates::ConstantBuffer | ResourceStates::VertexBuffer |
                             ResourceStates::IndexBuffer | ResourceStates::IndirectArgument |
                             ResourceStates::ShaderResource | ResourceStates::UnorderedAccess |
                             ResourceStates::RenderTarget | ResourceStates::DepthWrite | ResourceStates::DepthRead |
                             ResourceStates::StreamOut | ResourceStates::CopyDest | ResourceStates::CopySource |
                             ResourceStates::ResolveDest | ResourceStates::ResolveSource | ResourceStates::Present;
        return (state & ~Allowed) == ResourceStates::Unknown && (!keepInitialState || state != ResourceStates::Unknown);
    }
}
