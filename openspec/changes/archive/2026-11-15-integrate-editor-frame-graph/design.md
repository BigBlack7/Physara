# Design

## Context

动机与工作包边界见 proposal。当前基准为 `d780edab5683a9ae241741ec7c99cbb29619445b`，D05-T03 已归档。`Runtime::RenderFrame` 仍依次调用 `View::Prepare`、`PrepareUiFrame`、诊断上传、窗口 clear 和 `PreparedUiFrame::Record`。`UiRenderer::Prepare` 提前持有目标 FBO 和每条 draw 的 BindingSet，`UiTextures::Finish` 用整批布尔结果确认全部请求。 [proposal](proposal.md) · [Runtime](../../../../Runtime/Main.cpp) · [UI 实现](../../../../Editor/ImGui/UiRenderer.cpp) · [纹理实现](../../../../Editor/ImGui/UiTextures.cpp)

既有图接口已支持值拥有的 PassData、独立 setup/prepare/execute、确切版本及范围、存活计划、首次物化和图尾句柄。`PassPreparation` 为当前声明建立 BindingSet，`PassResources` 借出附件、资源和已准备绑定。`RenderGraph` 析构会回退其专用 arena，`FrameArena` 在已有借用时无法扩容。当前 View/Editor 均有实际预算和 Pending 行为，需要一并接入。 [图接口](../../../../Engine/Renderer/RenderGraph/RenderGraph.hpp) · [执行接口](../../../../Engine/Renderer/RenderGraph/GraphExecutor.hpp) · [arena](../../../../Engine/Core/FrameArena.hpp)

现有 `PresentTarget` 只有纹理、FBO 和尺寸。新建 `keepInitialState` 纹理第一次被 tracker 使用时从 `Common` 起步，正常 Close 和接受提交后才建立描述中的初态。UI 管理纹理的初态为 `Common`，诊断和窗口颜色的初态为 `ShaderResource`。图导入必须保留这种差别。 [呈现值](../../../../Platform/Presentation.hpp) · [呈现 owner](../../../../Platform/Presentation.cpp) · [状态追踪](../../../../RHI/Common/StateTracking.cpp)

## Goals / Non-Goals

本次设计闭合一个产品帧的资源声明、执行、确认和退休。沿用现有 Graphics/传输 profile、诊断图像算法、单窗口和固定帧槽。上层调用通过当前图的值传递资源，持久 owner 通过接受回执接收实际句柄。 [D05-T04](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t04)

后续能力按开发计划推进。D05-T05 交付编译失败后的安全 UI-only 图、完整取消和错误帧恢复验证。D06 接入真实场景输出，D08-T06 接入跨帧资源池，D16 扩展预览及读回消费者。这里的上传压力占位属于 D04 已有的正常准备结果，其依赖在开放列表前确定。 [帧流程](../../../../Docs/Architecture.md#frame-flow) · [诊断与有效设置](../../../specs/editor-viewport-output/spec.md)

## Decisions

### 1. 沿既有模块组装产品图

Runtime 拆取宿主目标值，建立当前图并汇合 Renderer 与 Editor 的声明。Renderer 生成诊断输出的图值，Editor 追加上传与 UI，Runtime 封存、编译、准入并执行。`PhysaraRenderer`、`PhysaraEditor` 和产品目标按实际公开头与实现依赖链接既有 `PhysaraRenderGraph`，Engine 接口使用公共 RHI/Graph 值。 [架构依赖](../../../../Docs/Architecture.md#overview) · [帧入口合同](specs/editor-frame-graph/spec.md#requirement-compose-the-product-frame-through-one-compiled-graph)

| 数据 | owner 与寿命 | 消费与发布 |
|---|---|---|
| `ViewId`、请求及已接受 `ViewOutput` | View，跨帧 | Editor 请求、Renderer 构图与提交后发布 |
| `UiTextureId` registry | Editor 会话，跨帧 | 驻留项持句柄和导入事实，View 项持 ViewId/role |
| `FrameSlotUiFrameBindings` | 当前 FrameSlot 使用期 | token 到当前 TextureId/版本/范围，普通 draw 持绑定索引 |
| 图、计划及 PassData | 当前帧 CPU 消费期 | 构图、编译、执行，结束后释放回调对象 |
| 几何、投影与上传像素 | FrameSlot 的拥有型存储 | 延迟回调及 RHI 即时复制 |
| 上传请求与执行记录 | 本次 UI 准备值 | 提交回执或取消，结束于下一次 Advance 前 |
| `PresentationToken`、取得及末端复制 | Runtime/DeviceHost | 关闭、提交、Present 和完成观察 |

准备好的 draw 保留数值裁剪、索引参数、reset 类型和帧内纹理索引。执行函数使用无捕获回调及已封存数据，避免借用 setup 局部对象。与 ImGui 请求元数据的关联留在本次确认 owner 中，执行回调消费复制的载荷和有限的执行记录。沿既有 Filament 参考中的 setup/实际资源分离落实项目自己的版本与权限合同。 [参考分析](../../../../Docs/Reference/Filament/FrameGraphAndPasses.md#f7)

### 2. 帧槽分别提供载荷与图对象存储

FrameSlot 保留现有几何和像素 arena，增加供 RenderGraph 独占的 arena，并持有 `FrameSlotUiFrameBindings`。两者按实际测量的字节及对齐预留，继续使用已有容量上限。图对象销毁和其游标回退先于相应存储复用，载荷借用在本次 CPU 消费结束前有效。独立 arena 使图的 `Rewind` 无法覆盖后来追加的 UI 像素，同时允许准入阶段重新组装候选图。

槽仍以宿主同索引的 Ready/真实完成作为复用证明。图与计划可在 CPU 消费结束后释放，已发 RHI 依赖沿录制包保活，帧槽的可复用载荷及绑定存储在安全取得后 reset。失败前缀的 ticket 继续关联原槽。 [帧槽合同](../../../specs/frame-slot-lifecycle/spec.md) · [绑定寿命](specs/editor-frame-graph/spec.md#requirement-retain-frame-bindings-and-resource-generations-until-safe-reuse)

### 3. 先形成逻辑候选，再按实际消费者保留物理输出

View 的准备阶段负责请求代、尺寸、设备限制、诊断预算和候选元数据。已接受且匹配的颜色显式 import。新代只声明描述、拥有型四象限像素及上传 Pass，实际纹理由首次使用物化。保持 `NoSceneOutputs` 的诊断语义和既有状态标签。

成功准备的外部驻留引用按 token 去重保存在 UI 批次的拥有型快照中，快照只含原纹理强引用和导入事实。公开注销只终止后继帧的新解析，不能使已经成功准备的批次丢失依赖。构图优先消费该快照并将原对象交给图保活，实际 BindingSet 仍在 Pass prepare 创建；取消准备值释放其快照，已发 GPU 访问继续按 RHI 票据退休。CR-001 的整改保持原公开合同，未新增注销阶段限制。

UI 收集完整帧的普通 draw，先处理 reset 和空裁剪，再校验 token。相同 token 复用帧内结果。不同 token 指向同一驻留对象时，规范导入表保存版本 0 和当前生产版本，采样使用更新后的版本。`ImportTexture` 等价重入返回版本 0，因此 UI 更新后必须从本帧映射取当前版本。 [重复导入合同](../../../specs/render-graph-contract/spec.md#requirement-declare-exact-resource-ranges-and-canonical-imports)

只有普通 draw 实际采样到候选版本时，才为需要跨帧保留的新 View 输出或新托管纹理添加 `ShaderRead` 导出。这样这些导出有真实持久消费者，隐藏、空裁剪和未使用请求的生产者仍可裁剪。独立 Image 即使不属于主视口的那一条命令，也参与同一消费统计。图执行后把导出句柄与候选身份组合为持久 `ViewOutput` 或驻留条目，发布值只携带跨帧所需的句柄、尺寸和代。 [解析合同](specs/editor-frame-graph/spec.md#requirement-resolve-ui-tokens-to-exact-frame-local-texture-versions) · [裁剪合同](specs/editor-frame-graph/spec.md#requirement-cull-unused-editor-producers-from-actual-ui-reads)

### 4. UI 上传和 Raster 分开声明权限

UI 几何仍按预算增长并复用现有缓冲缓存。准备阶段完成所需持久 Buffer 的容量选择和 CPU 拷贝，构图函数只导入它们并声明实际写入和读取区间。投影存储和完整绑定均使用 `sizeof(UiProjection)` 对应的 16 字节已写范围，避免在旧 256 字节缓冲上建立违反公共 RHI 对齐合同的部分视图。VB/IB 使用本批实际字节，indexed draw 的索引值与 base vertex 沿既有 CPU 预检保证落在声明数据内。

| 操作 | 图声明与后续读取 |
|---|---|
| 新诊断、新托管纹理或首次占位 | `CopyDestination`，完整 mip 的 `Discard + Full + Store`，后续读取新版本 |
| 管理纹理局部更新的暂存写 | 单独的 `CopyDestination` Pass，完整定义区域暂存纹理 |
| 将区域复制到原纹理 | 后续 Pass 读取暂存的 `CopySource`，以 `Load + Partial + Store` 写原 mip，保留区域外 texel |
| UI 投影、VB、IB 上传 | 精确 byte range 的 `CopyDestination` 写，分别由 ConstantBuffer、VertexBuffer、IndexBuffer 读取 |
| 窗口初始化 | 原 `PresentColor` 的 `ColorAttachment` 写，typed Clear 与 Store |
| UI 合成 | `Load + Partial + Store` 写窗口新版本，读取所有采样纹理及实际几何范围 |

暂存写入和复制各有 Pass，使同一暂存资源的写、读顺序由已有编译器表达。窗口 clear 与 UI 合成保留显式内容边，空 UI 仍可导出清除版本。各上传以其消费者决定活性，使用现有外部导出类型表达持久交接。

UI Raster 的 prepare 使用 `PassPreparation::SampledTexture`、`ReadBuffer` 和 `CreateBindingSet` 为每个去重后的采样绑定创建集合。execute 使用 `PassResources::BindingSet`、`Framebuffer` 及精确 Buffer 查询，保留原来的 Pipeline、颜色公式、裁剪、IdxOffset/VtxOffset、索引位宽及 reset-state 行为。绑定是本 Pass 拥有的对象，已发 draw 经 RHI 录制保活。 [UI 增量规范](specs/editor-ui-rendering/spec.md) · [图执行主规范](../../../specs/render-graph-execution/spec.md)

纹理创建请求先获得可校验的临时逻辑 token，物理句柄在执行时产生。长期 registry 的临时条目只保存身份及协议关联，图 ID 保存在本帧绑定中。驻留注册入口补充明确的进出状态、内容有效性和允许范围，现有直接消费者在迁移时提供这些值。注册及构图保留相同设备、有效身份和格式的既有拒绝行为。

### 5. 编译能力与上传准入取自真实产品输入

Runtime 在 owner/context 有效时取得本产品所用格式的 `QueryFormatSupport`、宿主尺寸限制及已经验证的 UI Shader/Buffer 能力，形成值类型的 `GraphCompileOptions`。首发只提供实际用到的单层二维、单样本颜色、传输、采样、单色附件及几何/常量用途。Buffer 范围和瞬态预算使用现有几何、UI 上传、诊断及驻留预算能够支持的界限。编译本身保持纯 CPU，未知能力组合保留为不支持。

纹理上传 Pass 先于几何上传 Pass 声明，稳定拓扑序在该产品依赖形状中保持所有纹理写入先于 Buffer 写入。图编译后按 `plan.passes` 选择存活上传，生成与命令实际次序一致的大小列表，继续调用现有 `CheckUploadCapacity`。该接口按纹理列表再按 Buffer 列表模拟 chunk，因此实施时须用计划核对这个顺序约束。CPU 拷贝、图对象容量和 GPU 上传额度分别按各自实际用途记账。 [现有容量核算](../../../../RHI/OpenGL/Upload.cpp) · [准入合同](specs/editor-frame-graph/spec.md#requirement-admit-the-uploads-that-will-actually-execute)

诊断上传造成 Pending 或超过共享额度时，准备值切换到已有的诊断占位原因，重新收集必要 UI 几何、字体请求及读取，清空旧图绑定并形成新的完整候选图，再核对准入。基础 UI 仍 Pending 时按现有协议取消尚未执行的取得。受控构图或编译错误交给失败收拢，D05-T05 后续扩展安全图恢复。

### 6. 显式提供外部资源交接事实

`PresentTarget` 增加公共 RHI 的 entry/final 值，由 Platform 在取得目标时按该目标代的真实使用状态给出。新代及从未执行的取消仍以 `Common` 接入，已被接受且正常交接的同代以 `ShaderResource` 接入。此事实与目标一起维护，按实际使用和接受提交推进。为避免把已发失败前缀错误标记为新资源，沿现有提交记录保留该代是否已实际使用的事实，失败无法证明状态时进入现有受控退出。

Runtime 用这些值导入原纹理，当前帧保守声明窗口输入内容无效并显式 Clear，最后导出合成版本的 `Presentation` 用途。呈现 token 保留在 Runtime/宿主。执行器返回的原对象、导出版本和 final 是颜色交接依据，随后沿现有 `Close → PrepareSubmission → SubmitPresentation → Present` 顺序执行，末端复制仍发生在跟踪 fence 前。 [呈现增量规范](specs/opengl-window-presentation/spec.md) · [架构呈现合同](../../../../Docs/Architecture.md#present)

驻留和已接受 View 纹理的 entry/final 由各自 owner 维护并匹配 Close 行为。现有 UI 管理纹理使用 `Common` 的保持初态，已接受诊断使用 `ShaderResource`。状态信息和内容是否已定义分别记录。外部注册提供缺失的状态合同，图按已采纳规则核对重复导入与一致性。

### 7. 按实际执行与同帧回执确认

每个上传请求关联自己的终结写 Pass 和本次准备身份。图成功意味着计划中的存活 Pass 全部成功，结合已编译存活集合即可形成逐请求的执行确认。区域更新的终结操作是复制到驻留纹理，新建的终结操作是完整上传。请求被裁剪时保留 Pending，清理尚未发布的临时 token。占位就绪与基础字体就绪也按对应实际上传确认。

执行输出的实际句柄只在图成功后可用。完成确认继续核对同一录制、准备代、非零 ticket、宿主 `colorValid` 和整段诊断。满足条件后把创建结果交给 registry，将匹配当前请求且实际采样的颜色交给 View，并确认相应 ImGui 状态。Present 的交换结果在之后单独处理。未满足条件时沿既有取消路径结束 ImGui 元数据借用，RHI 负责已经发出的 GPU 引用。 [输出发布](specs/editor-frame-graph/spec.md#requirement-publish-only-accepted-graph-outputs)

### 8. 复用现有失败与同步责任

所有 Pass 使用执行器准备的 GraphManaged 权限，按已有 tracker/emitter 完成同步。调用方在同设备上 Open，提供本次 `RhiErrorScope` 适配，执行后 Close。准备期失败保留其诊断，执行诊断独立取样。构图和编译失败在首条图命令前取消取得，执行中失败停止后继 Pass，关闭可用录制并让宿主提交前缀或受控退出，票据只用于相应资源的退休。

这次接线的最小失败验证覆盖准备取消、编译拒绝和已发前缀后的执行失败。D05-T05 在这些边界上继续实现跨错误帧恢复和完整故障场景。 [失败与寿命主规范](../../../specs/render-graph-execution/spec.md#requirement-stop-on-execution-failure-and-preserve-issued-work)

### 方案取舍依据

| 考虑方案 | 评估与决定 |
|---|---|
| 用一个 Pass 包住现有整个 UI Record | 会隐藏暂存写后读的依赖及各纹理活性，当前选择按生产与消费拆 Pass |
| 继续在准备期为每条 Image 绑定实际纹理 | 新诊断颜色只有执行时才物化，当前选择图内版本映射和 Pass 准备 |
| 为全部纹理请求导出资源以保留上传 | 会使未使用请求变成外部根，当前只为实际消费并需持久保留的结果导出 |
| 扩展通用 RHI 为图提供一套上传分配器 | 当前产品计划可以满足既有准入顺序，继续使用现有上传池和检查入口 |
| 为每次纹理换代维持长期 BindingSet 历史缓存 | 会增加失效和退休状态，当前采用 Pass 内去重及录制包保活 |

## Risks / Trade-offs

| 风险 | 应对与最小证据 |
|---|---|
| alias token 的二次 import 回到版本 0，UI 采到更新前内容 | 本帧规范映射保存最新写版本，使用两个 token 和多次 Image 检查一次更新及相同新像素 |
| 临时导出把隐藏诊断或无消费者上传留在图中 | 由实际 draw 决定持久导出，用隐藏、全裁剪及保留一个 Image 的对照检查资源与回调次数 |
| UI 常量绑定扩到已写范围外，或暂存写读落在同一 Pass | 明确投影字节范围及两阶段区域更新，原生图验证更新区域和外侧哨兵 |
| 准入仍按旧固定顺序估算，编译后上传顺序已变化 | 从存活计划生成账单，核对纹理到 Buffer 顺序和一次压力占位准备 |
| 新目标错误使用 ShaderResource 入口，或取消错误推进状态 | 用首次取得、执行前取消、有效提交及真实完成后重取同一代的序列核对 |
| 图回退覆盖 UI 载荷，resize 释放仍在途绑定 | 独立 graph arena 与既有载荷 arena，真实未完成票据下 resize 并观察最后引用 |
| 将成功计划或前缀 ticket 当成纹理已接受 | 逐请求关联存活执行与完整颜色回执，检查一项上传被裁剪及一个执行失败前缀 |

验证范围按当前变更风险确定，CPU 计划检查覆盖声明和裁剪，原生窗口及像素覆盖真实消费者，同一轮生命周期序列覆盖 resize、槽背压和退休。匹配输入的既有证据可复用，缺失的本地 fixture 按本次独立风险补最小消费者。 [验证技能](../../../../.agents/skills/physara-verification/SKILL.md)

## Migration Plan

1. 先接通显式目标/驻留导入事实、帧槽存储、帧内图输出及 UI 绑定值，沿实际目标依赖接入 RenderGraph。
2. 将 View 与 UI 上传、区域复制、几何和 Raster 迁入图声明及回调，同步修改 UI 准备和提交确认接口。
3. 将 Runtime 切换为唯一构图、编译、准入、执行和宿主交接路径，迁移直接消费者并清理旧入口。回退以整个 change 为单位恢复配套调用链。
4. 按 tasks 完成本地验证、维护者调度的 A/B 审查与注释、整改及 B 签收，最终验证后同步三项能力并归档。D05-T05 以本次产品图入口和实际失败结果为前置继续推进。

### 实施核对

2026-11-15，首次产品图编译拒绝在 256 字节投影缓冲中绑定 16 字节子范围。沿现有整块常量视图规则将 UI 投影存储缩为生成 ABI 的 16 字节，未修改 RHI 或图编译器合同。实际像素与范围验证见 [A 本地记录](tasks.md#a-local-verification)。View 创建仅接收宿主尺寸能力，设备分配权留在图执行器。UI 外部注册明确要求同设备 owner 提供进出状态和内容范围，执行接管及绑定继续使用现有 RHI 设备归属检查。
