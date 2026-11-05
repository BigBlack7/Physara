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
/// @brief Validation 实现共享的无状态输入/profile 规则与包装对象, CPU 保活不推断原生归属或 GPU 完成.

#include <atomic>
#include <ostream>
#include <set>
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>

#include "Validation.hpp"

namespace Physara::RHI::Validation
{
    /// @brief 同步报告带源位置的有界诊断, 文本不跨 callback 返回保留, 回调不抛出或重入.
    void Report(MessageCallback *callback, MessageSeverity severity, std::string_view message,
                std::source_location location = std::source_location::current());
    /// @brief 当前 API 策略过滤, 通过仅表示可以继续委托, 不意味着底层支持操作.
    namespace Profile
    {
        /// @brief 从实际 Device 查询 API, 不以外部配置或资源地址猜测后端.
        bool IsOpenGL(Device *device);
        /// @brief GL 上报告策略拒绝并返回 true, 其他 API 返回 false; 与 Check 系列的成功布尔值区分.
        bool RejectOpenGL(Device *device, std::source_location location = std::source_location::current());
        /// @brief 检查队列枚举与 API 策略, 通过为 true, 不创建或缓存原生队列.
        bool CheckQueue(Device *device, CommandQueue queue,
                        std::source_location location = std::source_location::current());
        /// @brief 先过滤已排除的能力, 其余查询委托设备, 不把策略允许提升为原生支持.
        bool QueryFeatureSupport(Device *device, Feature feature, void *info = nullptr, size_t infoSize = 0,
                                 std::source_location location = std::source_location::current());
        /// @brief 检查 Shader/RenderState 是否携带当前 GL 策略排除的扩展, 无原生副作用.
        bool HasExcludedShaderExtensions(const ShaderDesc &desc);
        bool HasExcludedRenderExtensions(const RenderState &state);
        /// @brief 检查集合/布局或 Pipeline 中的排除扩展, 只有实际拒绝才返回 true 并报告.
        /// 后续 RejectBindingSet/RejectGraphicsPipeline/RejectComputePipeline 遵循同一返回约定.
        bool RejectBindingLayout(Device *device, BindingLayout *layout,
                                 std::source_location location = std::source_location::current());
        bool RejectBindingSet(Device *device, BindingSet *bindings,
                              std::source_location location = std::source_location::current());
        bool RejectGraphicsPipeline(Device *device, const GraphicsPipelineDesc &desc,
                                    std::source_location location = std::source_location::current());
        bool RejectComputePipeline(Device *device, const ComputePipelineDesc &desc,
                                   std::source_location location = std::source_location::current());
        constexpr ResourceStates OpenGLExcludedStates = static_cast<ResourceStates>(
            uint32_t(ResourceStates::AccelStructRead) | uint32_t(ResourceStates::AccelStructWrite) |
            uint32_t(ResourceStates::AccelStructBuildInput) | uint32_t(ResourceStates::AccelStructBuildBlas) |
            uint32_t(ResourceStates::ShadingRateSurface) | uint32_t(ResourceStates::OpacityMicromapWrite) |
            uint32_t(ResourceStates::OpacityMicromapBuildInput) | uint32_t(ResourceStates::ConvertCoopVecMatrixInput) |
            uint32_t(ResourceStates::ConvertCoopVecMatrixOutput));
    }
    /// @brief 创建与命令共同使用的无状态输入校验, 不保存资源地址或推断原生 Device 归属.
    namespace Input
    {
        /// @brief 报告输入拒绝并恒返回 false, 供 Check 系列在首错处直接退出.
        bool Reject(Device *device, const char *reason, const std::string *name = nullptr,
                    std::source_location location = std::source_location::current());
        /// @brief 检查已知状态位及 API 策略, allowUnknown 只允许无已知前态, 不伪造初始化.
        bool CheckState(Device *device, ResourceStates state, bool allowUnknown = false,
                        std::source_location location = std::source_location::current());
        /// @brief 检查维度/层/mip/样本及 CPU 子资源表可表示性, 按需查询设备用途支持.
        bool CheckTextureDesc(Device *device, const TextureDesc &desc, bool querySupport = false,
                              std::source_location location = std::source_location::current());
        /// @brief 检查容量, 用途和互斥描述选项, querySupport 时才查询相关设备能力.
        bool CheckBufferDesc(Device *device, const BufferDesc &desc, bool querySupport = false,
                             std::source_location location = std::source_location::current());
        /// @brief 验证解析交集非空与可选单 mip 约束, 显式 copy/write 范围另做完整检查.
        bool CheckTextureRange(Device *device, Texture *texture, const TextureSubresourceSet &range,
                               bool singleMip = false, std::source_location location = std::source_location::current());
        /// @brief 检查显式 texel 区域, mip/layer 与原尺寸, 拒绝越界而不静默裁剪请求.
        bool CheckTextureSlice(Device *device, const TextureDesc &desc, const TextureSlice &slice,
                               std::source_location location = std::source_location::current());
        /// @brief 核对两个显式区域的维度/格式/样本与大小兼容, 不在失败时发出底层复制.
        bool CheckTextureCopy(Device *device, const TextureDesc &dest, const TextureSlice &destSlice,
                              const TextureDesc &src, const TextureSlice &srcSlice,
                              std::source_location location = std::source_location::current());
        /// @brief 使用剩余容量检查字节区间, 避免 offset+size 溢出, 零长度按命令合同解释.
        bool CheckBufferRange(Device *device, Buffer *buffer, uint64_t offset, uint64_t size,
                              std::source_location location = std::source_location::current());
        /// @brief 核对资源/view 格式解释及请求用途, 保留 stencil-only 平面与完整深度资源的区别.
        bool CheckViewFormat(Device *device, const TextureDesc &desc, Format view, FormatSupport required,
                             std::source_location location = std::source_location::current());
        /// @brief 检查可见阶段, 逻辑槽去重, 数组形状与 push constants 预算, 不建立原生布局.
        bool CheckBindingLayoutDesc(Device *device, const BindingLayoutDesc &desc,
                                    std::source_location location = std::source_location::current());
    }
    class DeviceWrapper;

    /// @brief 粗粒度闭区间摘要, min>max 表示空, 用于快速判断同资源类的槽范围交叠.
    struct Range
    {
        uint32_t min = ~0u;
        uint32_t max = 0;

        void Add(uint32_t item);
        [[nodiscard]] bool Empty() const;
        [[nodiscard]] bool OverlapsWith(const Range &other) const;
    };

    enum class GraphicsResourceType : uint32_t
    {
        SRV,
        Sampler,
        UAV,
        CB
    };

    /// @brief 完整逻辑位置身份, 资源类/registerSpace/slot/arrayElement 共同参与相等与排序.
    struct BindingLocation
    {
        GraphicsResourceType type = GraphicsResourceType::SRV;
        uint32_t registerSpace = 0;
        uint32_t slot = 0;
        uint32_t arrayElement = 0;

        bool operator==(BindingLocation const &other) const
        {
            return type == other.type && registerSpace == other.registerSpace && slot == other.slot &&
                   arrayElement == other.arrayElement;
        }

        bool operator!=(BindingLocation const &other) const
        {
            return !(*this == other);
        }

        // 有序集合按完整逻辑位置去重, registerSpace/arrayElement 均参与身份, 与相等比较保持一致.
        bool operator<(BindingLocation const &other) const noexcept
        {
            return std::tie(type, registerSpace, slot, arrayElement) <
                   std::tie(other.type, other.registerSpace, other.slot, other.arrayElement);
        }
    };

    // MSVC Debug 的 hash 工作表在 noexcept 内分配 proxy; 有序集合使本地存储失败可传播到既定边界.
    using BindingLocationSet = std::set<BindingLocation>;

    /// @brief 每阶段/布局的完整逻辑位置集与按资源类的粗范围, 用于缺项, 重复和冲突诊断.
    struct BindingSummary
    {
        BindingLocationSet locations;
        uint32_t numVolatileCBs = 0;
        Range rangeSRV;
        Range rangeSampler;
        Range rangeUAV;
        Range rangeCB;

        [[nodiscard]] bool Any() const;
        [[nodiscard]] bool OverlapsWith(const BindingSummary &other) const;
    };

    std::ostream &operator<<(std::ostream &os, const BindingLocationSet &set);

    enum class CommandListState
    {
        Initial,
        Open,
        Closed
    };

    /// @brief 解包已知 Validation 资源 wrapper, 其他对象原样借出, 不增加引用或创建适配对象.
    Resource *UnwrapResource(Resource *resource);

    /// @brief 记录通用 RT 构建协议的 CPU wrapper 并保活底层 AS, 不代表 GL 已交付 RT 支持.
    class AccelStructWrapper : public RefCounter<RT::AccelStruct>
    {
      public:
        bool isTopLevel = false;
        bool allowCompaction = false;
        bool allowUpdate = false;
        bool wasBuilt = false;

        // BLAS only
        std::vector<RT::GeometryDesc> buildGeometries;

        // TLAS only
        size_t maxInstances = 0;
        size_t buildInstances = 0;

        AccelStructWrapper(AccelStruct *as) : m_AccelStruct(as)
        {
        }
        AccelStruct *GetUnderlyingObject() const
        {
            return m_AccelStruct;
        }

        // IResource

        Object GetNativeObject(ObjectType objectType) override
        {
            return m_AccelStruct->GetNativeObject(objectType);
        }
        bool QueryMemoryRequirements(MemoryRequirements &outRequirements) override
        {
            return m_AccelStruct->QueryMemoryRequirements(outRequirements);
        }

        // IAccelStruct

        const RT::AccelStructDesc &GetDesc() const override
        {
            return m_AccelStruct->GetDesc();
        }
        bool IsCompacted() const override
        {
            return m_AccelStruct->IsCompacted();
        }
        uint64_t GetDeviceAddress() const override
        {
            return m_AccelStruct->GetDeviceAddress();
        };

      private:
        RT::AccelStructHandle m_AccelStruct;
    };

    /// @brief 单写者公共调用协议与输入验证, 强持有父设备及底层列表.
    /// @note Open/Closed 表示通过的调用顺序, 不证明 void 原生调用成功; 即时 claim 只限制录制重叠.
    class CommandListWrapper : public RefCounter<CommandList>
    {
        // 状态描述已通过的公共调用顺序; 底层 void 方法的原生失败仍由底层诊断/停止协议处理.
      public:
        friend class DeviceWrapper;

        CommandListWrapper(DeviceWrapper *device, CommandList *commandList, bool isImmediate, CommandQueue queueType);
        ~CommandListWrapper() override;

      protected:
        CommandListHandle m_CommandList;
        RefCountPtr<DeviceWrapper> m_Device;
        MessageCallback *m_MessageCallback;
        bool m_IsImmediate;
        bool m_HasImmediateClaim = false;
        CommandQueue m_QueueType;

        CommandListState m_State = CommandListState::Initial;
        bool m_GraphicsStateSet = false;
        bool m_ComputeStateSet = false;
        bool m_MeshletStateSet = false;
        bool m_RayTracingStateSet = false;
        GraphicsState m_CurrentGraphicsState;
        ComputeState m_CurrentComputeState;
        MeshletState m_CurrentMeshletState;
        RT::State m_CurrentRayTracingState;

        size_t m_PipelinePushConstantSize = 0;
        bool m_PushConstantsSet = false;

        void Error(const std::string &messageText,
                   std::source_location location = std::source_location::current()) const;
        void Warning(const std::string &messageText,
                     std::source_location location = std::source_location::current()) const;

        // 校验 recording 前提, 拒绝时不委托底层列表.
        bool RequireOpenState(std::source_location location = std::source_location::current()) const;
        // 只有 Closed 可进入批提交预检, 不在这里消费状态.
        bool RequireExecuteState(std::source_location location = std::source_location::current()) const;
        // 根据队列能力层级拒绝不适合当前列表的命令.
        bool RequireType(CommandQueue queueType, const char *operation) const;
        CommandList *GetUnderlyingCommandList() const
        {
            return m_CommandList;
        }

        // 布局已校验重复项后提取唯一 push constants 字节数, 无声明时为零.
        void EvaluatePushConstantSize(const Physara::RHI::BindingLayoutVector &bindingLayouts);
        // 每次设置 Pipeline 状态后必须重新提供所需 push constants, 不沿用上一代提交状态.
        bool ValidatePushConstants(const char *pipelineType, const char *stateFunctionName) const;
        // 按位置核对集合与布局身份/种类, 普通 set 再校验完整资源内容, 不仅比较数量.
        bool ValidateBindingSetsAgainstLayouts(const StaticVector<BindingLayoutHandle, MaxBindingLayouts> &layouts,
                                               const StaticVector<BindingSet *, MaxBindingLayouts> &sets,
                                               std::source_location location = std::source_location::current()) const;
        // 检查间接参数 Buffer 的用途与 offset/count/stride 所需字节范围.
        bool ValidateIndirectBuffer(Buffer *buffer, uint64_t offset, uint64_t count, size_t stride) const;

        // 核对 TLAS 构建/更新条件与实例预算, 不修改 wrapper 的已构建状态.
        bool ValidateBuildTopLevelAccelStruct(AccelStructWrapper *wrapper, size_t numInstances,
                                              RT::AccelStructBuildFlags buildFlags) const;

      public:
        // IResource implementation

        Object GetNativeObject(ObjectType objectType) override;

        // ICommandList implementation

        void Open() override;
        void Close() override;
        void ClearState() override;

        void ClearTextureFloat(Texture *t, TextureSubresourceSet subresources, const Color &clearColor) override;
        void ClearDepthStencilTexture(Texture *t, TextureSubresourceSet subresources, bool clearDepth, float depth,
                                      bool clearStencil, uint8_t stencil) override;
        void ClearTextureUInt(Texture *t, TextureSubresourceSet subresources, uint32_t clearColor) override;

        void CopyTexture(Texture *dest, const TextureSlice &destSlice, Texture *src,
                         const TextureSlice &srcSlice) override;
        void CopyTexture(StagingTexture *dest, const TextureSlice &destSlice, Texture *src,
                         const TextureSlice &srcSlice) override;
        void CopyTexture(Texture *dest, const TextureSlice &destSlice, StagingTexture *src,
                         const TextureSlice &srcSlice) override;
        void WriteTexture(Texture *dest, uint32_t arraySlice, uint32_t mipLevel, const void *data, size_t rowPitch,
                          size_t depthPitch) override;
        void ResolveTexture(Texture *dest, const TextureSubresourceSet &dstSubresources, Texture *src,
                            const TextureSubresourceSet &srcSubresources) override;

        void WriteBuffer(Buffer *b, const void *data, size_t dataSize, uint64_t destOffsetBytes) override;
        void ClearBufferUInt(Buffer *b, uint32_t clearValue) override;
        void CopyBuffer(Buffer *dest, uint64_t destOffsetBytes, Buffer *src, uint64_t srcOffsetBytes,
                        uint64_t dataSizeBytes) override;

        void ClearSamplerFeedbackTexture(SamplerFeedbackTexture *texture) override;
        void DecodeSamplerFeedbackTexture(Buffer *buffer, SamplerFeedbackTexture *texture,
                                          Physara::RHI::Format format) override;
        void SetSamplerFeedbackTextureState(SamplerFeedbackTexture *texture, ResourceStates stateBits) override;

        void SetPushConstants(const void *data, size_t byteSize) override;

        void SetGraphicsState(const GraphicsState &state) override;
        void Draw(const DrawArguments &args) override;
        void DrawIndexed(const DrawArguments &args) override;
        void DrawIndirect(uint32_t offsetBytes, uint32_t drawCount) override;
        void DrawIndexedIndirect(uint32_t offsetBytes, uint32_t drawCount) override;
        void DrawIndexedIndirectCount(uint32_t paramOffsetBytes, uint32_t countOffsetBytes,
                                      uint32_t maxDrawCount) override;

        void SetComputeState(const ComputeState &state) override;
        void Dispatch(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) override;
        void DispatchIndirect(uint32_t offsetBytes) override;

        void SetMeshletState(const MeshletState &state) override;
        void DispatchMesh(uint32_t groupsX, uint32_t groupsY = 1, uint32_t groupsZ = 1) override;
        void DispatchMeshIndirect(uint32_t offsetBytes, uint32_t maxDrawCount) override;
        void DispatchMeshIndirectCount(uint32_t paramOffsetBytes, uint32_t countOffsetBytes,
                                       uint32_t maxDrawCount) override;

        void SetRayTracingState(const RT::State &state) override;
        void DispatchRays(const RT::DispatchRaysArguments &args) override;

        void BuildOpacityMicromap(RT::OpacityMicromap *omm, const RT::OpacityMicromapDesc &desc) override;
        void BuildBottomLevelAccelStruct(RT::AccelStruct *as, const RT::GeometryDesc *pGeometries, size_t numGeometries,
                                         RT::AccelStructBuildFlags buildFlags) override;
        void CompactBottomLevelAccelStructs() override;
        void CopyRaytracingAccelerationStructure(RT::AccelStruct *destination, RT::AccelStruct *source) override;
        void BuildTopLevelAccelStruct(RT::AccelStruct *as, const RT::InstanceDesc *pInstances, size_t numInstances,
                                      RT::AccelStructBuildFlags buildFlags) override;
        void BuildTopLevelAccelStructFromBuffer(
            RT::AccelStruct *as, Physara::RHI::Buffer *instanceBuffer, uint64_t instanceBufferOffset,
            size_t numInstances, RT::AccelStructBuildFlags buildFlags = RT::AccelStructBuildFlags::None) override;
        void ExecuteMultiIndirectClusterOperation(const RT::Cluster::OperationDesc &desc) override;

        void ConvertCoopVecMatrices(CoopVec::ConvertMatrixLayoutDesc const *convertDescs, size_t numDescs) override;

        void BeginTimerQuery(TimerQuery *query) override;
        void EndTimerQuery(TimerQuery *query) override;

        void BeginMarker(const char *name) override;
        void EndMarker() override;

        void SetEnableAutomaticBarriers(bool enable) override;
        void SetResourceStatesForBindingSet(BindingSet *bindingSet) override;

        void SetEnableUavBarriersForTexture(Texture *texture, bool enableBarriers) override;
        void SetEnableUavBarriersForBuffer(Buffer *buffer, bool enableBarriers) override;

        void BeginTrackingTextureState(Texture *texture, TextureSubresourceSet subresources,
                                       ResourceStates stateBits) override;
        void BeginTrackingBufferState(Buffer *buffer, ResourceStates stateBits) override;

        void SetTextureState(Texture *texture, TextureSubresourceSet subresources, ResourceStates stateBits) override;
        void SetBufferState(Buffer *buffer, ResourceStates stateBits) override;
        void SetAccelStructState(RT::AccelStruct *as, ResourceStates stateBits) override;

        void SetPermanentTextureState(Texture *texture, ResourceStates stateBits) override;
        void SetPermanentBufferState(Buffer *buffer, ResourceStates stateBits) override;

        void CommitBarriers() override;

        ResourceStates GetTextureSubresourceState(Texture *texture, ArraySlice arraySlice, MipLevel mipLevel) override;
        ResourceStates GetBufferState(Buffer *buffer) override;

        Device *GetDevice() override;
        const CommandListParameters &GetDesc() override;
    };

    /// @brief 可组合的公共输入校验设备, 强持有底层 Device, 拒绝发生在底层工作之前.
    /// @note 不缓存原生 caps 或维护资源地址注册表, 实际归属和 GPU 寿命仍由原生后端验证.
    class DeviceWrapper : public RefCounter<Device>
    {
      public:
        friend class CommandListWrapper;

        DeviceWrapper(Device *device);

      protected:
        DeviceHandle m_Device;
        MessageCallback *m_MessageCallback;
        std::atomic<unsigned int> m_NumOpenImmediateCommandLists = 0;

        void Error(const std::string &messageText,
                   std::source_location location = std::source_location::current()) const;
        void Warning(const std::string &messageText,
                     std::source_location location = std::source_location::current()) const;

        // 先核对标签再读取活动 union, errorStream 为空可表示 Input 层已报告, 避免重复诊断.
        bool ValidateBindingSetItem(const BindingSetItem &binding, DescriptorTable *pOptDescriptorTable,
                                    std::stringstream &errorStream,
                                    std::source_location location = std::source_location::current());
        // 按 profile, layout, 单项和完整位置集合核对, 数组元素各有身份, 顺序不能代替匹配.
        bool ValidateBindingSetDesc(const BindingSetDesc &desc, BindingLayout *layout,
                                    std::source_location location = std::source_location::current());
        // 逐阶段汇总逻辑位置并核对集合映射与 push constants, 校验完成前不创建 Pipeline.
        bool ValidatePipelineBindingLayouts(const StaticVector<BindingLayoutHandle, MaxBindingLayouts> &bindingLayouts,
                                            const std::vector<Shader *> &shaders) const;
        // 指定阶段必须匹配实际 Shader 描述, 不把非空句柄视为阶段正确.
        bool ValidateShaderType(ShaderType expected, const ShaderDesc &shaderDesc, const char *function) const;
        // 对照 framebuffer 格式/样本信息检查固定状态及扩展能力.
        bool ValidateRenderState(const RenderState &renderState, FramebufferInfo const &fbinfo) const;

        // 校验固定来源的 cluster 操作参数, 实际可执行能力仍须通过 profile 和底层设备.
        bool ValidateClusterOperationParams(const RT::Cluster::OperationParams &params) const;

      public:
        // IResource implementation

        Object GetNativeObject(ObjectType objectType) override;

        // IDevice implementation

        HeapHandle CreateHeap(const HeapDesc &d) override;

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
        void *MapBuffer(Buffer *b, CpuAccessMode mapFlags) override;
        void UnmapBuffer(Buffer *b) override;
        MemoryRequirements GetBufferMemoryRequirements(Buffer *buffer) override;
        bool QueryTopLevelAccelStructPrebuildInfo(const RT::AccelStructDesc &desc, uint32_t instanceCount,
                                                  RT::AccelStructPrebuildInfo &outInfo) override;
        bool BindBufferMemory(Buffer *buffer, Heap *heap, uint64_t offset) override;

        BufferHandle CreateHandleForNativeBuffer(ObjectType objectType, Object buffer, const BufferDesc &desc) override;

        ShaderHandle CreateShader(const ShaderDesc &d, const void *binary, size_t binarySize) override;
        ShaderHandle CreateShaderSpecialization(Shader *baseShader, const ShaderSpecialization *constants,
                                                uint32_t numConstants) override;
        ShaderLibraryHandle CreateShaderLibrary(const void *binary, size_t binarySize) override;

        SamplerHandle CreateSampler(const SamplerDesc &d) override;

        InputLayoutHandle CreateInputLayout(const VertexAttributeDesc *d, uint32_t attributeCount,
                                            Shader *vertexShader) override;

        // event queries
        EventQueryHandle CreateEventQuery() override;
        void SetEventQuery(EventQuery *query, CommandQueue queue) override;
        bool PollEventQuery(EventQuery *query) override;
        void WaitEventQuery(EventQuery *query) override;
        void ResetEventQuery(EventQuery *query) override;

        // timer queries
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
    };

} // namespace Physara::RHI::Validation
