# Design

## Context

D04-T03 的 `Runtime/Main.cpp` 每轮先封存 CPU UI，再调用 `DeviceHost::BeginPresentation`。`Platform/Presentation.cpp` 用单个目标、票据和 Event 守住该目标的复用。Editor 的准备批次已经复制 ImGui 几何和纹理像素，`UiTextures` 持有受管理纹理及驻留纹理。Viewport 面板还没有离屏输出。 [立项](proposal.md) · [Runtime 循环](../../../../Runtime/Main.cpp) · [现有呈现边界](../../../../Platform/Presentation.cpp) · [UI 纹理准备](../../../../Editor/ImGui/UiTextures.cpp)

## Goals / Non-Goals

**Goals**

- 让实际帧数据、窗口目标和最后一次 GPU 使用能追溯到同一槽和票据，区分准备取消、已发前缀、完成观察失败以及交换结果。
- 让主视口的布局请求、逻辑身份和纹理代沿现有 Editor 与 Renderer 依赖方向传递，后续可把有序录制接入 RenderGraph。

**Non-Goals**

- 编辑命令、撤销与配置事务归 D04-T05。RenderGraph 的声明、编译、执行与图失败退路归 D05。场景创建、提取、真实 View 渲染及其历史产物随 D06 及后续工作包接入。 [阶段范围](../../../../Docs/Developments/D03-D05-FramePipeline.md#d04-t05) · [场景阶段](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d06)

## Decisions

<a id="frame-slots"></a>
### 1. Runtime 调度三个槽，宿主为同一索引取得窗口目标

Runtime 持有固定 3 个 FrameSlot 及其帧内 CPU 状态。Platform 按相同上限管理对应的 `PresentColor`、framebuffer 和完成观察者，由 Runtime 装配时给出同一容量。单 owner 线程同一时刻仅有一个活动窗口取得。token 关联宿主、槽索引、目标代和本次取得。应用层选择槽，宿主验证索引并在当前槽的旧票据真实完成后按最新 framebuffer 尺寸复用或成组替换目标。复制前及交换前的窗口事实重查仍在实际消费边界。 [槽与窗口目标](../../../../Docs/Architecture.md#frame-state) · [窗口呈现](../../../../Docs/Architecture.md#present)

`BeginPresentation(slotIndex)` 返回 Ready 时同时证明该索引首次使用或其上次提交已完成，Runtime 据此开放该槽的 CPU 存储。沿用现有宿主对 Graphics Event 的正向完成观察，`ResetEventQuery` 和 `SetEventQuery` 在该槽上一观察已结束后使用，捕获发生在实际 Execute 接受票据之后、其它提交之前。宿主的对应槽记录票据和 Event，Runtime 用回执关联本帧状态。两者共用设备的完成序列。Pending 时 Runtime 至多检查一轮有界索引。全忙时记录有界的背压次数或等待时间并回到有限事件等待，和 `PresentWait` 分别记录。完成失败当轮停止新渲染。 [FrameSlot 回收](../../../../Docs/Architecture.md#history-commit) · [Event 合同](../../../specs/opengl-completion-queries/spec.md)

<a id="frame-arena"></a>
### 2. FrameArena 持有正式 UI 几何和上传字节

`Engine/Core` 提供只负责字节存储、对齐及复用的 FrameArena。Runtime 持有的槽通过准备入口提供本次有效分配范围。封存的 ImGui 顶点、索引和纹理上传字节在准备时复制到槽内，诊断图候选的像素也在本次准备产生。命令记录和 RHI 句柄继续使用有明确析构边界的拥有型值。底层存储按本帧经检查的需求预留并保留容量供下次复用，发布借用后地址固定。失败准备先释放批次引用并撤回临时纹理身份，再将游标退回准备前的位置。此前区段的地址和数据、保留容量及历史高水位继续有效。 [Core 合同](../../../../Docs/Architecture.md#core) · [UI 准备与上传](../../../../Docs/Architecture.md#imgui-renderer)

每槽 CPU arena 的初始上限取 64 MiB，按需分配。其依据是现有 UI 几何 8 MiB、托管纹理更新 16 MiB、诊断视口像素最多 32 MiB，另留对齐填充余量。分别检查元素数乘积、对齐后的游标及累计末端，维持现有 UI、设备尺寸和 RHI 上传预算。诊断目标的实际请求超限时在准备阶段提供带原因的占位。基础 UI 准备超限走现有有界错误路径。

在途诊断上传共用 32 MiB 额度。Runtime 从仍受提交保护的槽扣除已发诊断字节，槽取得 Ready 后归还对应额度。准备阶段通过宿主预检 OpenGL 的 64 MiB 共享上传容量。UI 按实际录制顺序提供诊断色、当帧使用的首次占位、托管纹理及缓冲写入。预检沿用录制的映射对齐规则，先尝试复用同包首个可容纳的 chunk 尾部，再计算新 chunk 容量。小缓冲写入计入逻辑载荷和次数。设备据此区分空闲时仍超限的计划和暂被未退休区段占用的容量。准入与录制使用同一列表 chunk 参数，在同一个 owner 线程连续推进。 [原生容量预检](../../../../RHI/OpenGL/Upload.cpp)

诊断图因本身额度或共享容量暂缓时，若基础 UI 可准入，本帧合成带原因的占位。基础 UI 暂时无法准入时，Runtime 取消尚未打开列表的窗口取得，继续消费输入、零超时推进设备退休并有限等待事件。后续帧按当时的布局请求和实际容量重新准备。原生故障及永久超限沿既有有界错误路径报告。 [帧推进与退休](../../../../Docs/Architecture.md#retirement)

帧内借用在该次准备完成或取消时结束，复用地址不恢复旧准备值。沿用 `PreparedUiFrame` 的单次 `Record` 与 `Finish`/取消关系，先结束 `DrawData` 和 ImGui 纹理请求元数据借用，再进入下一次 `NewFrame`。有效槽重新取得后释放旧帧内状态、reset 游标并开始新的槽使用。RHI 写入在录制期间复制到后端管理的上传存储，设备的记录包及在途包继续持有原生使用所需的引用。 [借用 UI 帧](../../../specs/editor-session-input/spec.md#requirement-publish-a-borrowed-cpu-ui-frame) · [原生引用退休](../../../../Docs/Architecture.md#retirement)

<a id="viewport-output"></a>
### 3. View 管理离屏输出代，Editor 解析本帧绑定

由 `Engine/Renderer` 内实际使用的 View 状态提供类型独立的 `ViewId`、输出身份和当前颜色代。Editor/Viewport 持该 View 的逻辑身份、内容矩形与尺寸请求，复用 Core 代号和现有 ImGui 纹理注册机制。Runtime 在帧边界传递像素请求和携带 RHI 颜色句柄的输出值，Engine 的帧状态沿 Core/RHI 接口准备。驻留条目继续持有现有纹理依赖。新增 View 输出条目只登记 `ViewId` 与输出角色，逻辑 token 在 resize、隐藏和重新显示后保持稳定。合成时从封存的视口图像命令读取实际 token，并使用当帧匹配的 View 输出。身份失效、没有匹配颜色或等待上传容量时，洋红占位和裁剪在视口内的文字采用同一个原因。标签字形在布局期间准备，选中的标签几何在当帧合成。 [视口尺寸与输入](../../../../Docs/Architecture.md#viewport-input) · [纹理解析合同](../../../../Docs/Architecture.md#editor-output)

面板按当帧逻辑内容矩形和有效的 framebuffer 比例计算 View 像素范围，内容缩放用于 UI 逻辑样式。Engine 按检查后的有效请求准备独立于 `PresentColor` 的离屏颜色。隐藏或零面积跳过该视口的准备。持续变化时以当帧最后的有效请求为准。初始诊断输出采用单 mip、单样本的 `SRGBA8_UNORM` 纹理，通过公共 RHI 在第一次采样前完整写入非对称测试图。使用已有线性采样及 UI 合成链。相同尺寸复用已经接受的静态测试纹理，尺寸变化为本次帧准备候选纹理和绑定。有效执行及匹配回执后发布可复用的新代，旧代的 CPU owner 随替换释放，GPU 使用随对应包退休。

`NoSceneOutputs` 作为当前阶段场景渲染的显式结果。诊断纹理有独立角色和 UI 标识。场景未提供输出时，面板展示无场景或准备状态，诊断纹理尚无可用代时显示占位。可在准备阶段安全结束的视口尺寸或预算问题走这条路径，实际 RHI 故障沿已有图形诊断与退出合同处理。窗口底色与 Editor 合成继续独立提供，诊断提交没有可提交的场景历史版本。后续 D05 将本帧颜色及绑定接入图资源，视口 token 合同仍由 Editor 消费。 [UI 与窗口输出进图](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t04) · [View 历史合同](../../../../Docs/Architecture.md#history-commit)

<a id="frame-order"></a>
### 4. 帧边界决定请求，录制和确认消费对应的准备结果

每次发布先处理 close 或 poll limit。最小化和零 framebuffer 时调用现有 Editor 输入清理、非阻塞回收和有限事件等待。有有效窗口时先尝试取得一个槽和窗口目标，再封存 UI 并发布当帧布局请求。全忙的轮次仍推进既有 CPU UI 和输入路由，跳过拥有型帧准备及任何新 GPU 工作。D04 的这一处理延续每个可渲染输入发布的消费合同，避免背压期间丢掉一次性动作。 [主循环](../../../../Docs/Architecture.md#frame-flow) · [CPU 帧合同](../../../specs/editor-session-input/spec.md#requirement-publish-a-borrowed-cpu-ui-frame)

在打开列表前准备本次诊断输出、纹理请求和帧内绑定。保持唯一的临时顺序链，从写入新诊断图、初始化 `PresentColor` 和执行 UI 上传/合成，到 Close、宿主末端复制、Execute 和交换。GPU 录制阶段只消费封存的数据和目标。尺寸、布局、平台和字体请求的发布或更改发生在准备边界，纹理状态确认发生在同一工作实际执行并接受后。宿主仍可在末端消费发现窗口已变而取消复制或交换。 [窗口复制及提交](../../../specs/opengl-window-presentation/spec.md#requirement-copy-valid-color-before-the-tracking-fence-and-swap)

Windows 最大化的原生读回显示 PresentColor 已是新尺寸，默认后缓冲仍以旧 drawable 范围裁切末端复制。末端复制前经同一 ContextState 显式绑定默认 framebuffer，使此驱动更新窗口 drawable。目标和颜色检查、复制与 fence 的顺序沿原合同。

产品装配现有正式 `EditorUi` 程序与基础合成资源。通过固定 ImGui 动态 atlas 协议准备基础字体，要求第一份可呈现 UI 所需字体已成功生成、绑定并上传，且有效提交已接受。程序或必需字体准备、录制失败以明确启动原因收拢并返回 1。由窗口暂停、背压或尺寸过期导致的无有效尝试保持待处理。后续 atlas 更新与重建沿同一 `WantCreate`、`WantUpdates`、`WantDestroy` 协议，`OK` 由匹配回执确认，旧代最后使用随原提交保活。 [启动要求](../../../../Docs/Architecture.md#runtime) · [纹理确认合同](../../../specs/editor-ui-rendering/spec.md#requirement-confirm-texture-requests-only-after-their-upload-is-accepted)

<a id="frame-retirement"></a>
### 5. 在途前缀和最后一帧沿设备关闭协议收拢

只在确认没有发出本次 GPU 工作时退还未提交的槽。若已发合法前缀且获得接受票据，先关联其槽和实际依赖，再处理颜色有效性、UI 确认和交换。Execute 已产生票据但 Event 设置失败时保留原票据和引用，停止新帧并进入受控关闭。无可靠票据的已发工作由当前记录包或设备失败收拢路径保护。两种失败均不能 reset 对应槽。正常推进在所属 context 上调用现有非阻塞回收，包括暂停和槽忙轮次。 [帧失败合同](../../../../Docs/Architecture.md#frame-flow) · [取消和退出](../../../../Docs/Architecture.md#shutdown)

退出先停止新取得并结束本次准备及输入接入，释放 Editor、View 和 Runtime 持有的外部引用。帧内 CPU 字节的最后消费者是准备和命令录制，RHI 已复制实际访问的数据并接管必要句柄。设备按已有规则收拢最后的有效或失效包，随后释放原生环境。宿主报告 `RhiDrainPending` 时保留窗口、context 与设备所有者，沿现有显式 Shutdown 重试。受控失败保存首个原因，context 已确认失效时沿后端失效退出协议处理。 [Runtime 退出合同](../../../specs/runtime-lifecycle/spec.md#requirement-exit-in-ownership-order-with-meaningful-results)

## Risks / Trade-offs

| 可能失效的边界 | 约束 |
| --- | --- |
| 忙槽被过早覆盖，或 Event 观察到错误的队列尾 | 观察者紧随同槽真实 Execute 捕获。只有取得同一槽的 Ready 才重新开放 arena 与窗口目标。 |
| 连续 resize 和 atlas 重建让旧代继续被强引用 | 长期注册保存逻辑身份，帧内绑定随本帧消费，实际 GPU 依赖随在途包释放。按需分配并保留有上限的 arena 高水位容量。 |
| 尺寸或取整失败被误当作场景成功，或基础字体失败被占位掩盖 | UI 显示明确的测试/准备状态。基础字体有单独的就绪条件与失败出口。 |
| 退出先撤掉窗口/context，使最后访问缺少完成权威 | 沿设备显式收拢及失效退出，保留主因与未证明完成的工作，核对最后在途帧的实际顺序。 |

## Migration Plan

随 Runtime 一起更新宿主索引取得接口和相关本地消费者，普通产品构建复用同配置 UI 包。实现后更新构建说明中的实际帧路径。交付按 A 本地验证、独立 B 审查和复核、最终 verify、同步及归档推进。 [开发约定](../../../../AGENTS.md#review-loop) · [构建入口](../../../../Docs/Build.md)
