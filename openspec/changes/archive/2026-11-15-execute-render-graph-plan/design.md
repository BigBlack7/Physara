# Design

## Context

见[提案](proposal.md#why)。`CompiledGraphPlan` 已提供存活顺序、资源映射、槽描述、附件、清除和出口，原图持有导入句柄及回调数据。`InvokePass` 用当前声明约束 `PassResources`。OpenGL 即时列表已经有 Automatic 和显式用途入口，`ConsumeBarriers` 当前按有序传输与附件 profile 消费依赖，普通图用途中也已有只读深度联合采样的能力键。执行阶段需要把这些能力连接起来，并区分录制有效性与 GPU 完成。 [计划](../../../../Engine/Renderer/RenderGraph/GraphCompiler.hpp) · [权限与回调](../../../../Engine/Renderer/RenderGraph/RenderGraph.hpp) · [编译用途](../../../../Engine/Renderer/RenderGraph/GraphPlanUses.cpp) · [RHI 状态入口](../../../../RHI/OpenGL/CommandListState.cpp)

## Goals / Non-Goals

### Goals

- 保持编译入口纯 CPU。执行器借用调用方的同设备即时列表，按既有计划完成首用分配、受限回调和末端输出。
- 把图中声明的精确用途接到当前 RHI 状态粒度，保留物理访问历史和既有包退休语义。以真实图形和传输命令验证原生结果。

### Non-Goals

- D05-T04 负责产品帧、ImGui 与窗口接线，D05-T05 负责 UI fallback 与帧取消。D08-T04 扩展 Compute、image/SSBO 写入和多消费域，D08-T05 完成正式 staging/读回链，D08-T06 完成跨帧资源池、淘汰与预算。 [D05-T04](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t04) · [D05-T05](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t05) · [D08-T04 至 T06](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t04)

## Decisions

### 1. 同图代计划和调用方录制

在 `PhysaraRenderGraph` 内建立执行入口。入口核对同图代封存原图、由编译器发布的只读计划，以及同一设备上可用的 owning/current Graphics 录制。计划的槽号、Pass/Use 索引沿用编译器合同。打开状态和模式由现有 Validation/后端录制边界确认，拒绝时不改列表和设备状态。执行器借用公共 RHI，后端检查由调用方提供的窄录制边界接入，Renderer 的依赖沿 Core 和公共 RHI 传递。

沿用 `AddPass`/`InvokePass` 的类型化数据和查询语义。增加显式取得借用命令列表的执行入口，沿同一 arena 和回调分发机制处理原有回调。执行器持有图的活动阶段至返回，在同一份声明下准备及执行各 Pass。调用方提供同步的设备诊断观察并持有录制关闭、提交和完成观察。 [图声明主规范](../../../specs/render-graph-contract/spec.md) · [即时列表](../../../../RHI/OpenGL/CommandList.cpp) · [验证层借用边界](../../../../RHI/Validation/CommandListInterop.hpp) · [现有诊断取样](../../../../Platform/DeviceHost.hpp)

即时列表的接管入口检查 owner、实际物理录制和 Automatic 模式。调用方在接管前提交已经登记的用途，避免把其它录制工作的未决状态合入图的第一个阶段。Validation 保留原包装列表用于命令和提交，按底层录制身份配对进入与退出。 [即时录制互操作](../../../../RHI/ImmediateGraphics.hpp)

执行器持有同步接管对象。已知初态登记、状态转换请求和用途提交由该对象转发给 RHI。回调仍借用原命令列表消费已提交用途，GraphManaged 期间普通列表的状态修改入口关闭。回调返回即撤销该阶段的访问授权，包内跟踪状态和保活引用继续有效。

### 2. 按槽首用物化并准备当前 Pass 对象

执行结果以计划的物理槽和导入记录建立局部对象表。瞬态槽首次被存活 Pass 或图尾使用时创建，同一槽后续按不相交寿命复用兼容对象。状态和已登记访问随物理对象连续追踪，图内每项逻辑对象的内容仍按编译计划中的版本和写入效果确定。只在新对象、已建立外部初态或录制边界允许时建账。逻辑 last use 可解除图内路由，实际命令、使用中的复合对象和导出引用沿原有持有规则存活。

执行器根据计划中的精确 mip、layer、aspect、格式和 readOnly 标记构建 Framebuffer，实际对象提供样本信息。RHI 附件描述增加可选的 aspect，默认完整格式行为沿用既有调用。OpenGL 按选中的分量挂载 packed 图像，并在设置图形状态时核对 Pipeline 需要的分量。原生 FBO 准备在 typed clear 之前验证。采样视图与 BindingSet 沿现有创建入口构建，布局、Sampler 和绑定序号来自 Pass 的准备参数。同格式 packed 采样视图当前提供深度分量，Sampled 与 ShaderRead 导出均要求明确选择该分量。准备只解析当前声明授权的资源，在回调前核对创建结果。准备好的对象在 Pass 内借用，已用对象交给现有录制保活。图内有序复用按 T02 的槽计划进行。 [资源池边界](../../../../Docs/Architecture.md#graph-resources) · [附件创建](../../../../RHI/OpenGL/Framebuffer.cpp) · [绑定和视图创建](../../../../RHI/OpenGL/Bindings.cpp) · [在途持有](../../../../Docs/Architecture.md#retirement) · [OpenGL 4.6 Core §9.2.2 与 §9.4.1](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf#page=346)

OpenGL 的 ShaderSample 能力取驱动支持与已实现的浮点二维采样视图的交集。原生绑定工厂和图编译使用同一格式能力，图另核对声明的分量和视图范围。R32_UINT 的存储、typed clear 和 Readback 继续使用各自的能力。 [格式能力表](../../../../RHI/OpenGL/Formats.cpp) · [绑定视图](../../../../RHI/OpenGL/Bindings.cpp)

### 3. 分阶段提交用途与统一原生发射

先处理每个 `PlannedGraphClear` 的 CopyDest 和 typed clear，再合并本 Pass `PlannedGraphUse` 的普通请求。每个物理纹理 mip/layer 或整 Buffer 只得到该阶段的兼容完整状态，原精确 aspect 与 byte range 仍用于授权和实际传输。执行器通过接管对象的状态请求和 `CommitUses` 提交本阶段用途，回调使用现有 Draw、copy 和 upload 入口。RHI 在 GraphManaged 核对已登记用途，在 Automatic 的相同消费边界推导用途。两者共用包内 tracker 和后端发射入口，顺序传输与从其它纹理采样的可见性按 GL 有序命令处理。

GraphManaged 在整 Buffer 和纹理 mip/layer 的跟踪状态之外，为当前阶段保留带用途的原始字节区间和 aspect。实际传输、活动绑定和 Draw 中可由参数计算的区间检查这些授权。Indexed Draw 的索引区间与实例属性可直接计算，逐顶点索引值继续由提供数据的调用方保证落在声明范围内。状态枚举与上一阶段相同的资源仍需登记用途。资源状态和实际写历史贯穿录制。 [即时状态请求](../../../../RHI/OpenGL/CommandListState.cpp)

只读深度联合采样必须同时满足计划的只读附件、现有视图和 Pipeline 对深度/模板写入的限制。限定同一实际 mip/layer 的组合，核对前一附件写和后继 Draw 的原生依赖。需要 `glTextureBarrier` 时由同一后端发射入口在实际消费前生成一次，依据真实重叠附件写及联合读取更新该依赖。导入前史不足以排除相邻附件写时，仅在首次实际联合读取的消费边界满足该依赖。实际写入和依赖进度以底层存储为准，状态相同和缓存命中仍检查访问历史。能力表公布 backend 能兑现的格式、形状及联合用途。多消费域 Shader 同步由 D08-T04 交付。 [图执行与同步](../../../../Docs/Architecture.md#barriers) · [图形状态核对](../../../../RHI/OpenGL/CommandListGraphics.cpp) · [抽象状态合同](../../../specs/rhi-common-validation/spec.md) · [OpenGL 4.6 Core §8.14.2.1 与 §9.3.1](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf)

OpenGL 由 internal-format query 公布单样本 2D 的联合用途，格式同时需要实际的附件和采样能力。采样用同格式深度分量视图。联合读取按 Texture 存储记录逐 mip 的实际附件写，在设备级保存最近一次 `TextureBarrier` 覆盖的进度。公开 GL 工厂拥有这些对象的创建和写入记录，图导入、跨包录制和图内槽复用均保留前史。 [原生格式能力](../../../../RHI/OpenGL/Formats.cpp) · [绑定视图](../../../../RHI/OpenGL/Bindings.cpp) · [OpenGL 4.6 Core §22.3.1](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf)

### 4. 图尾状态和交付贯穿同一次录制

首次使用导入时按允许范围登记已知 entry。当前列表已有跟踪的对象继续使用并核对已建立的状态。对于尚未完成初态登记的新纹理，按现有 RHI 首次 Common 状态交接。`BeginTracking*` 属于已知初态建账，转换由实际请求、Commit 和 Close 现有恢复规则完成。使用计划里的导入记录归还约定 final，覆盖仅在图尾使用的直接导入和已使用的未导出导入。调用方对图入口的真实外部状态负责。

瞬态图尾用途按范围转换为供外部消费的着色读或 CopySource。采样深度沿用普通用途的 DepthRead 约定。使用与普通阶段相同的合并规则，在入口校验能静态判断的出口冲突。导入 final、`keepInitialState` 与宿主的 `PresentColor` 状态遵循已有协议，瞬态若在 Close 后继续使用，外部 owner 依据实际恢复后的状态接管。成功结果携带可保活句柄、原版本、范围及用途，发布点位于全部边界请求和清理成功之后。调用方在原列表合同下关闭并提交，经既有接受及完成凭据交付后续消费者。 [计划边界](../../../../Engine/Renderer/RenderGraph/GraphCompiler.hpp) · [图尾编译合同](../../../specs/render-graph-compilation/spec.md#requirement-keep-exported-ranges-available-at-the-end-of-the-plan) · [宿主呈现](../../../../Docs/Architecture.md#present)

### 5. 首错、模式恢复和已发前缀

分别处理入口预检、Pass 准备、实际执行和图尾阶段。每个可能产生 RHI 失败的阶段在进入后续阶段前查看本次诊断作用域，将首个错误关联到阶段、Pass 或资源。prepare 在查询或设备已经报告失败之后抛异常时，保留当时的诊断。回调返回和 RHI void 命令的诊断共同决定有效性。输出列表在入口确认录制后分配并预留，成功结果移交只读独占持有者，连空结果亦可通过同一入口消费。退出收口统一恢复 Automatic、清理逻辑图形状态并使缓存失效。原录制继续持有已发工作的依赖及上传区段。

Pass 准备从已授权条目的范围直接构造拥有型 BindingSet 描述。Debug 迭代器代理与条目存储的分配失败由现有准备错误路径收口，先前已发命令仍按原录制协议退休。 [绑定准备](../../../../Engine/Renderer/RenderGraph/PassPreparation.cpp)

现有 OpenGL 的常规 recording 校验拒绝无效包。失败清理需要能在 owning 录制已失效时执行合法的 CPU 状态收拢，且不能解除另一包的 claim。设备或 context 可用性由实际的 owner/backend 状态判定，诊断是否含 Fatal 只决定停止新工作。不可用时沿设备现有失效退出取得 CPU 交接，恢复阶段原生命令为零。可用时按既有 Close/提交或抛弃录制协议完成或安全退休先前工作。 [录制和状态清理](../../../../RHI/OpenGL/CommandList.cpp) · [设备活性](../../../../RHI/OpenGL/Device.cpp) · [失败和关闭](../../../../Docs/Architecture.md#shutdown)

## Risks / Trade-offs

- **复用槽时按逻辑资源重新声明初态** → 在相邻寿命复用同一物理槽，检查两任对象的首次用途、上一用途与实际输出。另以交叠寿命核对槽隔离。
- **命令时序正确但结果来自旧数据** → 用真实 GL 图执行先上传后采样、前一附件成为 UI 式输入、采样后覆写及只读深度测试。用可区分的像素或字节值对照，同时观察对应的用途和发射路径。
- **恢复动作掩盖首错或破坏已发前缀的引用** → 各观察一次可恢复的准备失败、已发工作后的实际失败、context 不可用的退出。应能区分后续回调停止、成功出口缺失和真实完成后的退休。
- **出口状态与 Close 恢复或消费用途冲突** → 比较入口、成功图尾和 Close 的状态，覆盖直接导入及瞬态出口冲突。分别记录提交接受和完成。

## Migration Plan

先让独立消费者通过现有正式图与 RHI 目标运行完整的首发执行路径，冻结 D05-T04 所需的输出和失败返回边界。D05-T04 随后替换 D04 的临时帧接线并实际提交窗口颜色，D05-T05 接入失败帧调度。 [产品帧接线](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t04) · [帧流程](../../../../Docs/Architecture.md#frame-flow)
