# NVRHI · 扩展能力与完整执行示例

[NVRHI 技术目录](../NVRHI_RHI_Vulkan_Analysis.md) · [源码索引](SourceIndex.md#na)

<a id="n11"></a>
## 11. 扩展路径：原生互操作、稀疏资源和光线追踪

### 11.1 模块架构：扩展对象接入同一创建、记录和回收体系

| 路径 | 增加的对象/数据 | 接入既有链路的位置 |
|---|---|---|
| 原生互操作 | native handles、外部 Texture/Buffer | Device 包装、当前 CmdBuf、外部同步 |
| 虚拟资源与 Heap | 未绑定资源、显式内存要求/偏移 | 资源创建后、第一次 GPU 使用前 |
| Tiled Texture | tile 坐标、mip tail、SparseMemoryBind | Queue 的 bindSparse |
| Ray tracing | AS、RT Pipeline、ShaderTable/SBT | Build / 状态跟踪 / dispatchRays |
| Meshlet / VRS | Mesh stage、shading-rate 状态与附件 | Graphics 类 Pipeline 与命令 |
| Cooperative Vector / 厂商几何扩展 | 矩阵布局、扩展几何数据/命令 | Device capability + 专用命令 |

扩展能力不会自动产生上层算法。RT 接口可以执行相交和命中 Shader，但是否用它做阴影、反射或路径追踪由渲染层的 Shader 与 Pass 组织决定。[N20](SourceIndex.md#n20) [N27](SourceIndex.md#n27) [N28](SourceIndex.md#n28) [N29](SourceIndex.md#n29)

### 11.2 原生互操作：操作真实对象也要维护 RHI 状态

getNativeObject 可以获取 Device、Queue、Buffer、Image、Pipeline、DescriptorSet 等原生对象；记录中的 CommandList 也能暴露当前 CommandBuffer。应用可对接外部库，但直接记录命令后应使 NVRHI 对资源状态和执行状态的认知重新一致。[N03](SourceIndex.md#n03) [N06](SourceIndex.md#n06) [N22](SourceIndex.md#n22)

```cpp
cmd.open();
cmd.setTextureState(T, range, externalOperationRequiredState);
cmd.commitBarriers();

VkCommandBuffer native = getCurrentNativeCommandBuffer(cmd);
recordExternalCommands(native, ...);

// 若外部操作改变了 T 的状态，声明其真实结束状态。
// 这是更新跟踪认知，不是补做外部命令的同步。
cmd.beginTrackingTextureState(T, range, actualExternalEndState);
cmd.clearState(); // 按互操作契约重新建立后续所需绑定状态
cmd.setGraphicsState(nextState);
cmd.draw(...);
cmd.close();
```

这个概要要求外部工作遵守同一命令缓冲的合法 scope、资源归属和同步约束。原生 State 与 RHI 缓存若被外部更改，不能只调用下一个 draw 然后期待缓存自动察觉。包装外部 Image 的 managed=false 也只改变销毁归属，不负责 acquire/present semaphore 与外部 ownership 协议。[N06](SourceIndex.md#n06) [N12](SourceIndex.md#n12) [N22](SourceIndex.md#n22)

### 11.3 Virtual、Heap、Tiled 和内存查询是不同能力

普通 isVirtual 资源先创建原生对象，再以 getBuffer/TextureMemoryRequirements 查询大小/对齐，并显式 bind 到 Heap 的合法偏移。Tiled Texture 则使用 sparse flags，由 tile mappings 区分普通图块和 mip tail，通过 Queue::updateTextureTileMappings 生成 bindSparse 工作。[N07](SourceIndex.md#n07) [N14](SourceIndex.md#n14) [N22](SourceIndex.md#n22)

```cpp
resource = createVirtualResource(desc);
requirements = queryRequirements(resource);
heap = createHeap(compatibleHeapDesc);
bindResourceMemory(resource, heap, alignedOffset);

// Tiled Texture 的另一条路径：
tiling = getTextureTiling(tiledTexture);
mappings = translateRequestedTilesToHeapOffsets(tiling, ...);
device.updateTextureTileMappings(tiledTexture, mappings, queue);
```

Feature::VirtualResources 返回真不等于已验证设备具备某种纹理格式的全部 sparse residency 条件；tiled 路径仍受外部启用 feature、队列 sparse 能力和格式要求约束。[N20](SourceIndex.md#n20) [N22](SourceIndex.md#n22)

IResource::queryMemoryRequirements 是可选成员查询，主要针对 Buffer、AS 和 OMM 的 backing buffer。Texture 的此成员查询不支持，Texture 可使用对应 Device 查询；失败时不能把输出当作零。Volatile Vulkan Buffer 返回多版本 backing storage 的需求，结果是 allocation requirements，不是实际驻留、驱动预算或所有池的独占占用。TLAS prebuild 查询则在不分配资源、不提交工作时返回构建需求。[N23](SourceIndex.md#n23)

### 11.4 AS 构建：几何描述先决定容量和存储

```cpp
createAccelStruct(D) {
    sizes = queryBuildSizes(D.geometriesOrMaxInstances, D.buildFlags);
    AS.data = createBuffer(
        sizes.resultBytes, accelStructStorage=true, virtual=D.isVirtual, ...);
    AS.native = vkCreateAccelerationStructureKHR(AS.data, sizes.resultBytes);
    AS.allowUpdate = D.buildFlags.contains(AllowUpdate);
    return AS;
}

buildBlas(AS, geometries, flags) {
    if (flags.contains(PerformUpdate)) assert(AS.allowUpdate);
    nativeGeometry = translateGeometriesAndDeviceAddresses(geometries);
    requireGeometryBuffers(AccelStructBuildInput);
    requireStorageForBuild(AS);
    scratch = scratchManager.suballocate(requiredScratch, currentPendingVersion);
    commitBarriers();
    vkCmdBuildAccelerationStructuresKHR(nativeGeometry, AS, scratch.address, flags);
    retainBuildObjectsAndInputsAccordingToLivenessPolicy(...);
}
```

以上为未走 RTXMU 的主要逻辑概要；RTXMU 构建/压缩走条件分支。BLAS/TLAS backing buffer、scratch 和几何输入的用途不同。TLAS 实例数据可由 UploadManager 写入，也可来自已有 Buffer；BLAS 构建结果再成为 TLAS build 的输入，最终 TLAS 以 AccelStructRead 供 Shader 使用。[N28](SourceIndex.md#n28)

允许 update 与执行 update 分开：创建时容量/flags 必须支持预期操作，执行时不能无限增长几何或实例计数。查询构建需求、分配 AS 和记录构建也处于三个不同阶段。[N23](SourceIndex.md#n23) [N28](SourceIndex.md#n28)

### 11.5 RT Pipeline 与 SBT：从 Shader 组名到设备地址区间

RT Pipeline 由 RayGen/Miss/Callable 等 Shader 和 HitGroup 组合构成。ShaderTable 把逻辑条目关联到 Pipeline 里的 Shader group；Vulkan 后端获取原生 group handle，将其写入满足 shaderGroupBaseAlignment 等要求的 SBT 记录，并填出 rayGen/miss/hit/callable 的 address、stride、size。[N28](SourceIndex.md#n28)

```cpp
setRayTracingState(S) {
    pso = S.shaderTable.pipeline;
    inferBindingResourceStatesIfEnabled(S);
    bindRtPipelineAndDescriptorSetsIfRequired(pso, S);

    tableState = getPerCommandListShaderTableState(S.shaderTable);
    if (tableState.version != S.shaderTable.version) {
        upload = uploadManager.suballocate(
            S.shaderTable.uploadSize, pendingVersion, shaderGroupBaseAlignment);
        S.shaderTable.bake(upload.cpu, effectiveSbtAddress, tableState);
        if (S.shaderTable.isCached)
            copyBuffer(S.shaderTable.cache, upload);
    }
    if (S.shaderTable.isCached)
        prepareInternalCacheForShaderRead();
    retain(S.shaderTable);
    commitBarriers();
}
dispatchRays(width, height, depth) {
    updateRayTracingVolatileBuffers();
    vkCmdTraceRaysKHR(tableState.rayGen, tableState.miss,
                     tableState.hitGroups, tableState.callable,
                     width, height, depth);
}
```

未缓存 SBT 至少按命令列表实例准备；缓存 SBT 允许复制进持久 Buffer，容量受 ShaderTableDesc::maxEntries 约束。它们仍复用 UploadManager 的记录/提交版本和 in-flight 引用。[N28](SourceIndex.md#n28)

Vulkan 路径不支持 DXR 式 local binding layouts/sets；不能把通用接口里可见的 local root signature 能力直接投射成已完成的 Vulkan Shader Record Buffer 数据设计。[N02](SourceIndex.md#n02)

### 11.6 能力枚举还需要落到具体命令路径核实

OMM 在普通 Vulkan 条件分支中有创建/构建；启用 RTXMU 的构建配置会把其 feature support 返回 false。Meshlet 依赖 EXT_mesh_shader，VRS 依赖对应扩展及 attachment feature，SER/cluster/position fetch/cooperative vector 走各自能力条件。[N20](SourceIndex.md#n20) [N27](SourceIndex.md#n27) [N28](SourceIndex.md#n28) [N29](SourceIndex.md#n29)

还存在“可查询到的硬件能力”和“当前每条几何执行分支均已实现”不完全一致的情况：例如 Spheres 有能力查询，但 buildBottomLevelAccelStruct 的 GeometryType::Spheres 分支仍调用 NotImplemented。技术分析应同时追踪 queryFeatureSupport、对象创建和实际命令入口，而不能仅按 Feature 枚举列一份完成度清单。[N20](SourceIndex.md#n20) [N28](SourceIndex.md#n28)

<a id="n12"></a>
## 12. 完整例程：把一次上传、绘制和采样贯穿起来

### 12.1 场景：先画到 A，再把 A 作为纹理画到 B

这个例程只说明已有接口与实际内部行为，不规定上层渲染器采用何种框架。为突出执行链，两个 Pass 都在 Graphics 同一队列，输出为离屏 B；原生 SwapChain 的 acquire/present 由外部系统负责。

| 对象 | 创建约定 | 在本例中的用途 |
|---|---|---|
| VB / IB | device-local；keepInitialState=Vertex/Index | 先上传，再绘制 |
| PerDraw | volatile ConstantBuffer；足够 maxVersions | 连续多个 Draw 的不同参数 |
| A | 可 RT、可 SRV；keepInitialState=ShaderResource | Pass 1 写，Pass 2 读 |
| B | 可 RT、可 SRV；keepInitialState=ShaderResource | Pass 2 写，结束后可采样 |
| Set1 | PerDraw 与 Pass 1 所需纹理/采样器 | 第一遍 Shader 输入 |
| Set2 | PerDraw、A 的 SRV、采样器 | 第二遍 Shader 输入 |
| PSO1 / PSO2 | 各自 Shader/Layout、A/B 兼容信息 | 固定管线执行状态 |
| FB1 / FB2 | A/B 的附件视图 | 本次 dynamic rendering 目标 |

所有未显式省略的资源必须具有与实际使用一致的用途/布局；本例涉及的清除和 Draw 不会替代对布局与数据结构 ABI 的正确配置。

### 12.2 公共调用和内部动作逐行对齐

```cpp
// 对象创建阶段。以下函数名简写代表相应公共 create* 调用。
VB = createBuffer(vertexDescWithKeepInitialVertexState);
IB = createBuffer(indexDescWithKeepInitialIndexState);
PerDraw = createVolatileConstantBuffer(size, maxVersions);
A = createTexture(rtSrvDescWithKeepInitialShaderReadState);
B = createTexture(rtSrvDescWithKeepInitialShaderReadState);
FB1 = createFramebuffer(colorAttachment=A);
FB2 = createFramebuffer(colorAttachment=B);
layout = createBindingLayout(volatileCB_b0, texture_t0, sampler_s0);
Set1 = createBindingSet(layout, PerDraw, sourceTexture, sampler);
Set2 = createBindingSet(layout, PerDraw, A, sampler);
PSO1 = createGraphicsPipeline(shader1, layout, FB1.info);
PSO2 = createGraphicsPipeline(shader2, layout, FB2.info);

// 第一次内容准备与两个 Pass。
cmd.open();                              // 新 recordingID + 物理命令缓冲
cmd.writeBuffer(VB, vertices);            // CopyDest + update/copy
cmd.writeBuffer(IB, indices);

cmd.clearTextureFloat(A, all, clearA);    // 初始化 A 的内容
cmd.writeBuffer(PerDraw, object0);        // 占用常量版本 v0
cmd.setGraphicsState({PSO1, FB1, Set1, VB, IB, viewport});
    // VB/IB: CopyDest -> Vertex/Index
    // A: CopyDest(清除) -> RenderTarget
    // beginRendering(FB1)，绑定 descriptor + offset(v0)
cmd.drawIndexed(draw0);

cmd.writeBuffer(PerDraw, object1);        // 占用 v1；允许在 rendering 中
cmd.drawIndexed(draw1);
    // updateGraphicsVolatileBuffers 自动绑定 offset(v1)

cmd.clearTextureFloat(B, all, clearB);    // 结束现有 rendering，准备 B
cmd.writeBuffer(PerDraw, fullscreenData);
cmd.setGraphicsState({PSO2, FB2, Set2, fullscreenGeometry, viewport});
    // A: RenderTarget -> ShaderResource
    // B: CopyDest -> RenderTarget
    // 依赖提交后 beginRendering(FB2)
cmd.draw(fullscreenArgs);

cmd.close();
    // endRendering
    // A/B 恢复 ShaderResource（若已经符合则不需状态变更）
    // flush 所写 volatile 版本范围；结束原生记录

id = device.executeCommandList(cmd, Graphics);
    // Queue Timeline signal=id
    // pending upload/volatile/scratch token -> submitted token
    // 引用进入 in-flight tracker

// CPU 可以准备后续工作。
device.runGarbageCollection();
    // 只有 Timeline>=id 才清除该提交的在途引用、回收物理列表。
```

结构体组合、清除参数和资源创建的部分样板被简写；这里的目的在于对应公共调用与第 3—10 节的真实动作，不是可直接编译的完整示例。

### 12.3 同一资源的状态线与生存期线

| 时间点 | A 的用途/布局变化 | A 的生存期证据 | 常量版本的归属 |
|---|---|---|---|
| 创建后 | 首次未初始化；keepInitialState 有约定 | 应用/Framebuffer/BindingSet 引用 | 全部可用 |
| Clear A | Common → CopyDest | 命令记录开始持有引用 | v0 被 recordingID 占用 |
| Pass 1 | CopyDest → RenderTarget | 当前物理 CmdBuf 保活依赖 | v0/v1 都不能重写 |
| Pass 2 | RenderTarget → ShaderResource | Set2 与当前记录持有依赖 | fullscreen 版本也占用 |
| close | 回到约定初始状态 | 关闭的记录仍保有引用 | pending，未提交也不可复用 |
| execute | 工作进入队列 | LifetimeTracker 持有物理 CmdBuf | 改为 queue+submissionID |
| GPU 完成/GC | 图像已可按结束状态使用 | 清除该提交的在途引用 | 完成值满足后允许再次占用 |

Barrier 决定“这个访问如何看见前一个访问”；Timeline 决定“这批访问是否已经执行完成”；Handle/引用决定“原生对象能否被销毁”。上述例子让三者共享一条时间线，同时保持各自责任清楚。

### 12.4 与 Filament 分析的衔接位置

| Filament 渲染链产物 | NVRHI 公共层的同类输入 | 本文向下追踪到的实现 |
|---|---|---|
| 已排好序的 Draw 与变体 | Pipeline + GraphicsState + DrawArguments | PipelineLayout、绑定缓存、vkCmdDraw* |
| PerView/Material/Renderable 数据 | 常量 Buffer / BindingSet / 动态偏移 | 上传 chunk、volatile 版本、descriptor |
| FrameGraph Pass 的目标与读写 | Framebuffer、SRV/UAV、状态请求 | dynamic rendering、Barrier2 |
| 帧结束后的资源复用需求 | execute 返回 ID、GC、query | Timeline、在途引用、版本回收 |

这两份源码的分界能解释一帧中的连续因果关系：渲染层先组织工作与数据，RHI 将其变为原生命令和依赖，GPU 执行后产生的完成值再驱动 CPU 资源复用。具体项目各有自己的接口和实现，阅读时以这里的职责关系连接，以各自源码验证细节。
