# Design

## Context

`Workspace` 目前直接维护面板开关。会话在 `Advance` 中生成 ImGui 布局和 CPU 帧，主视口直接用内容矩形和窗口像素比例生成离屏尺寸。Renderer/View 已持有按实际提交发布的诊断颜色。宿主在创建时请求交换区间，现有输入请求批次只处理光标和关闭。 [工作区](../../../../Editor/Panels/Workspace.cpp) · [会话推进](../../../../Editor/Session/EditorSession.cpp) · [视口测量](../../../../Editor/Viewport/Viewport.cpp) · [颜色代](../../../../Engine/Renderer/View.cpp) · [宿主请求](../../../../Platform/DeviceHost.cpp)

## Goals / Non-Goals

**Goals**

- 让领域意图经单一的字段应用入口到达实际 owner，历史能重放同一个入口，帧准备消费当前请求。
- 使用户能区分已输入的比例、图形提交已接受的比例、平台已确认的窗口设置，以及当前请求的等待或拒绝原因。

**Non-Goals**

- D05 负责整帧 RenderGraph。场景命令、选择、相机导航和最小 JSON 文件操作接入 D06-T06。D16-T02 完善持久化与工作区恢复。路径、AA 与 Shader 切换沿各自的就绪工作包实现。 [后续帧图](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d05) · [场景编辑](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t06) · [持久化](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#d16-t02) · [路径准备](../../../../Docs/Architecture.md#plan-readiness)

## Decisions

<a id="edit-owners"></a>
### 1. 两种领域编辑按字段到达各自 owner

| owner | 状态及应用入口 | UI 消费 |
|---|---|---|
| Renderer/View | 通过带 `ViewId` 的比例请求更新主 View 的 requested 状态。持有最后被接受的比例、尺寸及诊断颜色代 | Inspector 的主 View 区域显示比例编辑与状态，无选中实体时也可使用 |
| EditorSession | 保存主视口显隐，并通过 Editor/Commands 保存本会话的活动编辑及历史 | View 菜单和视口关闭按钮发出同一显隐意图。重新显示沿用停靠面板身份 |
| Platform/DeviceHost | 通过独立的标题及交换区间请求更新活动窗口设置 | Window 菜单提供 VSync 开关、标题输入和请求结果。标题仅在确认后提交，编辑缓冲属于控件草稿 |
| Runtime | 在主循环串行连接快照、编辑应用、窗口请求、视口准备和提交 | 只传递值、当次借用与结果 |

Editor/Commands 只需要表示已支持的比例与显隐变更，历史保存两个值和重放所需身份。会话协调器在应用边界借用 View 的窄修改入口。修改请求的对象负责检查 View 身份或会话归属以及字段范围，已失效输入在 Release 也拒绝。面板从值快照取得显示和启用状态。后续 Scene 字段可沿同一应用边界扩充。 [领域编辑合同](../../../../Docs/Architecture.md#edits) · [会话快照](../../../../Docs/Architecture.md#editor-input)

<a id="edit-transactions"></a>
### 2. 活动拖拽保存原值，完成后才进入历史

滑块激活时从权威快照取得原请求及目标。每轮只提交同一字段最后一个合法预览值。普通释放结束活动编辑，只有末值不同才记录一次。Escape 取消，失焦、暂停或捕获失效由会话沿已有 discard 门控取消。取消优先于同批迟到的更新和释放，经原入口恢复请求。恢复后的有效比例仍随新输出接受而确认。用户关闭程序时，Runtime 先处理退出，销毁会话的活动编辑及历史，随作用域释放 View 和输入接入。 [事务规范](specs/editor-domain-edits/spec.md) · [输入失效合同](../../../specs/editor-session-input/spec.md#requirement-reset-editor-interaction-when-input-becomes-invalid)

有效鼠标释放可能与末段移动在同一 ImGui 帧消费。Inspector 在比例滑块调用期间延后释放，复用 ImGui 的抓取偏移、限幅和舍入采样末段位置，随即恢复按钮状态并结束手势。 [比例控件](../../../../Editor/Panels/Workspace.cpp)

Undo 和 Redo 共用 Edit 菜单动作与当前工作区路由的 Ctrl+Z、Ctrl+Y。首次按下通过当前焦点、弹层及捕获资格后按事件顺序入队，各动作在重放时读取届时的历史位置。标题文本编辑保有其键盘输入。活动拖拽期间暂停重放。只有 owner 接受实际字段修改后才移动记录。提交有效的新编辑时清除 Redo 分支，净零操作、拒绝和取消保留原历史。初始容量为最近 64 次已提交编辑，Undo 与 Redo 共用该预算，超限时淘汰最早可撤销记录。 [工作区动作](specs/editor-session-input/spec.md) · [输入路由](../../../../Editor/Session/ActionRouter.cpp)

GLFW 适配在接入时记录宿主已发布的最后序号和待发布事件的前缀长度。EditorSession 从同一宿主批次筛出接入后到达的事件，再判断失效边沿和快捷键归属。重接前 Wait 累积的事件仍随宿主完整发布，重接后的首批可消费同一 bank 的新事件后缀。 [会话结束规范](specs/editor-domain-edits/spec.md#requirement-retire-editor-edits-on-session-shutdown) · [接入边界](../../../../Editor/ImGui/GlfwPlatform.cpp)

<a id="edit-frame-boundary"></a>
### 3. 封存布局后应用编辑，再准备像素请求

Runtime 先消费事件和退出决定，沿已有规则尝试取得窗口槽。EditorSession 用当前只读快照生成 UI、意图与主视口逻辑内容矩形，再封存 DrawData。随后应用域内编辑和独立窗口请求。视口尺寸转换使用这次封存的矩形、窗口度量和应用后的请求比例，准备完毕才进入录制。窗口槽 Pending 的轮次仍能消费 CPU UI 和编辑，资源准备等可用槽。失效批次只处理交互取消和输入归还。 [主循环合同](../../../../Docs/Architecture.md#frame-flow) · [正式入口](../../../../Runtime/Main.cpp)

显隐在帧边界生效。已封存的 UI 维持本轮布局，关闭后跳过该 View 的新目标准备。重开由下一份可见布局提供新的矩形。每轴使用 `floor(逻辑内容尺寸 × framebuffer 轴尺寸 / 窗口逻辑轴尺寸 × requestedScale)`，在相乘后检查有限值、取整和像素范围。内容缩放继续服务 UI 样式。View 以实际设备上限与已有诊断纹理预算核对范围。窗口目标、布局命中、已有诊断 token 和旧输出退休继续各走原合同。 [像素合同](specs/editor-viewport-output/spec.md#requirement-apply-viewport-extent-at-a-frame-boundary)

<a id="view-setting-readiness"></a>
### 4. 有效比例随当前诊断输出的接受发布

初始比例请求为 1.0，有效比例及尺寸在首个匹配提交接受前为空。以 0.5 到 1.0 的有限比例接通实际像素消费，现有诊断目标和上传预算覆盖该字段的真实 Pending 及范围拒绝场景。View 保持最后接受的状态，并按最新请求报告等待布局、窗口槽或上传容量的原因。当前尺寸超限时报告设备范围或单目标预算失败。可见视口没有匹配颜色时沿现有占位绘制，在封存前准备状态标签需要的字形。原生图形错误沿已有诊断和收拢合同处理。 [诊断呈现](../../../specs/editor-viewport-output/spec.md#requirement-present-a-diagnostic-editor-without-advancing-scene-history) · [配置准备](../../../../Docs/Architecture.md#plan-readiness)

封存请求、候选颜色和确认保留同一 View、本次请求和像素范围的关联。录制使用本帧准备的绑定。收到完整合成的有效提交回执，且诊断输出实际被该帧使用，才随 `View::Publish` 接受有效比例及尺寸。取整尺寸相同可复用已经接受的颜色代，仍由当前合成回执确认新比例。隐藏、取消、只提交前缀及未使用诊断输出的帧保留原有效状态。View 诊断颜色与场景历史沿 `NoSceneOutputs` 合同分离，旧 GPU 使用由原提交保活。 [有效比例合同](specs/editor-viewport-output/spec.md#requirement-confirm-effective-viewport-settings-from-accepted-output) · [退休合同](../../../../Docs/Architecture.md#history-commit)

会话只用本轮匹配的主 View 快照生成像素请求。缺省或失配时，工作区继续封存 UI，可见视口使用占位。比例请求代随像素范围传递，View 准备时核对当前代，EditorSession 在合成时核对封存请求，接受时再次核对 owner 的当前代。 [请求生成](../../../../Editor/Session/EditorSession.cpp) · [输出准备与发布](../../../../Engine/Renderer/View.cpp)

<a id="native-window-settings"></a>
### 5. 窗口设置在 Platform 内取得原生确认

编辑器分别发布拥有型标题意图与 VSync 意图。Runtime 在回调结束后的同一请求边界交给活跃宿主，标题或区间失败按字段返回。DeviceHost 校验 owner 线程、活跃窗口身份及适用的 context。标题的外部输入先校验 UTF-8、NUL 与 255 字节上限。每项合格意图由宿主记录为 requested，等待实际应用时属于 Pending。生效值只在本次原生确认后更新，失败保留最后确认值并提供原因。宿主仍可用时继续下一帧及另一字段的请求。光标和关闭沿原输入请求批次与失败合同。 [窗口设置合同](specs/native-window-host/spec.md) · [输入请求合同](../../../specs/window-input/spec.md#requirement-apply-requests-through-an-explicit-safe-boundary)

标题沿现有 GLFW 窗口更新路径设置，并核对 Win32 实际 caption。GLFW 的标题缓存于平台调用前更新，因此需要实际窗口读回确认。VSync 沿现有交换区间路径提出 0 或 1，由当前 WGL drawable 上可用的交换控制查询核对实际区间。固定 GLFW 快照未传回 WGL setter 的结果，区间读回能识别被忽略的请求。缺少能力或可用查询时报告原因。原始 HostInfo 保留创建观察的含义，实时请求状态由新的宿主读取入口发布。有效区间表示原生控制状态，交换等待和设备完成沿原有测量。 [标题调用](../../../../ThirdParty/glfw/src/window.c) · [交换控制](../../../../ThirdParty/glfw/src/wgl_context.c) · [呈现计时](../../../../Docs/Architecture.md#present)

菜单按当前字体的字形覆盖显示标题。缺字及控制码以 `\u{...}` 标示，字面反斜杠也转义，便于核对草稿和两项回执。编辑控件与 Platform 继续使用原始 UTF-8，码点显示只用于菜单文本。 [窗口菜单](../../../../Editor/Panels/Workspace.cpp)

## Risks / Trade-offs

| 风险 | 验证或约束 |
|---|---|
| 活动滑块失效后被迟到的释放提交，或文本撤销同时驱动工作区 | 保留已有输入归属和失效边沿。用一次真实动作序列覆盖拖拽合并、取消、净零及分支，并区分文本快捷键与会话历史 |
| 新比例被提前标为有效，或按窗口尺寸重建视口 | 关联当前布局、诊断绑定和接受回执。独立比较窗口与离屏像素，并核对 Pending 到接受、取整同尺寸和原范围拒绝后的恢复 |
| GLFW 调用返回后把未生效的窗口设置显示为成功 | Platform 在请求边界读取实际 caption 和交换区间。一个字段失败时仍验证另一字段可成功，已有帧和输入可继续 |
| 关闭时遗留命令或破坏已发工作的寿命 | 退出决定早于消费新命令，先解除会话输入接入。使用在途退出及同宿主重新接入证据核对相应边界 |

## Migration Plan

随 EditorSession 消费者更新快照和帧值，沿模块目标显式加入 Editor/Commands。默认可见入口、单窗口工作区、诊断视口与原生收拢继续作为产品验证入口。更新使用说明中的真实控制与状态。检查材料按工作包留在 `build/d04-t05/`，独立 A/B 签收后再验证、同步和归档。 [构建说明](../../../../Docs/Build.md) · [交付规则](../../../../AGENTS.md#workflow)
