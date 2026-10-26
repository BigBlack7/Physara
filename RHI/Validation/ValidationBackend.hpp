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

#include <atomic>
#include <ostream>
#include <source_location>
#include <string>
#include <string_view>
#include <unordered_set>

#include "Validation.hpp"

namespace Physara::RHI::Validation
{
    void Report(MessageCallback *callback, MessageSeverity severity, std::string_view message,
                std::source_location location = std::source_location::current());
    namespace Profile
    {
        bool IsOpenGL(Device *device);
        bool RejectOpenGL(Device *device, std::source_location location = std::source_location::current());
        bool CheckQueue(Device *device, CommandQueue queue,
                        std::source_location location = std::source_location::current());
        bool QueryFeatureSupport(Device *device, Feature feature, void *info = nullptr, size_t infoSize = 0,
                                 std::source_location location = std::source_location::current());
        bool HasExcludedShaderExtensions(const ShaderDesc &desc);
        bool HasExcludedRenderExtensions(const RenderState &state);
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
    namespace Input
    {
        bool Reject(Device *device, const char *reason, const std::string *name = nullptr,
                    std::source_location location = std::source_location::current());
        bool CheckState(Device *device, ResourceStates state, bool allowUnknown = false,
                        std::source_location location = std::source_location::current());
        bool CheckTextureDesc(Device *device, const TextureDesc &desc, bool querySupport = false,
                              std::source_location location = std::source_location::current());
        bool CheckBufferDesc(Device *device, const BufferDesc &desc, bool querySupport = false,
                             std::source_location location = std::source_location::current());
        bool CheckTextureRange(Device *device, Texture *texture, const TextureSubresourceSet &range,
                               bool singleMip = false, std::source_location location = std::source_location::current());
        bool CheckTextureSlice(Device *device, const TextureDesc &desc, const TextureSlice &slice,
                               std::source_location location = std::source_location::current());
        bool CheckTextureCopy(Device *device, const TextureDesc &dest, const TextureSlice &destSlice,
                              const TextureDesc &src, const TextureSlice &srcSlice,
                              std::source_location location = std::source_location::current());
        bool CheckBufferRange(Device *device, Buffer *buffer, uint64_t offset, uint64_t size,
                              std::source_location location = std::source_location::current());
        bool CheckViewFormat(Device *device, const TextureDesc &desc, Format view, FormatSupport required,
                             std::source_location location = std::source_location::current());
        bool CheckBindingLayoutDesc(Device *device, const BindingLayoutDesc &desc,
                                    std::source_location location = std::source_location::current());
    }
    class DeviceWrapper;

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
    };
} // namespace Physara::RHI::Validation

namespace std
{
    template <> struct hash<Physara::RHI::Validation::BindingLocation>
    {
        std::size_t operator()(Physara::RHI::Validation::BindingLocation const &s) const noexcept
        {
            size_t hash = 0;
            Physara::RHI::HashCombine(hash, uint32_t(s.type));
            Physara::RHI::HashCombine(hash, s.registerSpace);
            Physara::RHI::HashCombine(hash, s.slot);
            Physara::RHI::HashCombine(hash, s.arrayElement);
            return hash;
        }
    };
} // namespace std

namespace Physara::RHI::Validation
{
    typedef std::unordered_set<BindingLocation> BindingLocationSet;

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

    Resource *UnwrapResource(Resource *resource);

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

    class CommandListWrapper : public RefCounter<CommandList>
    {
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

        bool RequireOpenState(std::source_location location = std::source_location::current()) const;
        bool RequireExecuteState(std::source_location location = std::source_location::current()) const;
        bool RequireType(CommandQueue queueType, const char *operation) const;
        CommandList *GetUnderlyingCommandList() const
        {
            return m_CommandList;
        }

        void EvaluatePushConstantSize(const Physara::RHI::BindingLayoutVector &bindingLayouts);
        bool ValidatePushConstants(const char *pipelineType, const char *stateFunctionName) const;
        bool ValidateBindingSetsAgainstLayouts(const StaticVector<BindingLayoutHandle, MaxBindingLayouts> &layouts,
                                               const StaticVector<BindingSet *, MaxBindingLayouts> &sets,
                                               std::source_location location = std::source_location::current()) const;
        bool ValidateIndirectBuffer(Buffer *buffer, uint64_t offset, uint64_t count, size_t stride) const;

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

        bool ValidateBindingSetItem(const BindingSetItem &binding, DescriptorTable *pOptDescriptorTable,
                                    std::stringstream &errorStream,
                                    std::source_location location = std::source_location::current());
        bool ValidateBindingSetDesc(const BindingSetDesc &desc, BindingLayout *layout,
                                    std::source_location location = std::source_location::current());
        bool ValidatePipelineBindingLayouts(const StaticVector<BindingLayoutHandle, MaxBindingLayouts> &bindingLayouts,
                                            const std::vector<Shader *> &shaders) const;
        bool ValidateShaderType(ShaderType expected, const ShaderDesc &shaderDesc, const char *function) const;
        bool ValidateRenderState(const RenderState &renderState, FramebufferInfo const &fbinfo) const;

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
