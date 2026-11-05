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
/// @brief 上传限定的逻辑即时列表, 借用 Device 拥有的稳定物理包, 不保存调用方 CPU 源指针.

#include "Backend.hpp"
#include "Submission.hpp"

namespace Physara::RHI::OpenGL
{
    /// @brief 单写者即时列表, GL 在 recording 调用时发出, Execute 只接受提交而不重放.
    /// @note 物理包归 Device, 未提交列表的 worker 最终析构只交接 orphan, 不执行 GL.
    class OpenGLCommandList final : public RefCounter<RHI::CommandList>
    {
    public:
        OpenGLCommandList(OpenGLDevice *device, const CommandListParameters &params) noexcept;
        ~OpenGLCommandList() override;
        /// @brief 取得唯一 recording claim 与物理包; Close 后须成功 Execute 或释放列表才能重新 Open.
        void Open() override;
        /// @brief 结束逻辑录制并保留待提交包, 故障时仍可释放 claim, 不恢复错误包为合法提交.
        void Close() override;
        /// @brief 在有效 recording 中失效后端原生 unpack 缓存, 外部原生互操作后必须调用.
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
        /// @brief 在 owning/current recording 写完整选定 mip/layer/face 或 3D volume, 拒绝深度/模板/MS.
        /// @note CPU 源在返回前消费, caller 保证完整可读 footprint; 单行可 rowPitch=0, 单 depth 忽略 depthPitch.
        void WriteTexture(Texture *dest, uint32_t arraySlice, uint32_t mipLevel, const void *data, size_t rowPitch,
                          size_t depthPitch) override;
        void ResolveTexture(Texture *dest, const TextureSubresourceSet &dstSubresources, Texture *src,
                            const TextureSubresourceSet &srcSubresources) override;
        /// @brief 写受检 Buffer 区间, 非空源在返回前消费, GPU lease 保留至真实完成.
        /// @note dataSize=0 是严格空写, 允许 null 与末尾 offset, 不取得上传容量或 lease.
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
        void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) override;
        void DispatchIndirect(uint32_t offsetBytes) override;
        void SetMeshletState(const MeshletState &state) override;
        void DispatchMesh(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) override;
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
        void ExecuteMultiIndirectClusterOperation(const RT::Cluster::OperationDesc &desc) override;
        void BuildTopLevelAccelStructFromBuffer(RT::AccelStruct *as, Physara::RHI::Buffer *instanceBuffer,
                                                uint64_t instanceBufferOffset, size_t numInstances,
                                                RT::AccelStructBuildFlags buildFlags) override;
        void ConvertCoopVecMatrices(CoopVec::ConvertMatrixLayoutDesc const *convertDescs, size_t numDescs) override;
        void BeginTimerQuery(TimerQuery *query) override;
        void EndTimerQuery(TimerQuery *query) override;
        void BeginMarker(const char *name) override;
        void EndMarker() override;
        /// @brief 控制上传是否自动要求 CopyDest, 不启用当前未实现的 shader-write barrier lowering.
        void SetEnableAutomaticBarriers(bool enable) override;
        void SetResourceStatesForBindingSet(BindingSet *bindingSet) override;
        void SetEnableUavBarriersForTexture(Texture *texture, bool enableBarriers) override;
        void SetEnableUavBarriersForBuffer(Buffer *buffer, bool enableBarriers) override;
        /// @brief 声明资源已知前态, Buffer 重载同义; 保活 extension 后交由单份 Common tracker 管理.
        void BeginTrackingTextureState(Texture *texture, TextureSubresourceSet subresources,
                                       ResourceStates stateBits) override;
        void BeginTrackingBufferState(Buffer *buffer, ResourceStates stateBits) override;
        /// @brief 要求 transfer profile 目标状态, Buffer 重载同义, 生成的抽象依赖须在写入前消费.
        void SetTextureState(Texture *texture, TextureSubresourceSet subresources, ResourceStates stateBits) override;
        void SetBufferState(Buffer *buffer, ResourceStates stateBits) override;
        void SetAccelStructState(RT::AccelStruct *as, ResourceStates stateBits) override;
        void SetPermanentTextureState(Texture *texture, ResourceStates stateBits) override;
        void SetPermanentBufferState(Buffer *buffer, ResourceStates stateBits) override;
        /// @brief 消费已由当前有序 transfer 满足的抽象依赖, 不声明完整 Shader/Draw 同步能力.
        void CommitBarriers() override;
        ResourceStates GetTextureSubresourceState(Texture *texture, ArraySlice arraySlice, MipLevel mipLevel) override;
        ResourceStates GetBufferState(Buffer *buffer) override;
        Device *GetDevice() override;
        const CommandListParameters &GetDesc() override;

    private:
        // 只借出本列表拥有的有效 recording, 同时检查 context, claim 和 sticky tracker 错误.
        [[nodiscard]] UploadWorkPacket *Recording(const char *operation);
        // 核对实际实现和 Device 归属, 不以接口类型相同推定原生对象可用.
        [[nodiscard]] OpenGLBuffer *BufferFor(Buffer *buffer, const char *operation);
        // 取得属于同一 Device 的 Texture wrapper, 失败先报告并返回空.
        [[nodiscard]] OpenGLTexture *TextureFor(Texture *texture, const char *operation);
        // 一个包对一个 wrapper 只建立一份 lease, 跨包 holds 单独计数以支持精确关闭预检.
        [[nodiscard]] bool Retain(Resource *resource, unsigned long &holds);
        // 自动模式要求 CopyDest 并消费依赖; 显式模式核对调用方已完成的同一前提.
        [[nodiscard]] bool TransferState(OpenGLBuffer &buffer);
        // 对选定纹理子资源应用同一 CopyDest 合同, 不把原生缓存命中当成同步完成.
        [[nodiscard]] bool TransferState(OpenGLTexture &texture, TextureSubresourceSet subresources);
        // 保留完整公共命令表的失败出口, 不解引用专有 payload 或发出替代 GPU 工作.
        void Unsupported(const char *operation);
        RefCountPtr<OpenGLDevice> m_Device;
        const CommandListParameters m_Desc;
        UploadWorkPacket *m_Packet = nullptr;
        friend class OpenGLDevice;
    };
}
