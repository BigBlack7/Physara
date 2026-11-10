# Design

## Context

`EditorSession::Advance` 在宿主线程封存 CPU `DrawData`，公开结果只携带帧状态和宿主请求。固定 ImGui 1.93.0 docking 当前走 CPU 字体 atlas 的过渡路径。D03 的公共 RHI 已有规范 Shader 包消费、普通 VB/IB/CB 写入、整 mip 纹理上传、同格式矩形复制、单 viewport/scissor 绘制和真实提交退休。其 `WriteTexture`、`CopyTexture` 等命令通过回调报告失败，部分拒绝仍可为已发前缀取得票据。 [会话入口](../../../../Editor/Session/EditorSession.hpp) · [固定 ImGui 合同](../../../../ThirdParty/imgui/imgui.h) · [原生 RHI 消费边界](../../../../RHI/README.md#opengl-原生消费边界)

## Goals / Non-Goals

**Goals**

- 从同一会话接出正式的离屏 UI 录制入口，保持 `DrawData`、调用者目标和实际提交之间的所有权及错误边界。
- 在一次原生图像检查中验证坐标、索引、颜色和字体，并以有限的后续帧检查纹理更新、重试和退休。

**Non-Goals**

窗口目标取得、末端复制与交换由 D04-T03 装配。通用帧槽、View 输出身份、resize 重建由 T04 落地，Graph 读写依赖由 D05 接入。D04-T02 以调用者提供的离屏颜色目标为验收出口。 [工作包划分](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d04)

## Decisions

### 1. Renderer 随实际消费入口接入会话

由 `Editor/ImGui` 持有 renderer 资源和纹理注册，准备结果由本次调用者拥有。使用 renderer 的调用者提供已就绪的公共 `RHI::Device` 和正式 Shader 包路径，Session 在第一次 UI 帧之前完成 renderer 接入。对外沿 Session 暴露所需的 RHI 值和帧结果，GLFW 及 ImGui 细节留在对应私有适配。包或资源准备失败返回有界原因，并逆序清理已取得的对象。实际接入才设置 `RendererHasTextures`、`RendererHasVtxOffset` 和 reset-state 回调。Session 按消费方式选择字体准备，renderer 通过纹理协议生成 atlas，CPU-only Runtime 入口采用既有 CPU atlas 并沿用归档的生命周期。 [依赖方向](../../../../Docs/Architecture.md#overview) · [CPU 帧合同](../../../specs/editor-session-input/spec.md#requirement-publish-a-borrowed-cpu-ui-frame)

调用者提供已初始化内容的 `SRGBA8_UNORM` 单色、单样本二维 RHI framebuffer 和同一 UI 帧对应的实际像素范围。底色的 clear 或完整写入归调用者，UI Pass 只进行覆盖合成。缺少新 UI 帧或像素范围为零时返回无工作，非空但不兼容的目标在录制前给出原因。合法帧即使无可绘制三角形，也可处理待提交的纹理上传。 [UI 输出合同](../../../../Docs/Architecture.md#imgui-renderer) · [窗口目标衔接](../../../../Docs/Architecture.md#present)

### 2. Shader ABI 与颜色使用已有正式通路

接口声明进入 `Engine/Shader/Interface`，UI 顶点与片元源码位于 `Shaders/UI`。用现有 CMake 接口生成 `Data.hpp`、`Bindings.hpp` 和 `GraphicsBaselineV1` 包，`PhysaraEditor` 显式依赖唯一包 target 并按生成的 `AbiHash` 用 `LoadRhiProgram` 消费。每帧正交 scale/bias 放在普通 std140 常量块，绑定范围满足公共常量缓冲对齐。纹理与线性 clamp sampler 使用独立逻辑 binding。输入 layout 核对 `ImDrawVert` 的实际步长、位置、UV、打包 RGBA 偏移与通道顺序。位置和 UV 使用 `RG32_FLOAT`，顶点色使用归一化 `RGBA8_UNORM`，阶段 IO 及资源来自生成合同。 [接口生成](../../../../Engine/Shader/Interface/README.md) · [包消费入口](../../../../Engine/Shader/RhiProgram.hpp) · [Shader 层次](../../../../Docs/Architecture.md#shader-runtime)

顶点 Shader 在插值前把 sRGB 创作 RGB 解码为线性，alpha 原值进入插值。基线驻留纹理使用 `SRGBA8_UNORM` 的颜色图标或 `RGBA8_UNORM` 的 display-linear 输入，由 SRV 格式解释采样域。管理的 RGBA32 纹理采用 `SRGBA8_UNORM`，单通道 Alpha8 像素打包为白色 RGB 与原覆盖率 alpha，复用相同采样路径。片元将线性 RGB 及 alpha 分别相乘。Pipeline 以 `SrcAlpha/OneMinusSrcAlpha` 合成 RGB，alpha 使用 `One/OneMinusSrcAlpha`，sRGB 附件在存储处编码一次。关闭深度、模板和剔除，使用公共 RHI scissor。 [颜色及行序约定](../../../../Docs/Architecture.md#conventions) · [合成合同](../../../../Docs/Architecture.md#imgui-renderer)

### 3. 准备、录制和确认沿同一 UI 帧推进

```text
Session 封存 DrawData
  -> 准备拥有型几何、上传字节与绑定
  -> 调用者的开放 Graphics 列表记录上传和 UI Draw
  -> 调用者关闭、提交并带回该列表的结果
  -> 同一 UI 帧内确认或取消纹理请求
```

准备结果是单次消费值，拥有整理后的 VB/IB 源数据、裁剪后的命令、纹理更新及 RHI 引用。借用 `ImTextureData` 仅用于同一 UI 帧内的状态确认或取消，下一次 `NewFrame` 前结束借用。RHI 在录制调用内接收 CPU 源字节，在途继续持有的是后端的上传区段和资源引用。调用者拥有列表、清除、提交、完成及目标最终用途，Renderer 在已开放的 Graphics 列表中依靠现有 Automatic 跟踪建立上传、采样及附件用途。 [借用与复制](../../../specs/editor-session-input/spec.md#requirement-publish-a-borrowed-cpu-ui-frame) · [上传合同](../../../specs/opengl-resource-uploads/spec.md) · [在途引用](../../../../Docs/Architecture.md#retirement)

准备中的空几何、上传项和采样绑定描述沿用可抛的 count 构造。像素大小在复制前已知，上传项以独占数组持有字节，扩容移动只转移数组所有权。准备时使用可抛构造直接建立资源名称，使 MSVC Debug 的代理分配进入准备错误边界。OpenGL 绑定对象的资源、解析记录和视图节点三个空向量也使用 count 构造，分配失败由原生 `CreateBindingSet` 捕获并返回空句柄。两条失败路径都由准备值析构撤回已有创建前缀的临时 token。 [既有代理分配合同](../2026-10-28-establish-opengl-resource-uploads/design.md#8-复用-common-tracker-限定-transfer-状态) · [原生绑定构造](../../../../RHI/OpenGL/Bindings.cpp#L69)

投影基于 `DisplayPos` 和 `DisplaySize`，逻辑左上映射到 NDC `(-1,+1)`。`(ClipRect - DisplayPos) * FramebufferScale` 得到左上原点的像素裁剪，用本次真实目标尺寸夹取。以像素中心是否在半开区间内决定整数 scissor 边界，空区间跳过。几何连续拼接，`IdxOffset` 与列表全局索引基址以元素计，`VtxOffset` 与列表全局顶点基址组成 base vertex。先用宽算术校验范围和转换，再填 RHI 参数。`ImDrawIdx` 的 16 或 32 位配置分别使用 R16 或 R32 格式。持有有界的普通 VB/IB 和 CB，容量足够时用同队列有序写入复用，扩容后旧资源由原包保活。 [几何与裁剪约定](../../../../Docs/Architecture.md#imgui-renderer) · [Draw 参数](../../../../RHI/RHI.hpp)

回调命令在普通绘制解析前分流。固定版本的 `ImGuiPlatformIO::DrawCallback_ResetRenderState` 触发公共 `ClearState`，下一条普通命令重新设置完整 UI 图形状态。该分支随即结束本条命令。超出本包已声明语义的回调在准备时报告并拒绝该批。 [固定版本回调定义](../../../../ThirdParty/imgui/imgui.h) · [图形状态重建合同](../../../../RHI/RHI.hpp)

### 4. 稳定 token 与纹理上传请求分开管理

Renderer 以有代的 `UiTextureId` 索引当前会话的驻留条目，每条保存真实纹理、采样语义与保活绑定。ImGui 的非零 `ImTextureID` 显式编码槽与代，编码前检查位宽。代在进程内分配且不回绕，使同一宿主重接也能拒绝历史 token，耗尽时给出有界失败。正常更新沿用身份，删除或重建后的旧代不能再解析。Resident 图标使用已提供的 RHI 纹理，帧内只解析此帧使用的条目和其绑定。无效或过期的非基础图像引用使用有界诊断和一张共享的洋红色占位纹理，基础字体准备失败使该 UI 帧不可发布。 [纹理身份](../../../../Engine/Core/Identity.hpp) · [驻留与帧内绑定](../../../../Docs/Architecture.md#editor-output)

| 请求 | 准备与录制 |
|---|---|
| `WantCreate` | 校验格式、实际尺寸及字节范围，分配新驻留代，复制整 mip。通过 `SetTexID` 安装仅供本次准备解析的临时 token，RHI 整 mip 写入先于采样。 |
| `WantUpdates` | 复用有效代，核对 ImGui 的 `UpdateRect` 及行距，按行复制包围矩形。用现有 `WriteTexture` 写同格式、同区域尺寸的临时二维纹理，再用 `CopyTexture` 写目标子区域，区域之外的 texel 保持原值。 |
| `WantDestroy` | 以正式状态请求结束逻辑解析，清空本 renderer 的 `TexID` 和关联，再调用 `SetStatus(Destroyed)`。未标记 `WantDestroyNextFrame` 且仍有 CPU 像素时，该调用按 ImGui 协议重新排入 `WantCreate`。尚未进入 `WantDestroy` 的旧 atlas 仍可供本帧使用。已准备或已记录的使用依靠拥有型引用和 RHI 包保活，实际释放沿最后使用的真实完成或受控退出。 |

绑定按驻留代复用，缓存随条目淘汰。基线为几何可复用容量合计 8 MiB、每个准备结果的纹理上传 16 MiB、管理纹理驻留 32 MiB 及 64 个逻辑条目。准备值中的命令数组另有 8 MiB 上限，覆盖没有几何数据的回调命令。管理纹理边长上限为 2048，会话设置 atlas 的 `TexMaxWidth` 和 `TexMaxHeight` 以限制固定版本的实际增长，并设置 `ImGuiPlatformIO` 对应的能力字段。设备是否能创建仍由实际 RHI 校验。容量计入格式转换后的字节及一次准备的暂存资源，边界检查在分配和源访问之前。已发前缀或在途的旧代由现有 RHI 预算及退休约束。 [设备上传预算](../../../../RHI/README.md#opengl-原生消费边界) · [ImGui 纹理状态](../../../../ThirdParty/imgui/imgui.h)

### 5. 接受回执决定何时确认上传

`WantCreate` 和 `WantUpdates` 的确认需要该准备结果中的全部必需上传实际录制、作用域无 Error 或 Fatal、调用者返回包含这些上传的同一列表的非零接受票据。确认才通过 `SetStatus(OK)` 发布内容有效性。CPU 侧收集的有效绑定与票据接受分开，GPU 完成另用于退休。回执随该次准备值消费一次。

公开 RHI 的 void 命令由宿主的 `CaptureRhiErrors` 提供同步错误区间。回调在写诊断日志前推进 Error/Fatal 计数，多个区间各自对比取样值。Renderer 只在写入成功后执行依赖的复制或 draw，图形状态设置成功后才发出对应 `DrawIndexed`。任一录制命令报错即返回有界原因并取消准备值。调用者仍负责关闭并提交或收拢列表中的已发前缀，并在完整录制后把 Close/Execute 的诊断和接受票据一并传回。T03 可在产品呈现的同一宿主上沿用该区间。 [主循环的执行结果](../../../../Docs/Architecture.md#frame-flow) · [已接受提交](../../../specs/opengl-resource-uploads/spec.md)

准备、录制或提交失败时取消临时 token，保留 ImGui 的请求、像素及更新范围。可继续运行时以最新请求重试，未确认的局部写允许保守重传。已发前缀能取得票据时由票据保护依赖，该票据只用于退休；未取得票据的前缀沿 RHI 的收拢和受控退出合同。Renderer 在销毁 ImGui context 前结束准备、解除注册并交回 CPU 引用，退出中的 GPU 依赖仍归设备处理。 [提交保活](../../../../Docs/Architecture.md#retirement) · [退出收拢](../../../../Docs/Architecture.md#shutdown)

## Risks / Trade-offs

| 风险 | 约束与验证 |
|---|---|
| 像素比例、坐标原点或偏移多算一次 | 同一非对称图使用非零 DisplayPos、非等比缩放、多个列表及非零局部偏移。两种索引宽度走同一录制入口，按独立像素预期核对。 |
| reset 命令误读纹理或继续绘制 | 在有效 Draw 之间插入无纹理的 reset 命令，核对两侧的实际像素及命令次数。 |
| 正票据掩盖纹理未完成的执行 | 在一条更新序列核对取消、实际 RHI 错误和成功重试，比较状态、token 与受影响区域。 |
| 已拒绝的写入或状态仍被后续命令消费 | 在真实映射容量拒绝及附件 feedback 时检查原驻留纹理、已发前缀和目标内未被绘制的像素。 |
| Debug 容器代理分配绕过准备边界 | 在建立批次和已有临时身份的上传前缀后分别触发代理分配拒绝。用本机调用栈区分 Editor 采样绑定描述和 OpenGL 绑定对象的实际分配点，核对有界失败、身份撤回、请求和像素保留及实际重试。 |
| 旧 token 回到新资源或在途引用过早释放 | 删除后立即释放 CPU 句柄，触发槽复用并在真实 Event 完成前后核对引用。 |
| blend 或 sRGB 顺序出错 | 在已知底色上验收独立推导的重叠颜色、alpha、图标和字形边缘。冷路径读回沿 D03 的本地观察边界。 |

## Migration Plan

D04-T02 交付可录制到离屏目标的 Session 消费入口和提交确认。T03 向相同入口传 `PresentColor`，在列表关闭后做末端复制，再提交和交换。T04 接入带完成票据的帧槽与 View 输出代，D05 将本帧上传、绑定及 UI 读取移交 RenderGraph 依赖。 [D04-T03](../../../../Docs/Developments/D03-D05-FramePipeline.md#d04-t03) · [D04-T04](../../../../Docs/Developments/D03-D05-FramePipeline.md#d04-t04) · [D05](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d05)
