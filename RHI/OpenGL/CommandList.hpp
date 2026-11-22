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
/// @brief 图形, 计算和传输共用 Graphics 即时列表, 录制时借用物理包并把命令交给当前 context.

#include <optional>

#include "Backend.hpp"
#include "ImmediateGraphics.hpp"
#include "Submission.hpp"

namespace Physara::RHI::OpenGL
{
    class OpenGLGraphicsPipeline;
    struct PlannedResource;
    struct PipelineBindingPlan;
    class OpenGLComputePipeline;
    class OpenGLStagingTexture;
    struct NativeBinding;
    /// @brief 单写者即时列表, GL 在 recording 调用时发出, Execute 只接受提交而不重放.
    /// @note 物理包归 Device, 未提交列表的 worker 最终析构只交接 orphan, 不执行 GL.
    /// 逻辑状态只借用对象, 默认由物理包 lease 保活; trackLiveness=false 的集合由调用方保活到完成.
    class OpenGLCommandList final : public RefCounter<RHI::CommandList>, public ImmediateGraphicsRecording
    {
    public:
        OpenGLCommandList(OpenGLDevice *device, const CommandListParameters &params) noexcept;
        ~OpenGLCommandList() override;
        /// @brief 取得唯一 recording claim 与物理包; Close 后须成功 Execute 或释放列表才能重新 Open.
        void Open() override;
        /// @brief 结束逻辑录制并保留待提交包, 未配对 Timer 使该包失效, 不自动补结束点.
        /// @note 可用 context 上的故障 Close 仍释放 claim; context 已不可用时由列表 Release 交接退出.
        void Close() override;
        /// @brief 撤销 Draw 和 Dispatch 候选并失效原生缓存. 已发工作及其资源保活随当前包继续推进.
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
        /// @brief 读回只写目标矩形, 选层起点和行距由 Map 返回. 本次复制所属包在提交并完成后才可发布数据.
        void CopyTexture(StagingTexture *dest, const TextureSlice &destSlice, Texture *src,
                         const TextureSlice &srcSlice) override;
        /// @brief 从已 Unmap 的 Write staging 上传同格式矩形. 录制包占用整份 staging 存储直到退休.
        void CopyTexture(Texture *dest, const TextureSlice &destSlice, StagingTexture *src,
                         const TextureSlice &srcSlice) override;
        /// @brief 在 owning/current recording 写完整选定 mip/layer/face 或 3D volume, 拒绝深度/模板/MS.
        /// @note CPU 源在返回前消费, caller 保证完整可读 footprint; 单行可 rowPitch=0, 单 depth 忽略 depthPitch.
        void WriteTexture(Texture *dest, uint32_t arraySlice, uint32_t mipLevel, const void *data, size_t rowPitch,
                          size_t depthPitch) override;
        /// @brief 逐层将多样本颜色附件归约到同尺寸的单样本目标 mip, 两端可有不同的起始层.
        void ResolveTexture(Texture *dest, const TextureSubresourceSet &dstSubresources, Texture *src,
                            const TextureSubresourceSet &srcSubresources) override;
        [[nodiscard]] bool GenerateMipmaps(Texture *texture, TextureSubresourceSet range) override;
        /// @brief 写受检 Buffer 区间, 非空源在返回前消费, GPU lease 保留至真实完成.
        /// @note dataSize=0 是严格空写, 允许 null 与末尾 offset, 不取得上传容量或 lease.
        void WriteBuffer(Buffer *b, const void *data, size_t dataSize, uint64_t destOffsetBytes) override;
        void ClearBufferUInt(Buffer *b, uint32_t clearValue) override;
        /// @brief 复制两个普通 Buffer 的字节区间, 或在普通 Buffer 和一个方向匹配的 staging 之间复制.
        /// @note 同一存储的重叠区间在发出前拒绝. 图内的 Buffer 读回从图尾导出后在调用方录制.
        void CopyBuffer(Buffer *dest, uint64_t destOffsetBytes, Buffer *src, uint64_t srcOffsetBytes,
                        uint64_t dataSizeBytes) override;
        void ClearSamplerFeedbackTexture(SamplerFeedbackTexture *texture) override;
        void DecodeSamplerFeedbackTexture(Buffer *buffer, SamplerFeedbackTexture *texture,
                                          Physara::RHI::Format format) override;
        void SetSamplerFeedbackTextureState(SamplerFeedbackTexture *texture, ResourceStates stateBits) override;
        void SetPushConstants(const void *data, size_t byteSize) override;
        /// @brief 预检同设备的完整候选, 先保活再应用. 图内在准备新候选前撤销绘制资格, 成功发布后才恢复.
        /// @note 资源保活失败发生在候选发布前. 跟踪或原生错误使录制失效, 已发前缀仍按原包退休.
        void SetGraphicsState(const GraphicsState &state) override;
        /// @brief 即时消费当前用途和状态, indexed 的 startVertexLocation 只作为一次 baseVertex.
        /// @note 图内按声明检查已知顶点, 索引和实例区间. 索引内容对应的顶点范围由数据提供方保证.
        void Draw(const DrawArguments &args) override;
        void DrawIndexed(const DrawArguments &args) override;
        void DrawIndirect(uint32_t offsetBytes, uint32_t drawCount) override;
        /// @brief 从当前 indirectParams 的 4 字节对齐偏移读取 drawCount 条紧排 indexed 记录.
        /// @pre 录制已建立图形状态和起点为 0 的 R16/R32 索引绑定. 生产者保证记录内的几何与实例地址有效.
        /// @note 合法零条调用直接返回. 非空范围按本次录制模式核对用途后即时发出一次原生调用.
        void DrawIndexedIndirect(uint32_t offsetBytes, uint32_t drawCount) override;
        void DrawIndexedIndirectCount(uint32_t paramOffsetBytes, uint32_t countOffsetBytes,
                                      uint32_t maxDrawCount) override;
        /// @brief 在当前录制中建立单 CS 候选并替换图形执行资格. 该调用只准备绑定和保活.
        /// @note 图内先撤销旧候选. 开启 trackLiveness 的集合由包保活, 关闭时由调用方保留至完成.
        void SetComputeState(const ComputeState &state) override;
        /// @brief 在设备逐轴上限内即时调度工作组. 有效非空调度在发出前核对动态常量, 实际用途和范围.
        /// @note 任一轴为零且其余参数合法时直接返回. 组内尺寸由 Pipeline 的 CS 固定.
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
        /// @brief 同 recording 的时间戳对可与其它 Timer 重叠; 同 query 一次 recording 只使用一次.
        /// @note 新使用只接受 Idle/Ready; 后继 recording 的 Begin 消费 Ready, 不释放旧包的 lease.
        /// 未解析使用不可覆盖, 未配对 Begin 使 Close 后的正常提交无效, 已发点仍保活.
        void BeginTimerQuery(TimerQuery *query) override;
        /// @brief 只结束本 recording 已成功 Begin 的查询; 错误配对在新时间点之前拒绝.
        void EndTimerQuery(TimerQuery *query) override;
        void BeginMarker(const char *name) override;
        void EndMarker() override;
        /// @brief 选择命令推导或调用方显式准备资源用途. 两种模式共用 tracker 和实际消费的屏障路径.
        void SetEnableAutomaticBarriers(bool enable) override;
        /// @brief 按布局可见阶段请求集合资源的用途, 不绑定资源或隐式 Commit; 集合只在本调用期间借用.
        void SetResourceStatesForBindingSet(BindingSet *bindingSet) override;
        /// @brief 调整 tracker 的 UAV 请求策略. 已知重叠的原生访问仍按实际读写和后继域同步. 两个重载同义.
        void SetEnableUavBarriersForTexture(Texture *texture, bool enableBarriers) override;
        void SetEnableUavBarriersForBuffer(Buffer *buffer, bool enableBarriers) override;
        /// @brief 声明资源已知前态, Buffer 重载同义; 保活 extension 后交由单份 Common tracker 管理.
        void BeginTrackingTextureState(Texture *texture, TextureSubresourceSet subresources,
                                       ResourceStates stateBits) override;
        void BeginTrackingBufferState(Buffer *buffer, ResourceStates stateBits) override;
        /// @brief 为纹理或整 Buffer 请求后继用途. 显式模式须在使用前提交待决状态.
        void SetTextureState(Texture *texture, TextureSubresourceSet subresources, ResourceStates stateBits) override;
        void SetBufferState(Buffer *buffer, ResourceStates stateBits) override;
        void SetAccelStructState(RT::AccelStruct *as, ResourceStates stateBits) override;
        void SetPermanentTextureState(Texture *texture, ResourceStates stateBits) override;
        void SetPermanentBufferState(Buffer *buffer, ResourceStates stateBits) override;
        /// @brief 提交公共 tracker 的待决用途. 后继实际命令按其访问范围和原生域决定所需的 GL 屏障.
        void CommitBarriers() override;
        ResourceStates GetTextureSubresourceState(Texture *texture, ArraySlice arraySlice, MipLevel mipLevel) override;
        ResourceStates GetBufferState(Buffer *buffer) override;
        Device *GetDevice() override;
        const CommandListParameters &GetDesc() override;

    private:
        /// @brief 接管同设备且无待决依赖的 Automatic 录制. 归还后原 owner 继续持有包.
        [[nodiscard]] std::uint64_t BeginGraphManaged(Device &device) noexcept override;
        /// @brief 按匹配的身份归还接管并清除本地渲染状态和阶段授权, 包内 tracker 与引用继续存活.
        [[nodiscard]] bool EndGraphManaged(std::uint64_t recordingId, bool executionSucceeded) noexcept override;
        /// @brief 由接管者登记前态, 请求已合并状态, 并提交或清除独立的消费授权.
        void TrackGraphTexture(Texture &texture, TextureSubresourceSet range, ResourceStates state) override;
        void TrackGraphBuffer(Buffer &buffer, ResourceStates state) override;
        void RequestGraphTexture(Texture &texture, TextureSubresourceSet range, ResourceStates state) override;
        void RequestGraphBuffer(Buffer &buffer, ResourceStates state) override;
        void CommitGraphUses(std::span<const GraphTextureUse> textures, std::span<const GraphBufferUse> buffers) override;
        void RevokeGraphUses() noexcept override;
        void RevokeGraphPipelineState() noexcept override;
        /// @brief 只借出本列表拥有的有效 recording, 同时检查 context, claim 和 sticky tracker 错误.
        [[nodiscard]] UploadWorkPacket *Recording(const char *operation);
        /// @brief 在用途来源匹配时准许修改 tracker. 普通列表在 GraphManaged 只消费授权.
        [[nodiscard]] UploadWorkPacket *StateRecording(const char *operation, bool graphRequest = false);
        /// @brief 公共和图入口共用实际初态登记与状态请求, 修改前确认用途来源.
        void SetTrackedBufferState(Buffer *buffer, ResourceStates state, bool initial, bool graphRequest);
        void SetTrackedTextureState(Texture *texture, TextureSubresourceSet range, ResourceStates state,
                                    bool initial, bool graphRequest);
        /// @brief 串行 owner 上借用本列表当前持有的物理录制, 含已失效但仍须收拢的包.
        [[nodiscard]] UploadWorkPacket *OwningRecording() const noexcept;
        /// @brief 撤销两类执行资格和 push, 保留本次录制建立的 viewport/scissor.
        void ResetPipelineState() noexcept;
        /// @brief 清除两类执行候选, push 和图形动态状态, 并使原生缓存失效. 已发命令仍由包持有.
        void ResetGraphicsState() noexcept;
        /// @brief 核对实际实现和 Device 归属, 不以接口类型相同推定原生对象可用.
        [[nodiscard]] OpenGLBuffer *BufferFor(Buffer *buffer, const char *operation);
        /// @brief 取得属于同一 Device 的 Texture wrapper, 失败先报告并返回空.
        [[nodiscard]] OpenGLTexture *TextureFor(Texture *texture, const char *operation);
        /// @brief 一个包对一个 wrapper 只建立一份 lease, 跨包 holds 单独计数以支持精确关闭预检.
        [[nodiscard]] bool Retain(Resource *resource, InternalOwnership &owners);
        /// @brief 核对方向对应的可用性, 在包内取得或复用 lease. 准备本身保留旧结果.
        /// @return 返回值借自包内 vector, 仅在准备下一份 lease 前使用.
        [[nodiscard]] UploadWorkPacket::StagingLease *PrepareStagingUse(StagingAccess &access, bool &ready,
                                                                       CpuAccessMode mode, const char *operation);
        /// @brief 在可能发出原生复制前占用 backing, 同时撤销 Read 目标的旧就绪标记.
        void RecordStagingUse(UploadWorkPacket::StagingLease &lease) noexcept;
        void CopyStaging(StagingTexture *staging, const TextureSlice &stagingSlice, Texture *texture,
                         const TextureSlice &textureSlice, CpuAccessMode mode);
        /// @brief 为已预检的源和目标各选一个附件并执行 blit. epoch 由调用方在同一批 blit 中复用.
        [[nodiscard]] bool BlitColor(OpenGLTexture &dest, std::uint32_t destMip, std::uint32_t destLayer,
                                     OpenGLTexture &src, std::uint32_t srcMip, std::uint32_t srcLayer,
                                     GLenum filter, const char *operation, std::uint64_t &epoch);
        /// @brief 已验证的原生复合对象按实际依赖展开一次, 返回稳定账目; nullptr 表示准备失败.
        [[nodiscard]] InternalOwnership *RetainResource(Resource *resource);
        /// @brief 自动模式请求状态. 图模式核对已提交范围, 无 range 时只检查该 Buffer 能否绑定到指定角色.
        /// @param structuredBinding 核对结构化 SRV 的原生绑定时为 true, 允许匹配本 Pass 另行声明的表绑定范围.
        [[nodiscard]] bool UseBuffer(OpenGLBuffer &buffer, ResourceStates state, const char *operation,
                                     std::optional<BufferRange> range = std::nullopt, bool structuredBinding = false,
                                     StorageAccess storage = StorageAccess::None, GLbitfield domain = 0,
                                     bool replaces = false);
        /// @brief 纹理消费按已解析的底层 mip/layer 与实际分量核对授权, 与跟踪粒度独立.
        [[nodiscard]] bool UseTexture(OpenGLTexture &texture, TextureSubresourceSet range, TextureAspect aspects,
                                      ResourceStates state, const char *operation,
                                      ResourceStates access = ResourceStates::Unknown,
                                      StorageAccess storage = StorageAccess::None, GLbitfield domain = 0,
                                      bool replaces = false);
        /// @brief Draw 计算出的精确区间沿用传输和绑定的 GraphManaged 授权规则.
        [[nodiscard]] bool CheckGraphBufferUse(const OpenGLBuffer &buffer, ResourceStates state,
                                               const char *operation, std::optional<BufferRange> range,
                                               bool structuredBinding = false, StorageAccess storage = StorageAccess::None);
        /// @brief 预留实际访问的范围历史, 合并待满足的原生消费域. 命令仍未发出.
        [[nodiscard]] bool PrepareNativeAccesses(const char *operation);
        /// @brief 向当前命令暂存一个带单一 GL 域位的实际范围. 准备存储失败时报告并返回 false.
        [[nodiscard]] bool AddNativeAccess(NativeAccess access, const char *operation);
        /// @brief 发布已发原生命令的访问, 在本调用中不再分配存储.
        void RecordNativeAccesses() noexcept;
        /// @brief 一次 FBO clear 成功后登记已经完全覆盖的层和 mip. 其它子资源保留其写入历史.
        void RecordClearedSubresource(OpenGLTexture &texture, std::uint32_t layer, std::uint32_t mip) noexcept;
        /// @brief 提交公共 tracker 的用途. 实际命令依赖由 PrepareNativeAccesses 处理.
        [[nodiscard]] bool ConsumeBarriers();
        /// @brief 在实际联合采样的 Draw 前, 由同一发射路径满足重叠的先前附件写.
        [[nodiscard]] bool PrepareDepthSampling(const GraphicsState &state, const OpenGLGraphicsPipeline &pipeline);
        /// @brief 判断启用的测试与写掩码是否允许修改深度或模板, 供只读附件预检使用.
        [[nodiscard]] static bool WritesDepthStencil(const DepthStencilState &state) noexcept;
        /// @brief 以原存储和已解析的 mip/layer 判断采样与附件是否相交.
        [[nodiscard]] static bool SamplesAttachment(const NativeBinding &binding,
                                                    const FramebufferAttachment &attachment) noexcept;
        /// @brief 发出实际修改深度分量的 Draw 或 clear 后登记写. 范围引用底层物理 mip.
        void RecordDepthWrite(OpenGLTexture &texture, TextureSubresourceSet subresources) noexcept;
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
        [[nodiscard]] const NativeBinding &BindingFor(const BindingSetVector &bindings, const PlannedResource &entry) const;
        /// @brief 按当前计划和具体附件核对图形用途; 显式用途不满足或 tracker 错误时使 recording 失效.
        [[nodiscard]] bool GraphicsUses(const GraphicsState &state, const OpenGLGraphicsPipeline &pipeline,
                                        const char *operation, bool executed = false);
        /// @brief 调用方已满足用途及 BeginNative, 本函数只通过唯一 ContextState 应用绑定/普通状态.
        void ApplyGraphics(const GraphicsState &state, const OpenGLGraphicsPipeline &pipeline);
        /// @brief 在 Draw 或 Dispatch 前核对当前 push 区段和本 recording 的 volatile 常量版本.
        [[nodiscard]] bool CheckConstants(const BindingSetVector &bindings, const PipelineBindingPlan &plan,
                                              const char *operation) const;
        /// @brief 按本次执行选出的版本刷新动态 UBO range. 调用前已通过 CheckConstants.
        void ApplyConstants(const BindingSetVector &bindings, const PipelineBindingPlan &plan);
        /// @brief 按当前 Program 计划应用普通缓冲范围, 存储图像及组合采样单元. 动态常量另按版本绑定.
        void ApplyProgramBindings(const BindingSetVector &bindings, const PipelineBindingPlan &plan);
        /// @brief 为本次调度逐项核对活动范围及图授权, 收集需要发射原生依赖的访问.
        [[nodiscard]] bool ComputeUses(const BindingSetVector &bindings, const PipelineBindingPlan &plan);
        /// @brief 验证计算候选的集合身份, 活动块布局和同存储别名. 失败时尚未发出调度.
        [[nodiscard]] bool ValidateComputeBindings(const ComputeState &state, const OpenGLComputePipeline &pipeline);
        /// @brief 占用一个可写版本并复制逻辑前缀. 槽耗尽时报告原因, 先前已写版本继续由包持有.
        void WriteVolatile(OpenGLBuffer &buffer, const void *data, std::size_t size);
        /// @brief 验证本次参数/可知数据范围, 恢复实际用途与缓存后发出一个直接 Draw.
        void DrawDirect(const DrawArguments &args, bool indexed);
        /// @brief 从当前有效 recording 取得可用于本次 Draw 的管线, 缺少必要状态时报告失败.
        [[nodiscard]] const OpenGLGraphicsPipeline *DrawPipeline(bool indexed, const char *operation);
        /// @brief 直接与间接 Draw 共用的实际用途, 附件依赖和动态常量准备.
        [[nodiscard]] bool PrepareDraw(const OpenGLGraphicsPipeline &pipeline, const char *operation);
        /// @brief 核对发出的 Draw, 成功时登记实际深度写入, 否则保留已发包并使其失效.
        void FinishDraw(const OpenGLGraphicsPipeline &pipeline, const char *operation);
        /// @brief 保留完整公共命令表的失败出口, 不解引用专有 payload 或发出替代 GPU 工作.
        void Unsupported(const char *operation);
        RefCountPtr<OpenGLDevice> m_Device;
        const CommandListParameters m_Desc;
        UploadWorkPacket *m_Packet = nullptr;
        friend class OpenGLDevice;
    };
}
