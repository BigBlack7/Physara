#include "DeviceHostInternal.hpp"

#include <array>
#include <cstring>

#include <glad/gl.h>

namespace Physara::Platform::Detail
{
    static GLADapiproc ResolveGlEntry(void *user, const char *name)
    {
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        const auto &checks = static_cast<DeviceHostState *>(user)->checks;
        if ((checks.missingEntry == MissingEntry::VersionString && std::strcmp(name, "glGetString") == 0) ||
            (checks.missingEntry == MissingEntry::BooleanQuery && std::strcmp(name, "glGetBooleanv") == 0) ||
            (checks.missingEntry == MissingEntry::BufferStorage && std::strcmp(name, "glNamedBufferStorage") == 0))
        {
            return nullptr;
        }
#else
        static_cast<void>(user);
#endif
        return glfwGetProcAddress(name);
    }

    static HostError GlQueryError(const DeviceHostState &state, GLenum code, std::string_view description) noexcept
    {
        auto error = MakeError(&state, HostErrorReason::EnvironmentQuery, state.stage, description);
        error.context.nativeCode =
            Physara::Core::NativeErrorCode{Physara::Core::NativeErrorDomain::OpenGL, static_cast<std::int64_t>(code)};
        return error;
    }

    static bool ReadGlString(GLenum name, Physara::Core::BoundedText &destination) noexcept
    {
        const auto *text = glGetString(name);
        if (text == nullptr || text[0] == 0)
        {
            return false;
        }
        destination.Assign(reinterpret_cast<const char *>(text));
        return true;
    }

    static std::optional<HostError> ReadGlfwEnvironment(DeviceHostState &state)
    {
        BeginNativeOperation(state);
        int major = 0;
        int minor = 0;
        int revision = 0;
        glfwGetVersion(&major, &minor, &revision);
        const auto platform = glfwGetPlatform();
        if (auto error = TakeNativeError(state, HostErrorReason::EnvironmentQuery, state.stage))
        {
            return error;
        }
        if (platform != GLFW_PLATFORM_WIN32 || major < 0 || minor < 0 || revision < 0)
        {
            return MakeError(&state, HostErrorReason::UnsupportedContext, state.stage,
                             "This host requires the retained Win32 GLFW platform");
        }
        state.info.platform = HostPlatform::Win32;
        state.info.glfwMajor = static_cast<std::uint32_t>(major);
        state.info.glfwMinor = static_cast<std::uint32_t>(minor);
        state.info.glfwRevision = static_cast<std::uint32_t>(revision);
        return std::nullopt;
    }

    struct GlObservations
    {
        GLint major = 0;
        GLint minor = 0;
        GLint profile = 0;
        GLint flags = 0;
        GLint drawFramebuffer = 0;
        GLint encoding = 0;
        GLboolean doubleBuffered = GL_FALSE;
        std::array<GLint, 6> limits{};
    };

    static std::optional<HostError> ValidateObservations(DeviceHostState &state, const GlObservations &observed)
    {
        if (!(observed.major > 4 || (observed.major == 4 && observed.minor >= 6)) ||
            (observed.profile & GL_CONTEXT_CORE_PROFILE_BIT) == 0 ||
            (observed.profile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) != 0 || observed.doubleBuffered != GL_TRUE)
        {
            return MakeError(&state, HostErrorReason::UnsupportedContext, state.stage,
                             "Actual context must be OpenGL 4.6+ Core with a double-buffered default framebuffer");
        }
        if (observed.drawFramebuffer != 0 || observed.flags < 0 || observed.minor < 0 ||
            (observed.encoding != GL_SRGB && observed.encoding != GL_LINEAR))
        {
            return MakeError(&state, HostErrorReason::EnvironmentQuery, state.stage,
                             "Unexpected default framebuffer, context flags or color encoding");
        }
        for (const auto value : observed.limits)
        {
            if (value <= 0)
            {
                return MakeError(&state, HostErrorReason::EnvironmentQuery, state.stage,
                                 "Required startup limit is not a positive representable value");
            }
        }
        auto &info = state.info;
        info.glMajor = static_cast<std::uint32_t>(observed.major);
        info.glMinor = static_cast<std::uint32_t>(observed.minor);
        info.contextFlags = static_cast<std::uint32_t>(observed.flags);
        info.coreProfile = true;
        info.debugContext = (observed.flags & GL_CONTEXT_FLAG_DEBUG_BIT) != 0;
        info.doubleBuffered = true;
        info.defaultFramebufferEncoding =  observed.encoding == GL_SRGB ? FramebufferEncoding::SRGB : FramebufferEncoding::Linear;
        info.maxTextureSizePixels = static_cast<std::uint32_t>(observed.limits[0]);
        info.maxRenderbufferSizePixels = static_cast<std::uint32_t>(observed.limits[1]);
        info.maxUniformBufferBindings = static_cast<std::uint32_t>(observed.limits[2]);
        info.maxStorageBufferBindings = static_cast<std::uint32_t>(observed.limits[3]);
        info.maxCombinedTextureUnits = static_cast<std::uint32_t>(observed.limits[4]);
        info.uniformBufferOffsetAlignmentBytes = static_cast<std::uint32_t>(observed.limits[5]);
        return std::nullopt;
    }
}

namespace Physara::Platform::Detail
{
    std::optional<HostError> LoadGlEntries(DeviceHostState &state)
    {
        state.stage = HostStage::EntryLoading;
        BeginNativeOperation(state);
        // UserPtr 使用相同固定 loader, 让 resolver 借用当前 owner 而不增加全局指针.
        const auto version = gladLoadGLUserPtr(ResolveGlEntry, &state);
        if (auto error = TakeNativeError(state, HostErrorReason::EntryLoading, state.stage))
        {
            return error;
        }
        if (version == 0 || !GLAD_GL_VERSION_4_6 || glGetString == nullptr || glGetIntegerv == nullptr ||
            glGetBooleanv == nullptr || glGetError == nullptr || glGetFramebufferAttachmentParameteriv == nullptr)
        {
            return MakeError(&state, HostErrorReason::EntryLoading, state.stage,
                             "GLAD failed to load the required OpenGL 4.6 Core query entries");
        }
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (state.checks.stopAfter == state.stage)
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage, "Injected stop after GL loading");
        }
#endif
        return std::nullopt;
    }

    std::optional<HostError> InspectEnvironment(DeviceHostState &state)
    {
        state.stage = HostStage::EnvironmentQuery;
        if (auto error = ReadGlfwEnvironment(state))
        {
            return error;
        }
        if (const auto code = glGetError(); code != GL_NO_ERROR)
        {
            return GlQueryError(state, code, "GL error preceded environment inspection");
        }
        GlObservations observed;
        glGetIntegerv(GL_MAJOR_VERSION, &observed.major);
        glGetIntegerv(GL_MINOR_VERSION, &observed.minor);
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &observed.profile);
        glGetIntegerv(GL_CONTEXT_FLAGS, &observed.flags);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &observed.drawFramebuffer);
        glGetBooleanv(GL_DOUBLEBUFFER, &observed.doubleBuffered);
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_BACK_LEFT,
                                              GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &observed.encoding);
        constexpr std::array<GLenum, 6> LimitNames = {GL_MAX_TEXTURE_SIZE,
                                                      GL_MAX_RENDERBUFFER_SIZE,
                                                      GL_MAX_UNIFORM_BUFFER_BINDINGS,
                                                      GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,
                                                      GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                                                      GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT};
        for (std::size_t index = 0; index < LimitNames.size(); ++index)
        {
            glGetIntegerv(LimitNames[index], &observed.limits[index]);
        }
        const bool stringsValid = ReadGlString(GL_VENDOR, state.info.vendor) &&
                                  ReadGlString(GL_RENDERER, state.info.renderer) &&
                                  ReadGlString(GL_VERSION, state.info.version) &&
                                  ReadGlString(GL_SHADING_LANGUAGE_VERSION, state.info.shadingLanguageVersion);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (state.checks.queryError)
        {
            GLint unused = 0;
            glGetIntegerv(0xFFFFFFFFu, &unused);
        }
        if (state.checks.lowerObservedVersion)
        {
            observed.major = 4;
            observed.minor = 5;
        }
        if (state.checks.compatibilityObservedProfile)
        {
            observed.profile = GL_CONTEXT_COMPATIBILITY_PROFILE_BIT;
        }
        if (state.checks.linearObservedEncoding)
        {
            observed.encoding = GL_LINEAR;
        }
        if (state.checks.singleBufferedObserved)
        {
            observed.doubleBuffered = GL_FALSE;
        }
        if (state.checks.invalidObservedLimit)
        {
            observed.limits[0] = -1;
        }
        if (state.checks.unknownObservedEncoding)
        {
            observed.encoding = -1;
        }
        if (state.checks.nonDefaultObservedFramebuffer)
        {
            observed.drawFramebuffer = 1;
        }
#endif
        if (const auto code = glGetError(); code != GL_NO_ERROR)
        {
            return GlQueryError(state, code, "OpenGL environment query failed");
        }
        if (!stringsValid)
        {
            return MakeError(&state, HostErrorReason::EnvironmentQuery, state.stage,
                             "OpenGL returned an empty required environment string");
        }
        if (auto error = ValidateObservations(state, observed))
        {
            return error;
        }
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (state.checks.stopAfter == state.stage)
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected stop after environment inspection");
        }
#endif
        return std::nullopt;
    }

    std::optional<HostError> RequestSwapInterval(DeviceHostState &state, bool vsync)
    {
        state.stage = HostStage::SwapInterval;
        BeginNativeOperation(state);
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (state.checks.swapIntervalError)
        {
            glfwMakeContextCurrent(nullptr);
        }
#endif
        glfwSwapInterval(vsync ? 1 : 0);
        if (auto error = TakeNativeError(state, HostErrorReason::SwapInterval, state.stage))
        {
            return error;
        }
        state.info.requestedSwapInterval = vsync ? 1 : 0;
#if defined(PHYSARA_PLATFORM_NATIVE_CHECKS)
        if (state.checks.stopAfter == state.stage)
        {
            return MakeError(&state, HostErrorReason::InternalFailure, state.stage,
                             "Injected stop after swap interval request");
        }
#endif
        return std::nullopt;
    }
}
