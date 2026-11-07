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
/// @brief 基线图形/传输的逻辑即时列表, 借用 Device 稳定物理包, 不保存调用方 CPU 源或命令重放记录.

#include "Backend.hpp"
#include "Submission.hpp"

namespace Physara::RHI::OpenGL
{
    class OpenGLGraphicsPipeline;
    struct PlannedResource;
    struct NativeBinding;
    /// @brief 单写者即时列表, GL 在 recording 调用时发出, Execute 只接受提交而不重放.
    /// @note 物理包归 Device, 未提交列表的 worker 最终析构只交接 orphan, 不执行 GL.
    /// 逻辑状态只借用对象, 默认由物理包 lease 保活; trackLiveness=false 的集合由调用方保活到完成.
    class OpenGLCommandList final : public RefCounter<RHI::CommandList>
    {
    public:
        OpenGLCommandList(OpenGLDevice *device, const CommandListParameters &params) noexcept;
        ~OpenGLCommandList() override;
        /// @brief 取得唯一 recording claim 与物理包; Close 后须成功 Execute 或释放列表才能重新 Open.
        void Open() override;
        /// @brief 结束逻辑录制并保留待提交包, 故障时仍可释放 claim, 不恢复错误包为合法提交.
        void Close() override;
        /// @brief 清空逻辑图形状态并失效唯一原生缓存, 保留 tracker 和已经发出工作的引用.
        void ClearState() override;
        /// @brief 完整清除合法二维选层, 忽略当前 scissor/mask, float 与 UInt 入口各自核对数值类型.
        void ClearTextureFloat(Texture *t, TextureSubresourceSet subresources, const Color &clearColor) override;
        /// @brief 完整清除选层的指定 depth/stencil aspect, 保留另一 aspect 和未选子资源.
        void ClearDepthStencilTexture(Texture *t, TextureSubresourceSet subresources, bool clearDepth, float depth,
                                      bool clearStencil, uint8_t stencil) override;
        void ClearTextureUInt(Texture *t, TextureSubresourceSet subresources, uint32_t clearColor) override;
        /// @brief 同 format/单样本二维 slice 精确 1:1 copy, 不重采样, 翻转, 转换或裁剪越界请求.
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
        /// @brief 按受检 byte interval 复制普通 Buffer, 同 storage 非法重叠在 native 前拒绝.
        void CopyBuffer(Buffer *dest, uint64_t destOffsetBytes, Buffer *src, uint64_t srcOffsetBytes,
                        uint64_t dataSizeBytes) override;
        void ClearSamplerFeedbackTexture(SamplerFeedbackTexture *texture) override;
        void DecodeSamplerFeedbackTexture(Buffer *buffer, SamplerFeedbackTexture *texture,
                                          Physara::RHI::Format format) override;
        void SetSamplerFeedbackTextureState(SamplerFeedbackTexture *texture, ResourceStates stateBits) override;
        void SetPushConstants(const void *data, size_t byteSize) override;
        /// @brief 预检同设备的完整候选, 先保活再应用; 对象/参数预检失败不替换已有逻辑状态.
        /// @note tracker 或原生错误会使 recording 失效, 已发前缀仍按原包退休, 不能据此继续绘制.
        void SetGraphicsState(const GraphicsState &state) override;
        /// @brief 即时消费当前用途和状态, indexed 的 startVertexLocation 只作为一次 baseVertex.
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
        /// @brief 选择由命令推导或调用方显式提供图形/传输用途, 两者共用 tracker 与 CommitBarriers.
        void SetEnableAutomaticBarriers(bool enable) override;
        /// @brief 按布局可见阶段请求集合资源的用途, 不绑定资源或隐式 Commit; 集合只在本调用期间借用.
        void SetResourceStatesForBindingSet(BindingSet *bindingSet) override;
        void SetEnableUavBarriersForTexture(Texture *texture, bool enableBarriers) override;
        void SetEnableUavBarriersForBuffer(Buffer *buffer, bool enableBarriers) override;
        /// @brief 声明资源已知前态, Buffer 重载同义; 保活 extension 后交由单份 Common tracker 管理.
        void BeginTrackingTextureState(Texture *texture, TextureSubresourceSet subresources,
                                       ResourceStates stateBits) override;
        void BeginTrackingBufferState(Buffer *buffer, ResourceStates stateBits) override;
        /// @brief 请求已实现的图形/传输用途, Buffer 重载同义, 显式模式须在实际消费前 Commit.
        void SetTextureState(Texture *texture, TextureSubresourceSet subresources, ResourceStates stateBits) override;
        void SetBufferState(Buffer *buffer, ResourceStates stateBits) override;
        void SetAccelStructState(RT::AccelStruct *as, ResourceStates stateBits) override;
        void SetPermanentTextureState(Texture *texture, ResourceStates stateBits) override;
        void SetPermanentBufferState(Buffer *buffer, ResourceStates stateBits) override;
        /// @brief 由同一权威消费有序 transfer/附件与只读 Shader 依赖, 不扩展 Shader 写入能力.
        void CommitBarriers() override;
        ResourceStates GetTextureSubresourceState(Texture *texture, ArraySlice arraySlice, MipLevel mipLevel) override;
        ResourceStates GetBufferState(Buffer *buffer) override;
        Device *GetDevice() override;
        const CommandListParameters &GetDesc() override;

    private:
        /// @brief 只借出本列表拥有的有效 recording, 同时检查 context, claim 和 sticky tracker 错误.
        [[nodiscard]] UploadWorkPacket *Recording(const char *operation);
        /// @brief 核对实际实现和 Device 归属, 不以接口类型相同推定原生对象可用.
        [[nodiscard]] OpenGLBuffer *BufferFor(Buffer *buffer, const char *operation);
        /// @brief 取得属于同一 Device 的 Texture wrapper, 失败先报告并返回空.
        [[nodiscard]] OpenGLTexture *TextureFor(Texture *texture, const char *operation);
        /// @brief 一个包对一个 wrapper 只建立一份 lease, 跨包 holds 单独计数以支持精确关闭预检.
        [[nodiscard]] bool Retain(Resource *resource, InternalOwnership &owners);
        /// @brief 已验证的原生复合对象按实际依赖展开一次, 返回稳定账目; nullptr 表示准备失败.
        [[nodiscard]] InternalOwnership *RetainResource(Resource *resource);
        /// @brief 自动模式要求 CopyDest 并消费依赖; 显式模式核对调用方已完成的同一前提.
        [[nodiscard]] bool TransferState(OpenGLBuffer &buffer);
        // 对选定纹理子资源应用同一 CopyDest 合同, 不把原生缓存命中当成同步完成.
        [[nodiscard]] bool TransferState(OpenGLTexture &texture, TextureSubresourceSet subresources);
        /// @brief 在自动模式请求用途, 显式模式核对已提交用途; 两者消费同一份 Common 依赖.
        [[nodiscard]] bool UseBuffer(OpenGLBuffer &buffer, ResourceStates state, const char *operation);
        [[nodiscard]] bool UseTexture(OpenGLTexture &texture, TextureSubresourceSet range, ResourceStates state,
                                      const char *operation);
        /// @brief 本 profile 没有 Shader 写生产者, 有序访问满足的依赖在此唯一消费.
        [[nodiscard]] bool ConsumeBarriers();
        /// @brief 全量解析本次二维传输范围, 先拒绝显式越界, 不把无效请求裁成合法范围.
        [[nodiscard]] bool TransferRange(OpenGLTexture &texture, TextureSubresourceSet requested,
                                         TextureSubresourceSet &resolved, const char *operation);
        /// @brief 两种颜色入口共用完整子资源准备与 typed clear, integer 只由公开重载选择.
        void ClearColorTexture(Texture *texture, TextureSubresourceSet range, const Color &color,
                               std::uint32_t integerValue, bool integer);
        /// @brief 候选状态的公开输入边界, 不修改当前逻辑状态或 GL.
        [[nodiscard]] bool ValidateGraphicsState(const GraphicsState &state, OpenGLGraphicsPipeline *&pipeline,
                                                 OpenGLFramebuffer *&framebuffer);
        /// @brief 由候选验证和不可变计划保证该逻辑项存在, 不在 Draw 重新解析布局/反射.
        [[nodiscard]] const NativeBinding &BindingFor(const GraphicsState &state, const PlannedResource &entry) const;
        /// @brief 按当前计划和具体附件核对图形用途; 显式用途不满足或 tracker 错误时使 recording 失效.
        [[nodiscard]] bool GraphicsUses(const GraphicsState &state, const OpenGLGraphicsPipeline &pipeline,
                                        const char *operation);
        /// @brief 调用方已满足用途及 BeginNative, 本函数只通过唯一 ContextState 应用绑定/普通状态.
        void ApplyGraphics(const GraphicsState &state, const OpenGLGraphicsPipeline &pipeline);
        /// @brief 验证本次参数/可知数据范围, 恢复实际用途与缓存后发出一个直接 Draw.
        void DrawDirect(const DrawArguments &args, bool indexed);
        /// @brief 保留完整公共命令表的失败出口, 不解引用专有 payload 或发出替代 GPU 工作.
        void Unsupported(const char *operation);
        RefCountPtr<OpenGLDevice> m_Device;
        const CommandListParameters m_Desc;
        UploadWorkPacket *m_Packet = nullptr;
        friend class OpenGLDevice;
    };
}
