# Design

## Context

OpenGL 的现有录制使用公共资源状态 tracker 和 GraphManaged 接管协议。Draw 和传输都核对实际用途，D08-T03 的 indexed indirect Buffer 可带精确字节范围。当前生产者都是 CPU 上传、复制或附件命令。Compute 入口仍在 Unsupported，Shader 反射和绑定计划以 Vertex/Fragment 为主，Graph 的 Buffer 状态用整 Buffer 作为公共提交单元。引入 Shader 写以后，同一状态的重复访问和跨列表消费也会产生 GPU 依赖。 [提案](proposal.md) · [RHI 能力说明](../../../../RHI/README.md#opengl-原生消费边界) · [当前 RHI 状态路径](../../../../RHI/OpenGL/CommandListState.cpp) · [图执行器](../../../../Engine/Renderer/RenderGraph/GraphExecutor.cpp)

## Goals / Non-Goals

- 同一正式 Shader 入口可准备 Compute 工作，结果经过现有 Graph/RHI 后能由已交付的图形、计算或传输命令按准确范围消费。调度顺序、执行权和实际数据可见性各有可核对的依据。 [Graph 合同](../../../../Docs/Architecture.md#graph) · [同步合同](../../../../Docs/Architecture.md#barriers)
- 沿用单 Graphics 队列、现有即时提交与资源退休。试验 Shader 及验收输入放在 `build/d08-t04/`。产品 Forward 路由、公开传输和读回范围继续由各自工作包决定。 [工作包范围](proposal.md#impact) · [开发检查约定](../../../../AGENTS.md#workflow)

## Decisions

### Canonical Compute 程序与原生准备

- 在现有接口生成、离线编译、SPIR-V 验证、最终反射与包读取入口中增加 `ComputeBaselineV1`。采用 `main` 的单个 Compute stage，保持固定的 Vulkan 1.1/SPIR-V 1.3 目标。固定 `local_size` 进入身份与反射校验。已有 Graphics 包按原 profile 读取和构建。Compute 的数据结构和 b/t/s/u 绑定由同一接口生成，格式、维度和读写意图与活动 IR 对照。仍以参与编译的源码和工具身份判定包能否复用。 [Shader 构建架构](../../../../Docs/Architecture.md#shader-build) · [包消费入口](../../../../Engine/Shader/RhiProgram.cpp) · [Shader 构建规范](specs/shader-build/spec.md)
- OpenGL `CreateShader` 保存 canonical 字节与 Compute 反射。`CreateComputePipeline` 沿用 SPIRV-Cross 和现有 Program/BindingPlan，在准备期完成编译、链接、兼容检查及持有实际依赖。工作组 local size 分轴和乘积用查询的 `MAX_COMPUTE_WORK_GROUP_SIZE`、`MAX_COMPUTE_WORK_GROUP_INVOCATIONS` 核对，Dispatch 的 group count 另按三个轴的 `MAX_COMPUTE_WORK_GROUP_COUNT` 核对。设备创建收集 compute/image 入口、相关阶段与 combined 限制以及 SSBO 对齐，命令录制使用该快照。专用队列能力保持按实际队列查询。 [RHI 对象](../../../../Docs/Architecture.md#rhi-objects) · [OpenGL lowering](../../../../Docs/Architecture.md#opengl) · [OpenGL 4.6 Core §7、§19](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf)

### Storage 视图与公开调度边界

- 逻辑身份使用 set/category/slot，实际活动 t/u 结构化资源在 GL 的同一 SSBO 空间紧凑分配，UBO、image 与组合采样各有独立空间。普通/volatile UBO 与公共有限 push 块继续走各自既有范围版本，公开 RHI 的 push 块允许在原生 Compute 准备中绑定到保留的 UBO 位置。产品包的参数先走生成的普通 UBO。Program 切换按实际计划比较绑定。原生 image 缓存键包含 view、level、layered/layer、access 和 sized format。 [绑定计划](../../../../RHI/OpenGL/BindingPlan.cpp) · [原有范围解析](../../../../RHI/OpenGL/Bindings.cpp) · [Pipeline 增量](specs/opengl-shader-pipelines/spec.md)
- StructuredBuffer_UAV 使用 `canHaveUAVs` 的普通结构化 Buffer，已解析范围受 stride、反射 extent、实际 SSBO 对齐和最大块字节数约束。符合各自公开角色的 Buffer 可同时用于 storage 和 D08-T03 的 indexed 参数。参数生产者负责写入合法的 `DrawIndexedIndirectArguments` 布局和数据。消费者继续核对请求字节区间、用途及命令格式，GPU 生产的数据直接留在 GPU 上。 [资源增量](specs/opengl-device-resources/spec.md) · [公共参数结构](../../../../RHI/RHI.hpp) · [D08-T03 证据](../2026-11-20-complete-draw-batching/tasks.md#final-verification)
- image 采用有限同格式颜色 profile，候选为 `R32_FLOAT`、`R32_UINT`、`RGBA8_UNORM`、`RGBA16_FLOAT` 的 2D/2DArray，实际支持由 target/format 的 image load/store 查询及实现交集决定。创建 `isUAV` 需要写能力，绑定按实际方向再核对读/写能力。视图选一个 mip。`image2D` 绑定一个实际层，`image2DArray` 使用匹配连续层区间的 layered view，访问元数据始终换算回原 storage 的 mip/layer。image 和 SSBO 的实际读写从接口与 IR 可证明的方向取得，无法证明单向时使用读写权限。写范围覆盖和同一 Dispatch 内的并发正确性由生产者合同保证。 [OpenGL 4.6 Core §7.8、§8.26](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf)
- Compute 与 Graphics 的逻辑状态沿用一次一种可执行状态的约定。Validation 在委托前校验公开候选，底层核对实际所有权、布局和 limits。只有有效、非空的 `Dispatch` 才消费活动范围、建立同步依赖并推进执行记录。图内候选更新先撤销旧 Draw、Dispatch 和 push 资格，保留本次录制已设置的 viewport/scissor。显式 ClearState 清除动态区域与候选。SetState 的准备结果、`ContextState` 中的绑定实际值和 Dispatch 的访问记录各自有明确所有者。 [即时录制合同](../../../../Docs/Architecture.md#command-execution) · [Validation 增量](specs/rhi-common-validation/spec.md)

### 图版本、范围与访问授权

- `PassBuilder` 区分 storage 只读、完整写入和读改写。读改写一次声明同时有输入版本和输出版本，依照已有 Load 保留前内容的模型建立内容边。写入按生产者契约覆盖声明的字节或完整 mip/layer 内容。编译按精确重叠范围继续处理 RAW、WAR、WAW、活性和环。不为未被消费的完整覆盖保留旧生产，不为 compute Pass 添加默认副作用。允许的独立同 Pass 访问按实际范围处理，需要中间版本或先后顺序的依赖拆为多个 Pass。单一 read-write 绑定的本地存储算法仍由 Shader 负责。 [图版本合同](../../../../Docs/Architecture.md#graph) · [图编译合同](../../../../Docs/Architecture.md#graph-compile) · [存储用途增量](specs/render-graph-contract/spec.md)
- Runtime 从宿主转交的设备快照填入直接计算支持及 2D/2DArray 各自的格式、方向和层数。Graph 编译据此核对 storage、范围和对齐，生成公共 `ResourceStates::UnorderedAccess` 等用途及精确授权。整 Buffer 公共状态仍按该单元检查兼容性。区间权限和内容依赖按实际字节区间。预建结构化表验证完整绑定身份，实际同步逐条记录匹配的图声明行。同一 storage 同时以不兼容状态访问时沿现有规则拆分。Compute Pipeline 在图执行前准备完成，资源及其视图仍沿既有首次使用的物化路径。`PassResources` 校验类型化版本与用途，RHI 在发出 Dispatch 前以物理对象身份、角色、方向和范围核对当前有效授权。检查落在已有 GraphRecording/放行入口，由其在退场时恢复调用方模式。 [用途编译](../../../../Engine/Renderer/RenderGraph/GraphPlanUses.cpp) · [范围与版本解析](../../../../Engine/Renderer/RenderGraph/PassResources.cpp) · [图执行增量](specs/render-graph-execution/spec.md)

### 按实际访问发射 OpenGL 依赖

公共 tracker 保留用途、初态、永久状态和提交协议。由同一 GL 发射路径根据真正即将发生的访问形成所需依赖。Automatic 提供推导用途，Explicit 要求已提交的对应状态，GraphManaged 再核对计划授权。一次抽象状态提交和一次命令实际消费属于同一依赖路径。可由当前用途确定的原生同步在安全边界合并，身份相同的后继命令仍检查实际访问。 [上游用途与状态来源](../../../../Docs/Reference/NVRHI/RecordingAndBarriers.md#n7) · [GL 同步架构](../../../../Docs/Architecture.md#barriers)

| 实际后继访问 | Shader 写之后的主要 GL barrier bit |
|---|---|
| Texture fetch/采样 | `GL_TEXTURE_FETCH_BARRIER_BIT` |
| image load/store | `GL_SHADER_IMAGE_ACCESS_BARRIER_BIT` |
| SSBO 读写 | `GL_SHADER_STORAGE_BARRIER_BIT` |
| 固定数量 indexed indirect 参数 | `GL_COMMAND_BARRIER_BIT` |
| 顶点、索引、普通 uniform 读取 | `GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT`、`GL_ELEMENT_ARRAY_BARRIER_BIT`、`GL_UNIFORM_BARRIER_BIT`，按有效绑定求并集 |
| Buffer update 或 copy | `GL_BUFFER_UPDATE_BARRIER_BIT` |
| Texture update/copy | `GL_TEXTURE_UPDATE_BARRIER_BIT` |
| 作为读写附件，或使用 FBO clear | `GL_FRAMEBUFFER_BARRIER_BIT` |

后端以实际原生调用选择域。例如 FBO clear 使用 attachment 域，公共 `CopyDest` 同时服务对象更新及 copy。存储 image 或 SSBO 后继写所用的对应 bit 还约束此前已启动的访问次序。连续同态写及读后写依据重叠范围进入该规则。`SetEnableUavBarriersFor*` 的抑制语义限于调用方能够证明独立的访问，已经识别的重叠依赖继续满足。图的同子资源非法反馈由声明和原生边界拒绝。 [OpenGL 4.6 Core §7.13](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf#page=183)

- 原生访问历史由 GL 设备在 owning context 管理，身份与实际 Buffer/Texture storage 一致，随真实发出的工作在录制之间延续。按 Buffer 重叠区段或 Texture mip/layer 标记未满足的最新 Shader 写和各消费域的可见进度，另记录可能阻塞后继 Shader 写的先前访问。一次 `TEXTURE_FETCH` 不推进 `COMMAND` 或 `BUFFER_UPDATE`，GPU 完成证明可收敛未完成访问，不能单独替代 Shader 写的域可见性。Buffer 接近区间预算时先按已完成访问和各域进度合并等价记录。历史随物理存储退休。限制新增存储并使无法建立记录的错误在发出相应命令前可报告。 [资源退休合同](../../../../Docs/Architecture.md#retirement)
- 先完成候选、访问集合、保活和必要存储的准备，再根据即将执行的真实命令合并有效 bits 并发出依赖。已提交的状态元数据、BeginTracking、绑定命中和仅有声明的访问均不充当已发工作记录。普通写只在已证实完全覆盖某记录范围时收敛其历史。部分覆盖保留其它范围的事实。成功或已经可能发出的命令继续纳入前缀保活，失败与安全退场沿既有提交/退出合同。 [上传与完成](../../../../Docs/Architecture.md#uploads) · [即时 Compute 增量](specs/opengl-immediate-compute/spec.md)

## Risks / Trade-offs

- 同一原生 storage 可经多个视图访问。用非零字节偏移、mip 和层选择对照相邻哨兵，并比较活动绑定范围与 IR 反射。
- 有序驱动可能使漏发 barrier 时仍读到期望数据。使用同一已知数据链同时核对必要的域位、发射位置与最终值，覆盖已见域后的第二消费、同态写和读后写。
- GraphManaged 在实际 Dispatch 前的拒绝可能处于已发图前缀之后。结合已有错误作用域和退休队列，用一次确定的非法后继候选验证授权退场、保活和恢复。
- Compute 和 Graphics 共用准备及状态缓存。运行匹配签收输入上的既有 Forward/indirect 回放，检验图形 Program、动态范围和缓存路径仍按原合同工作。

## Migration Plan

先交付 Compute 的 canonical 准备及存储能力，再完成直接 Dispatch、状态切换与寿命。以小型 GPU 序列接通并核对各消费域，随后把 storage 声明、图权限、编译和失败路径接入同一执行入口。程序集和能力说明随覆盖证据交付。正式实现、独立审查、最终 verify 及同步归档按既有流程推进。 [D08-T04 工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t04) · [A/B 流程](../../../../AGENTS.md#review-loop)
