/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "../OpenGL.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <source_location>
#include <thread>

#include <glad/gl.h>

#include "../Common/StateTracking.hpp"
#include "ContextState.hpp"

namespace Physara::RHI::OpenGL
{
    // 接口签名来源见 RHI/README; GL 实现与下列私有状态由本项目定义.
    inline constexpr std::size_t TextureDimensionCount = 10;
    inline constexpr std::size_t UploadPacketLimit = 64;
    struct FormatCaps
    {
        GLenum internalFormat = 0;
        FormatSupport support = FormatSupport::None;
        GLint viewClass = 0;
        std::uint32_t sampleMask = 0;
    };

    struct DeviceCaps
    {
        std::array<std::array<FormatCaps, TextureDimensionCount>, std::size_t(Format::COUNT)> formats{};
        GLint maxTextureSize = 0;
        GLint max3DTextureSize = 0;
        GLint maxCubeTextureSize = 0;
        GLint maxArrayLayers = 0;
        GLint maxColorAttachments = 0;
        GLint maxDrawBuffers = 0;
        GLint maxSamples = 0;
        GLfloat maxAnisotropy = 0;
        GLfloat maxLodBias = 0;
        GLint minMapAlignment = 0;
    };

    enum class NativeKind : std::uint8_t
    {
        Buffer,
        Texture,
        Sampler,
        Framebuffer
    };
    struct NativeDeletion
    {
        // 创建前预留的节点自持 native 名称; final Release 只入队, 不再分配或执行 GL.
        NativeKind kind;
        GLuint name = 0;
        std::array<GLuint, MaxRenderTargets + 1> views{};
        std::uint32_t viewCount = 0;
        NativeDeletion *next = nullptr;

        explicit NativeDeletion(NativeKind value) noexcept : kind(value)
        {
        }
        [[nodiscard]] bool HasObjects() const noexcept
        {
            return name != 0 || viewCount != 0;
        }
    };

    struct ResourceChecks;
    struct UploadChecks;
    struct UploadWorkPacket;
    struct UploadSlice;
    struct SubmissionState;
    class OpenGLCommandList;
    class UploadResourceLease;
    class OpenGLDevice final : public RefCounter<RHI::Device>
    {
    public:
        explicit OpenGLDevice(const DeviceDesc &desc) noexcept;
        ~OpenGLDevice() override;
        [[nodiscard]] bool Initialize();
        [[nodiscard]] bool Shutdown(ShutdownMode mode, std::uint32_t *nativeError);
        [[nodiscard]] ShutdownPrecondition CheckShutdown(unsigned long knownRoots) const noexcept;
        [[nodiscard]] DrainResult DrainWork(ShutdownMode mode, std::uint32_t *nativeError);
        [[nodiscard]] bool RequireContext(const char *operation, bool allowFaulted = false) const noexcept;
        [[nodiscard]] bool BeginNative(const char *operation, const char *label = nullptr,
                                       std::source_location location = std::source_location::current()) noexcept;
        [[nodiscard]] bool CheckNative(const char *operation, GLuint object = 0, const char *label = nullptr,
                                       std::source_location location = std::source_location::current()) noexcept;
        void Report(const char *operation, const char *reason, const char *resource = nullptr,
                    GLenum nativeError = GL_NO_ERROR,
                    std::source_location location = std::source_location::current()) const noexcept;
        void ReleaseAllocation(std::unique_ptr<NativeDeletion> record, bool published) noexcept;
        [[nodiscard]] const FormatCaps &GetFormatCaps(Format format, TextureDimension dimension) const noexcept;
        [[nodiscard]] const DeviceCaps &GetCaps() const noexcept
        {
            return m_Caps;
        }
        [[nodiscard]] GLenum GetFirstNativeError() const noexcept
        {
            return m_FirstNativeError;
        }

        HeapHandle CreateHeap(const HeapDesc &d) override;
        bool QueryTopLevelAccelStructPrebuildInfo(const RT::AccelStructDesc &desc, uint32_t instanceCount,
                                                  RT::AccelStructPrebuildInfo &outInfo) override;
        TextureHandle CreateTexture(const TextureDesc &d) override;
        MemoryRequirements GetTextureMemoryRequirements(Texture *texture) override;
        bool BindTextureMemory(Texture *texture, Heap *heap, uint64_t offset) override;
        TextureHandle CreateHandleForNativeTexture(ObjectType objectType, Object texture,
                                                   const TextureDesc &desc) override;
        StagingTextureHandle CreateStagingTexture(const TextureDesc &d, CpuAccessMode cpuAccess) override;
        void *MapStagingTexture(StagingTexture *tex, const TextureSlice &slice, CpuAccessMode cpuAccess,
                                size_t *outRowPitch) override;
        void UnmapStagingTexture(StagingTexture *tex) override;
        void GetTextureTiling(Texture *texture, uint32_t *numTiles, PackedMipDesc *desc, TileShape *tileShape,
                              uint32_t *subresourceTilingsNum, SubresourceTiling *subresourceTilings) override;
        void UpdateTextureTileMappings(Texture *texture, const TextureTilesMapping *tileMappings,
                                       uint32_t numTileMappings,
                                       CommandQueue executionQueue = CommandQueue::Graphics) override;
        SamplerFeedbackTextureHandle CreateSamplerFeedbackTexture(Texture *pairedTexture,
                                                                  const SamplerFeedbackTextureDesc &desc) override;
        SamplerFeedbackTextureHandle CreateSamplerFeedbackForNativeTexture(ObjectType objectType, Object texture,
                                                                           Texture *pairedTexture) override;
        BufferHandle CreateBuffer(const BufferDesc &d) override;
        void *MapBuffer(Buffer *buffer, CpuAccessMode cpuAccess) override;
        void UnmapBuffer(Buffer *buffer) override;
        MemoryRequirements GetBufferMemoryRequirements(Buffer *buffer) override;
        bool BindBufferMemory(Buffer *buffer, Heap *heap, uint64_t offset) override;
        BufferHandle CreateHandleForNativeBuffer(ObjectType objectType, Object buffer, const BufferDesc &desc) override;
        ShaderHandle CreateShader(const ShaderDesc &d, const void *binary, size_t binarySize) override;
        ShaderHandle CreateShaderSpecialization(Shader *baseShader, const ShaderSpecialization *constants,
                                                uint32_t numConstants) override;
        ShaderLibraryHandle CreateShaderLibrary(const void *binary, size_t binarySize) override;
        SamplerHandle CreateSampler(const SamplerDesc &d) override;
        InputLayoutHandle CreateInputLayout(const VertexAttributeDesc *d, uint32_t attributeCount,
                                            Shader *vertexShader) override;
        EventQueryHandle CreateEventQuery() override;
        void SetEventQuery(EventQuery *query, CommandQueue queue) override;
        bool PollEventQuery(EventQuery *query) override;
        void WaitEventQuery(EventQuery *query) override;
        void ResetEventQuery(EventQuery *query) override;
        TimerQueryHandle CreateTimerQuery() override;
        bool PollTimerQuery(TimerQuery *query) override;
        float GetTimerQueryTime(TimerQuery *query) override;
        void ResetTimerQuery(TimerQuery *query) override;
        GraphicsAPI GetGraphicsAPI() override;
        FramebufferHandle CreateFramebuffer(const FramebufferDesc &desc) override;
        GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc &desc,
                                                      FramebufferInfo const &fbinfo) override;
        GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc &desc, Framebuffer *fb) override;
        ComputePipelineHandle CreateComputePipeline(const ComputePipelineDesc &desc) override;
        MeshletPipelineHandle CreateMeshletPipeline(const MeshletPipelineDesc &desc,
                                                    FramebufferInfo const &fbinfo) override;
        MeshletPipelineHandle CreateMeshletPipeline(const MeshletPipelineDesc &desc, Framebuffer *fb) override;
        RT::PipelineHandle CreateRayTracingPipeline(const RT::PipelineDesc &desc) override;
        BindingLayoutHandle CreateBindingLayout(const BindingLayoutDesc &desc) override;
        BindingLayoutHandle CreateBindlessLayout(const BindlessLayoutDesc &desc) override;
        BindingSetHandle CreateBindingSet(const BindingSetDesc &desc, BindingLayout *layout) override;
        DescriptorTableHandle CreateDescriptorTable(BindingLayout *layout) override;
        void ResizeDescriptorTable(DescriptorTable *descriptorTable, uint32_t newSize,
                                   bool keepContents = true) override;
        bool WriteDescriptorTable(DescriptorTable *descriptorTable, const BindingSetItem &item) override;
        RT::OpacityMicromapHandle CreateOpacityMicromap(const RT::OpacityMicromapDesc &desc) override;
        RT::AccelStructHandle CreateAccelStruct(const RT::AccelStructDesc &desc) override;
        MemoryRequirements GetAccelStructMemoryRequirements(RT::AccelStruct *as) override;
        RT::Cluster::OperationSizeInfo GetClusterOperationSizeInfo(const RT::Cluster::OperationParams &params) override;
        bool BindAccelStructMemory(RT::AccelStruct *as, Heap *heap, uint64_t offset) override;
        CommandListHandle CreateCommandList(const CommandListParameters &params = CommandListParameters()) override;
        uint64_t ExecuteCommandLists(CommandList *const *pCommandLists, size_t numCommandLists,
                                     CommandQueue executionQueue = CommandQueue::Graphics) override;
        void QueueWaitForCommandList(CommandQueue waitQueue, CommandQueue executionQueue, uint64_t instance) override;
        bool WaitForIdle() override;
        CommandListLifetimeTrackerHandle CreateCommandListLifetimeTracker(CommandQueue executionQueue) override;
        void RunGarbageCollection() override;
        bool QueryFeatureSupport(Feature feature, void *pInfo = nullptr, size_t infoSize = 0) override;
        FormatSupport QueryFormatSupport(Format format) override;
        CoopVec::DeviceFeatures QueryCoopVecFeatures() override;
        CoopVec::MatMulFormatSupport QueryCoopVecMatMulFormatSupport(
            const CoopVec::MatMulFormatCombo &combination) override;
        CoopVec::TrainingFormatSupport QueryCoopVecTrainingFormatSupport(CoopVec::DataType componentType) override;
        size_t GetCoopVecMatrixSize(CoopVec::DataType type, CoopVec::MatrixLayout layout, int rows,
                                    int columns) override;
        Object GetNativeQueue(ObjectType objectType, CommandQueue queue) override;
        MessageCallback *GetMessageCallback() override;
        bool IsAftermathEnabled() override;
        AftermathCrashDumpHelper &GetAftermathCrashDumpHelper() override;

    private:
        enum class Phase : std::uint8_t
        {
            Active,
            Faulted,
            Draining,
            Stopped
        };
        void Drain(bool nativeAvailable) noexcept;
        [[nodiscard]] bool DeleteNative(NativeDeletion &record) noexcept;
        void NativeFailure(const char *operation, GLenum error, const char *resource,
                           std::source_location location) noexcept;
        [[nodiscard]] UploadWorkPacket *OpenPacket(std::size_t minimumChunk);
        void AbandonPacket(UploadWorkPacket &packet) noexcept;
        void ReleaseClaim(std::uint64_t recordingId) noexcept;
        void RetirePacket(UploadWorkPacket &packet, bool nativeAvailable) noexcept;
        [[nodiscard]] bool SealOrphans();
        [[nodiscard]] bool CollectWork(bool wait);
        [[nodiscard]] bool WaitSync(GLsync sync, std::chrono::steady_clock::time_point deadline);
        [[nodiscard]] bool CheckCompletionFence(GLsync sync, const char *operation) noexcept;
        [[nodiscard]] bool AllocateUpload(UploadWorkPacket &packet, std::size_t size, UploadSlice &slice);
        void ReportBudget(const char *operation, std::size_t requested, std::size_t retained,
                          std::size_t limit) const noexcept;
        DeviceDesc m_Desc;
        const std::thread::id m_OwnerThread = std::this_thread::get_id();
        DeviceCaps m_Caps;
        Phase m_Phase = Phase::Active;
        bool m_NativeUsable = true;
        GLenum m_FirstNativeError = GL_NO_ERROR;
        std::mutex m_DeletionMutex;
        NativeDeletion *m_DeletionHead = nullptr;
        NativeDeletion *m_DeletionTail = nullptr;
        std::array<std::unique_ptr<UploadWorkPacket>, UploadPacketLimit> m_Packets{};
        std::unique_ptr<SubmissionState> m_Submissions;
        std::atomic<std::uint64_t> m_ActiveRecording{0};
        std::uint64_t m_LastRecordingId = 0;
        std::uint64_t m_LastSubmissionId = 0;
        std::uint64_t m_CompletedSubmissionId = 0;
        std::size_t m_UploadCapacity = 0;
        unsigned long m_InternallyRetainedWrappers = 0;
        ContextState m_ContextState;
        bool m_WorkFailed = false;
        friend struct ResourceChecks;
        friend struct UploadChecks;
        friend class OpenGLCommandList;
        friend class UploadResourceLease;
    };

    [[nodiscard]] bool BuildCaps(OpenGLDevice &device, DeviceCaps &caps);
    [[nodiscard]] GLenum TextureTarget(TextureDimension dimension) noexcept;
    [[nodiscard]] bool ValidInitialState(ResourceStates state, bool keepInitialState) noexcept;
    [[nodiscard]] bool GetUploadFormat(Format format, GLenum &externalFormat, GLenum &externalType) noexcept;

    // 以下 wrapper 的不可变描述/CPU 引用可跨线程, 原生创建/收集限 owning/current 线程.
    // 每个活 wrapper 强持有 Device; 外部根 owner 必须保留到全部 wrapper 释放和退休排空.
    class OpenGLBuffer final : public RefCounter<RHI::Buffer>
    {
    public:
        OpenGLBuffer(OpenGLDevice *device, const BufferDesc &desc);
        ~OpenGLBuffer() override;
        [[nodiscard]] const BufferDesc &GetDesc() const override
        {
            return m_Desc;
        }
        [[nodiscard]] GpuVirtualAddress GetGpuVirtualAddress() const override
        {
            return 0;
        }
        void Publish() noexcept
        {
            m_Published = true;
        }

    private:
        RefCountPtr<OpenGLDevice> m_Device;
        BufferDesc m_Desc;
        BufferStateExtension m_State{m_Desc};
        unsigned long m_InternalUploadHolds = 0;
        std::unique_ptr<NativeDeletion> m_Deletion;
        bool m_Published = false;
        friend class OpenGLDevice;
        friend struct ResourceChecks;
        friend struct UploadChecks;
        friend class OpenGLCommandList;
    };

    class OpenGLTexture final : public RefCounter<RHI::Texture>
    {
    public:
        OpenGLTexture(OpenGLDevice *device, const TextureDesc &desc);
        ~OpenGLTexture() override;
        [[nodiscard]] const TextureDesc &GetDesc() const override
        {
            return m_Desc;
        }
        Object GetNativeView(ObjectType, Format, TextureSubresourceSet, TextureDimension, bool,
                             std::optional<ComponentMapping>) override
        {
            return nullptr;
        }
        [[nodiscard]] OpenGLDevice *Owner() const noexcept
        {
            return m_Device.Get();
        }
        [[nodiscard]] GLuint Name() const noexcept
        {
            return m_Deletion->name;
        }
        void Publish() noexcept
        {
            m_Published = true;
        }

    private:
        RefCountPtr<OpenGLDevice> m_Device;
        TextureDesc m_Desc;
        TextureStateExtension m_State{m_Desc};
        unsigned long m_InternalUploadHolds = 0;
        std::unique_ptr<NativeDeletion> m_Deletion;
        bool m_Published = false;
        friend class OpenGLDevice;
        friend struct ResourceChecks;
        friend struct UploadChecks;
        friend class OpenGLCommandList;
    };

    class OpenGLSampler final : public RefCounter<RHI::Sampler>
    {
    public:
        OpenGLSampler(OpenGLDevice *device, const SamplerDesc &desc);
        ~OpenGLSampler() override;
        [[nodiscard]] const SamplerDesc &GetDesc() const override
        {
            return m_Desc;
        }
        void Publish() noexcept
        {
            m_Published = true;
        }

    private:
        RefCountPtr<OpenGLDevice> m_Device;
        SamplerDesc m_Desc;
        std::unique_ptr<NativeDeletion> m_Deletion;
        bool m_Published = false;
        friend class OpenGLDevice;
        friend struct ResourceChecks;
    };

    class OpenGLFramebuffer final : public RefCounter<RHI::Framebuffer>
    {
    public:
        OpenGLFramebuffer(OpenGLDevice *device, const FramebufferDesc &desc);
        ~OpenGLFramebuffer() override;
        [[nodiscard]] const FramebufferDesc &GetDesc() const override
        {
            return m_Desc;
        }
        [[nodiscard]] const FramebufferInfoEx &GetFramebufferInfo() const override
        {
            return m_Info;
        }
        void Publish() noexcept
        {
            m_Published = true;
        }

    private:
        RefCountPtr<OpenGLDevice> m_Device;
        FramebufferDesc m_Desc;
        FramebufferInfoEx m_Info;
        std::array<TextureHandle, MaxRenderTargets + 1> m_Textures{};
        std::unique_ptr<NativeDeletion> m_Deletion;
        bool m_Published = false;
        friend class OpenGLDevice;
        friend struct ResourceChecks;
    };
}
