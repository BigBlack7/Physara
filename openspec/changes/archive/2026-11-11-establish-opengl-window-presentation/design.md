# Design

## Context

`Runtime/Main.cpp` 按宿主输入发布推进 `EditorSession`，终止轮次先处理 close 和 poll limit。D04-T02 已提供可选 RHI 会话装配、正式 `EditorUi.pshader`、独占的 UI 准备值和提交回执。宿主已观测实际双缓冲和 `GL_BACK_LEFT` 编码，并提供同步 RHI 错误区间。OpenGL 列表是即时录制，Close 结束录制，Execute 才建立 fence 并 flush。 [产品入口](../../../../Runtime/Main.cpp) · [UI 帧回执](../../../../Editor/Session/UiFrame.hpp) · [实际窗口编码](../../../../Platform/OpenGLDeviceHost.cpp) · [提交实现](../../../../RHI/OpenGL/Submission.cpp)

## Goals / Non-Goals

- 让默认产品在真实窗口显示由当前 EditorSession 合成的 Docking UI。隐藏运行共用同一帧链和退出合同。
- 让有效颜色、已发原生工作、提交接受、交换及 GPU 完成各自有明确判据。
- 通用 FrameSlot、FrameArena、ViewId 和连续 resize/DPI 的阶段验收归 D04-T04。Scene/View 输出及 RenderGraph 沿相应工作包接入。 [工作包边界](../../../../Docs/Developments/D03-D05-FramePipeline.md#d04-t04) · [D05 阶段](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d05)

## Decisions

<a id="presentation-target"></a>
### 1. 宿主持有一个可按完成复用的呈现槽

DeviceHost 在 owner 线程持有一组 `PresentColor`、公共 RHI framebuffer 和完成观察。D04-T03 使用一个固定呈现槽，后续由 D04-T04 的 FrameSlot 提供槽索引。`BeginPresentation` 使用当前发布的可渲染窗口事实取得目标。像素尺寸来自 framebuffer，和逻辑窗口尺寸分开。新目标在纹理和 framebuffer 都创建成功后成组发布。尺寸变化只在槽完成后替换，未取得时以 Suspended 或 Retry 返回，事件继续以有限等待推进。 [帧槽合同](../../../../Docs/Architecture.md#frame-state) · [窗口观测](../../../../Platform/GlfwInput.cpp)

取得值带 `WindowId`、目标代及单次取得身份。token 留在 Runtime 与 DeviceHost 边界，绘制只接收纹理、framebuffer、状态和像素范围。目标为 `SRGBA8_UNORM`，按单层、单 mip、单样本渲染及读取能力创建。初态和出口用 `ShaderResource`，复用 Automatic 跟踪及 Close 的初态恢复。内容初始化由每帧完整清除承担。第一次末端访问前重新核对当前像素范围及可渲染状态，取得期间的尺寸变化取消本轮。 [呈现目标合同](../../../../Docs/Architecture.md#present) · [Close 恢复初态](../../../../RHI/OpenGL/CommandList.cpp)

<a id="runtime-assembly"></a>
### 2. Runtime 装配同配置的正式包和已交付的 UI 入口

Runtime 继续用 `DeviceHost::GetDevice` 借用验证设备，沿 `EditorSession::Create` 的 RHI 装配入口加载 `EditorUi`。CMake 复用 Editor 中包的唯一生成目标，给 Runtime 提供从实际可执行文件目录到该配置产物的相对路径。启动从主模块路径解析，当前工作目录可以不同。资源路径失败、包缺失或 ABI 不符返回有界启动错误，走既有退出码和逆序清理。帮助和参数错误仍在原生及包初始化之前完成。 [构建路径合同](../../../../Docs/Architecture.md#shader-build) · [包生成](../../../../Editor/CMakeLists.txt) · [构建说明](../../../../Docs/Build.md#shader-构建)

Runtime 先消费输入并处理 close、poll limit 和延迟请求。Editor 仍收到暂停轮次以清理交互。可绘制帧取得当前窗口目标后，准备拥有型 UI 批，开启同一 Graphics 列表，先用公共 RHI 清除底色，再调用 `PreparedUiFrame::Record`。有效空批可沿同一路径提交清屏。准备、Open、clear、Record 及 Close 的同步错误各自保留事实，任一失败都撤销本帧交换资格。产品入口按既有状态 1 报告操作失败，正常暂停、过期尺寸和槽忙沿下一次输入重试。 [主循环](../../../../Docs/Architecture.md#frame-flow) · [离屏合成合同](../../../specs/editor-ui-rendering/spec.md)

<a id="window-copy"></a>
### 3. OpenGL 专用互操作执行等尺寸颜色复制

Platform 在 Close 后将同一录制和真实源 framebuffer 交给 `RHI/OpenGL.hpp` 的专用入口。Validation 侧复用已知列表归属及现有原生列表借用，只为宿主提供私有窄交接。后端核对 owner/current context、同设备、无开放录制、闭合且可提交的同一物理包、源附件与导出状态。互操作沿当前即时 profile 的同一 context 有序消费已完成录制的附件写入。GL 的一致附件写和 framebuffer 读取在该顺序下可见，公共导出状态仍为 `ShaderResource`。 [原生互操作入口](../../../../RHI/OpenGL.hpp) · [列表提取](../../../../RHI/Validation/ValidationBackend.hpp) · [依赖发射](../../../../RHI/OpenGL/CommandListState.cpp)

本包选择 `glBlitNamedFramebuffer`，复用 PresentColor 的原生 FBO 及其 `GL_COLOR_ATTACHMENT0`，等尺寸以 `GL_NEAREST` 仅复制颜色到默认 `GL_BACK_LEFT`。宿主在装配呈现能力时核对所需原生入口和实际后缓冲编码。OpenGL 专用路径将离屏逻辑顶行映射到窗口原生顶行，等尺寸矩形的 Y 端点只在该边界反向一次。`ContextState` 建立实际 back buffer 选择、全范围 scissor 前提及 sRGB 开关，精确更新或失效触及的缓存字段，原生错误后仍撤销对应已知性。DSA 复制使用现有 FBO 及颜色转换规则。 [原生 FBO 读写选择](../../../../RHI/OpenGL/Framebuffer.cpp) · [ContextState](../../../../RHI/OpenGL/ContextState.hpp) · [行序约定](../../../../Docs/Architecture.md#conventions)

实际 `GL_SRGB` 后缓冲启用 `GL_FRAMEBUFFER_SRGB`。等尺寸 blit 对源 RGB 解码并在目标编码。实际 `GL_LINEAR` 后缓冲关闭该开关，直接传送已存储的一次 SDR 编码值。两条分支都消费同一 `SRGBA8_UNORM` 源，约束为等尺寸逐像素复制。该选择依赖 OpenGL 4.6 的读写转换规则，原生验收以已知中间色、半透明覆盖和四角读回确认。 [OpenGL 4.6 Core §17.3.7、§18.3.1](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf)

<a id="submission-and-cancel"></a>
### 4. 用同一物理包承接末端源访问和失败前缀

宿主在末端复制前显式登记源 FBO 和实际附件依赖，沿 `OpenGLCommandList::RetainResource` 取得本次闭合包的 lease。任何可失败的保活准备在发命令前完成。复用同一 `UploadWorkPacket` 的已发标记和退役路径，复制可能已发时标记 work。由该包的 Execute fence 保护末端源访问，FBO 同时持有附件 view 和纹理。包已因 Draw 保活相同依赖时沿原账目去重，清屏后复制的帧也显式保活源 FBO。 [原生资源 lease](../../../../RHI/OpenGL/CommandListOwnership.cpp) · [在途记录](../../../../RHI/OpenGL/Submission.hpp)

`PrepareSubmission` 关联同一取得和闭合列表。Runtime 调用宿主的窄提交步骤，由宿主委托同一验证 Device 的 `ExecuteCommandList`，将真实结果登记到当前取得并返回给 Runtime。这样交换只能消费宿主已关联的接受结果。Prepare 检查目标已变或帧失败时取消复制。已有前缀且列表仍合法时可接受仅用于退休的票据。复制报告错误或 Execute 返回零时取消交换，仍可能在途的资源按已有包和受控退出保留。无发出工作的取得直接取消。未证明完成的包不得通过重开列表或再次提交来覆盖。 [正式 Validation 提交](../../../../RHI/Validation/ValidationDevice.cpp) · [即时提交接受](../../../../RHI/OpenGL/Submission.cpp) · [取消与收拢](../../../../Docs/Architecture.md#shutdown)

Runtime 用同一录制的 Close、复制及 Execute 结果消费 UI 回执。完整成功路径先确认 UI 上传，再由宿主交换已接受的有效颜色。交换前如已知窗口已失效则取消交换并保留真实票据。交换自身失败独立报告，已确认的上传及已接受的资源寿命按原票据推进。 [UI 回执](../../../../Editor/Session/UiFrame.hpp) · [呈现与退休](../../../../Docs/Architecture.md#present)

<a id="completion-and-shutdown"></a>
### 5. 通过既有 Event 推进槽复用和退出

宿主在非零票据返回后、任何新提交前设置本槽 Event 快照。Poll 返回真且同步区间成功才解除槽保留。Pending 走有限事件等待，查询失败或接受后无法建立观察则停止新工作并保持保活。正常帧不做全设备等待。停止时 Runtime 先结束准备值、逻辑列表和 Editor，宿主结束取得、释放自有槽句柄与 Event 观察，再沿现有外部 owner 预检和设备 drain。物理包持有实际在途依赖；外部引用仍阻止破坏性关闭。保持 `RhiDrainPending` 的宿主及 context 重试合同，已失效 context 沿现有不可用分支。 [Event 查询](../../../../RHI/OpenGL/Queries.cpp) · [宿主收拢](../../../../Platform/RhiDeviceHost.cpp) · [退休](../../../../Docs/Architecture.md#retirement)

<a id="present-timing"></a>
### 6. 分开记录交换耗时与 GPU 完成

2026-11-11 实现时核对固定 GLFW 的 `swapBuffersWGL`，其调用 Win32 `SwapBuffers` 后丢弃返回值。Platform 因此在同一 owner/current context 上用 `wglGetCurrentDC` 借出设备上下文，直接检查 `SwapBuffers` 的实际返回值及 `GetLastError`，以有界宿主错误报告失败。原生窗口和 context 的创建、输入及销毁仍沿宿主已建立的 GLFW 生命周期。 [固定适配](../../../../ThirdParty/glfw/src/wgl_context.c) · [Win32 交换合同](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-swapbuffers)

仅用单调 CPU 时钟包围实际窗口交换调用，返回 `PresentWait` 秒值与交换结果，取消无交换采样。GPU 完成只观察对应 Event。若验证需要对照 GPU 时长，沿现有 TimerQuery 记录实际 GPU 区间。运行时记录有界，独立区分 CPU 帧就绪、提交接受及交换成功。 [完成查询合同](../../../specs/opengl-completion-queries/spec.md)

## Risks / Trade-offs

| 风险 | 约束与验证 |
|---|---|
| 重复编码、遗漏编码或上下颠倒 | 在真实窗口的交换前读回非对称四角、中间色和 UI 叠加区，记录实际后缓冲编码和通道位数。环境只有一种后缓冲编码时，用另一编码的真实等尺寸附件补查颜色转换，并单列真实窗口覆盖。 |
| 末端复制早于 Close，或晚于跟踪 fence | 用同一帧的原生调用顺序观察 Close、blit、fence、flush、swap。顺序与退休检查避开会提前完成 GPU 的同步读回。 |
| 复制源由普通 clear 恰好保活而掩盖 FBO 缺失 | 用清屏后复制的帧验证末端 FBO lease；在真实完成观察前后核对保活、可复用和关闭。 |
| 失效目标或失败复制被误当作可交换 | 以已发前缀后取消、实际复制错误以及复制后无有效票据作为差异场景，观察复制、交换和退休边界。 |
| 原生裁剪或 sRGB 缓存污染下一帧 | 下一列表复用前次带 scissor 的 RHI 绘制描述，在不同背景上核对剪裁边界及中间色。 |
| 单槽限制吞吐 | Pending 时继续事件处理，延迟到真实完成再复用。多槽容量与度量归后续帧槽工作包。 |

## Migration Plan

D04-T03 首先在唯一产品入口启用底色和已交付的 Docking UI。D04-T04 接入通用槽索引、帧内存和 View 输出，D05 将生产和导出移交 RenderGraph。窗口目标、单次取得、末端复制和交换仍归宿主边界。 [D04-T04](../../../../Docs/Developments/D03-D05-FramePipeline.md#d04-t04) · [D05](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d05)
