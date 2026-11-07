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

/// @file
/// @brief GL 后端跨文件私有合同, Device 拥有 caps, 物理包和删除 FIFO, wrapper 持有描述与 Device 引用.

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
#include "ShaderCaps.hpp"

namespace Physara::RHI::OpenGL
{
    // 接口签名来源见 RHI/README; GL 实现与下列私有状态由本项目定义.
    inline constexpr std::size_t TextureDimensionCount = 10;
    inline constexpr std::size_t UploadPacketLimit = 64;
    /// @brief owner 线程维护的内部引用账目, 包 lease 与内部复合对象的实际强引用分别计数.
    /// @note 关闭预检前调用方已停止用户; 不替代 Resource 自身的原子引用计数.
    struct InternalOwnership
    {
        unsigned long packetHolds = 0;     ///< 各物理包持有本 wrapper 的 lease 数.
        unsigned long dependencyHolds = 0; ///< 已由包保活的父对象对本 wrapper 的实际强引用边数.
    };
    /// @brief 单个格式/target 的实际用途交集, sampleMask 的第 n 位表示支持 n 个样本.
    struct FormatCaps
    {
        GLenum internalFormat = 0;
        FormatSupport support = FormatSupport::None;
        GLint viewClass = 0;
        std::uint32_t sampleMask = 0;
    };

    /// @brief 创建时查询的设备限制及格式表, 准备与执行能力仍由各工厂/命令入口明确限定.
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
        std::array<GLint, 2> maxViewportDimensions{};
        std::array<GLfloat, 2> viewportBounds{};
        ShaderCaps shader;
    };

    /// @brief 删除 FIFO 的原生对象类别, Program 与临时 GL Shader 不等同于公共 CPU Shader 对象.
    enum class NativeKind : std::uint8_t
    {
        Buffer,
        Texture,
        Sampler,
        Framebuffer,
        Program,
        Shader
    };
    /// @brief 创建前预留的原生删除节点, final Release 只转交节点, owner 才执行 GL 删除.
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

    struct UploadWorkPacket;
    struct UploadSlice;
    struct SubmissionState;
    class OpenGLCommandList;
    class UploadResourceLease;
    /// @brief 原生设备状态的唯一 owner, 跨实现文件管理能力, 上传, 提交与删除.
    /// @invariant Active 接受新工作, Faulted 保留未知完成前缀, Draining 仅推进退出, Stopped 后才可最终析构.
    /// @note context 健康与阶段独立, CPU 引用释放不能代替 GPU 完成证明.
    class OpenGLDevice final : public RefCounter<RHI::Device>
    {
    public:
        explicit OpenGLDevice(const DeviceDesc &desc);
        ~OpenGLDevice() override;
        /// @brief 核对实际入口和环境, 查询 caps 并建立 upper-left/zero-to-one clip 约定.
        [[nodiscard]] bool Initialize();
        /// @brief 单根关闭适配, 先预检外部 owner, 再收拢设备; false 可为 Pending 或清理带错.
        [[nodiscard]] bool Shutdown(ShutdownMode mode, std::uint32_t *nativeError);
        /// @brief 在使用者已停止后精确核对根引用及内部 lease, 不修改设备或调用 GL.
        [[nodiscard]] ShutdownPrecondition CheckShutdown(unsigned long knownRoots) const noexcept;
        /// @brief 正常 fence 收拢可超时重试, 无可靠完成证明时仅在退出尝试全流收拢.
        [[nodiscard]] DrainResult DrainWork(ShutdownMode mode, std::uint32_t *nativeError);
        /// @brief 核对 owner, 阶段, 确切 current context 和原生可用性, 拒绝不改变 context.
        [[nodiscard]] bool RequireContext(const char *operation, bool allowFaulted = false) const noexcept;
        /// @brief 发出新操作前检查调用合同和已有 GL 错误, 防止把前序故障归因到新操作.
        [[nodiscard]] bool BeginNative(const char *operation, const char *label = nullptr,
                                       std::source_location location = std::source_location::current()) noexcept;
        /// @brief 消费一次 GL 错误并锁存失败/健康状态, 不将未知完成的工作当作可释放.
        [[nodiscard]] bool CheckNative(const char *operation, GLuint object = 0, const char *label = nullptr,
                                       std::source_location location = std::source_location::current()) noexcept;
        /// @brief 以固定栈缓冲组合操作, 资源与原生错误, 同步交给借用的消息回调.
        void Report(const char *operation, const char *reason, const char *resource = nullptr,
                    GLenum nativeError = GL_NO_ERROR,
                    std::source_location location = std::source_location::current()) const noexcept;
        /// @brief 未发布分配可在所属 context 回滚; 已发布对象仅入删除 FIFO, 不等待 GPU.
        void ReleaseAllocation(std::unique_ptr<NativeDeletion> record, bool published) noexcept;
        /// @brief 只读查询指定格式/target 的能力, 非法或未映射项返回零能力, 不调用 GL.
        [[nodiscard]] const FormatCaps &GetFormatCaps(Format format, TextureDimension dimension) const noexcept;
        [[nodiscard]] const DeviceCaps &GetCaps() const noexcept
        {
            return m_Caps;
        }
        /// @brief 按需准备设备自有 nearest/clamp sampler, 失败返回零, 不持有反向 Device 引用.
        /// @pre 调用点已通过 BeginNative, 仅在 owner/current context 的准备阶段使用.
        [[nodiscard]] GLuint PrepareDummySampler();
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
        /// @brief 复制 canonical VS/FS 字节并解析入口反射, 失败返回空, 不在此编译 GL Shader.
        ShaderHandle CreateShader(const ShaderDesc &d, const void *binary, size_t binarySize) override;
        ShaderHandle CreateShaderSpecialization(Shader *baseShader, const ShaderSpecialization *constants,
                                                uint32_t numConstants) override;
        ShaderLibraryHandle CreateShaderLibrary(const void *binary, size_t binarySize) override;
        SamplerHandle CreateSampler(const SamplerDesc &d) override;
        /// @brief 保存按顺序展开的属性描述; 可选 VS 只核对归属/阶段, 不决定另一套 location ABI.
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
        /// @brief 按格式/样本兼容信息核对单样本状态与 IO, 生成绑定计划并事务式链接 Program.
        /// @note framebuffer 重载先核对本设备归属再取兼容信息; 两者均不保活具体附件或执行 Draw.
        GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc &desc,
                                                      FramebufferInfo const &fbinfo) override;
        GraphicsPipelineHandle CreateGraphicsPipeline(const GraphicsPipelineDesc &desc, Framebuffer *fb) override;
        ComputePipelineHandle CreateComputePipeline(const ComputePipelineDesc &desc) override;
        MeshletPipelineHandle CreateMeshletPipeline(const MeshletPipelineDesc &desc,
                                                    FramebufferInfo const &fbinfo) override;
        MeshletPipelineHandle CreateMeshletPipeline(const MeshletPipelineDesc &desc, Framebuffer *fb) override;
        RT::PipelineHandle CreateRayTracingPipeline(const RT::PipelineDesc &desc) override;
        /// @brief 保存常规逻辑布局, 不占用 GL 单元; 非基线类别/数组/可见阶段明确失败.
        BindingLayoutHandle CreateBindingLayout(const BindingLayoutDesc &desc) override;
        BindingLayoutHandle CreateBindlessLayout(const BindlessLayoutDesc &desc) override;
        /// @brief 全量预检后物化自有二维视图并发布集合, 集合强持有布局/资源, 不改全局绑定.
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
        // Active 接受新工作; Faulted 保留无法证明完成的前缀; Draining 只推进显式退出, 超时可重试.
        // Stopped 才允许最终 CPU 析构. NativeUsable 独立记录 context 健康, 不从 phase/日志严重度推断 loss.
        enum class Phase : std::uint8_t
        {
            Active,
            Faulted,
            Draining,
            Stopped
        };
        /// @brief 摘走删除 FIFO 后在锁外清理, context 不可用或删除失败时由最终 context 销毁回收原生存储.
        void Drain(bool nativeAvailable) noexcept;
        /// @brief 先删对象再删其自有 view, 原生错误后停止后续未知 GL 清理.
        [[nodiscard]] bool DeleteNative(NativeDeletion &record) noexcept;
        /// @brief 记录首个 GL 错误并停止新工作, 仅依据原生 loss/reset 事实更新 context 健康.
        void NativeFailure(const char *operation, GLenum error, const char *resource,
                           std::source_location location) noexcept;
        /// @brief 先构造物理包和 tracker, 再取得唯一 recording claim, 成功后发布不回绕身份.
        [[nodiscard]] UploadWorkPacket *OpenPacket(std::size_t minimumChunk);
        /// @brief 解除本包 recording claim 后 release 交接 orphan, worker 此后不再访问包或 lease.
        void AbandonPacket(UploadWorkPacket &packet) noexcept;
        /// @brief 只解除与 recordingId 匹配的 claim, 不影响后继录制.
        void ReleaseClaim(std::uint64_t recordingId) noexcept;
        /// @brief 完成或退出边界统一清 tracker, chunk, lease 后销毁包, 由 owner 独占执行.
        void RetirePacket(UploadWorkPacket &packet, bool nativeAvailable) noexcept;
        /// @brief acquire 接收交接包, 有已发工作时建立内部 fence, 不发布公开票据或 tracker metadata.
        [[nodiscard]] bool SealOrphans();
        /// @brief 按 FIFO 收集真实完成, wait 共享一次五秒截止点; false 由失败标记区分 Pending 与故障.
        [[nodiscard]] bool CollectWork(bool wait);
        /// @brief 分段等待至统一截止点, 只将实际 signaled 视为成功, WAIT_FAILED 锁存停止.
        [[nodiscard]] bool WaitSync(GLsync sync, std::chrono::steady_clock::time_point deadline);
        /// @brief 同时检查 GL 错误和 sync 的实际有效性, 零 fence 不表示完成.
        [[nodiscard]] bool CheckCompletionFence(GLsync sync, const char *operation) noexcept;
        /// @brief 从当前包尾部或新 coherent chunk 分配, 受设备总预算约束, 不隐式等待其他包退休.
        [[nodiscard]] bool AllocateUpload(UploadWorkPacket &packet, std::size_t size, UploadSlice &slice);
        /// @brief 输出 requested/retained/limit, 让容量拒绝可归因, 不伪装成硬件显存不足.
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
        // 每个拥有包 lease 的 wrapper 只贡献一个 Device 根, 不随其跨包 lease 数重复计数.
        unsigned long m_InternallyRetainedWrappers = 0;
        ContextState m_ContextState;
        std::unique_ptr<NativeDeletion> m_DummySampler;
        bool m_WorkFailed = false;
        friend class OpenGLCommandList;
        friend class UploadResourceLease;
    };

    /// @brief 查询限制和每 target 的格式/样本集合, 再验证必需的 2D 基线.
    [[nodiscard]] bool BuildCaps(OpenGLDevice &device, DeviceCaps &caps);
    /// @brief 核对图形准备所需入口并查询 VS/FS 与原生全局限额.
    [[nodiscard]] bool BuildShaderCaps(OpenGLDevice &device, ShaderCaps &caps);
    /// @brief 按 RHI 维度取得原生 target, Unknown 或非法值返回零.
    [[nodiscard]] GLenum TextureTarget(TextureDimension dimension) noexcept;
    /// @brief 检查普通状态元数据与 keepInitialState 的一致性, 不推导 texel 初始化或命令支持.
    [[nodiscard]] bool ValidInitialState(ResourceStates state, bool keepInitialState) noexcept;
    /// @brief 取得已支持颜色上传的 external format/type, 不支持时输出参数不变.
    [[nodiscard]] bool GetUploadFormat(Format format, GLenum &externalFormat, GLenum &externalType) noexcept;

    /// @brief 不可变 Buffer 存储的 CPU wrapper, 自持描述并强持有 Device, 发布后析构仅交接删除节点.
    /// @note 以下资源 wrapper 的原生创建/收集限 owning/current, CPU 引用可跨线程释放.
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
        InternalOwnership m_InternalOwners;
        BufferDesc m_Desc;
        BufferStateExtension m_State{m_Desc};
        std::unique_ptr<NativeDeletion> m_Deletion;
        bool m_Published = false;
        friend class OpenGLDevice;
        friend class OpenGLCommandList;
    };

    /// @brief 纹理存储, 不可变描述及 tracker extension 的共同 owner, 不因创建而初始化 texel.
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
        InternalOwnership m_InternalOwners;
        TextureDesc m_Desc;
        TextureStateExtension m_State{m_Desc};
        std::unique_ptr<NativeDeletion> m_Deletion;
        bool m_Published = false;
        friend class OpenGLDevice;
        friend class OpenGLCommandList;
    };

    /// @brief 独立采样参数对象, 不拥有或修改 Texture 存储的采样配置.
    class OpenGLSampler final : public RefCounter<RHI::Sampler>
    {
    public:
        OpenGLSampler(OpenGLDevice *device, const SamplerDesc &desc);
        ~OpenGLSampler() override;
        [[nodiscard]] const SamplerDesc &GetDesc() const override
        {
            return m_Desc;
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
        friend class OpenGLCommandList;
        RefCountPtr<OpenGLDevice> m_Device;
        InternalOwnership m_InternalOwners;
        SamplerDesc m_Desc;
        std::unique_ptr<NativeDeletion> m_Deletion;
        bool m_Published = false;
        friend class OpenGLDevice;
    };

    /// @brief 一个实际附件集的 FBO owner, 强持有纹理及自有重解释 view, 先退休 FBO/view 再释放附件.
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
        friend class OpenGLCommandList;
        RefCountPtr<OpenGLDevice> m_Device;
        InternalOwnership m_InternalOwners;
        FramebufferDesc m_Desc;
        FramebufferInfoEx m_Info;
        std::array<TextureHandle, MaxRenderTargets + 1> m_Textures{};
        std::unique_ptr<NativeDeletion> m_Deletion;
        bool m_Published = false;
        friend class OpenGLDevice;
    };
}
