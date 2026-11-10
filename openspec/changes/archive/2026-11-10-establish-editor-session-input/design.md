# Design

## Context

`Runtime/Main.cpp` 在参数解析后创建 Diagnostics 和 `DeviceHost`，消费已编号的输入批次，并在关闭或达到 poll limit 时统一退出。`DeviceHost` 已在原生窗口安装回调、保存稳定的 user pointer，并提供 `GlfwAccess`、`HostFrameInput` 和延迟请求入口。仓库固定的 ImGui docking 和官方 GLFW 平台 backend 已在 `imgui` target 中编译，`Editor/CMakeLists.txt` 尚无消费者。 [Runtime](../../../../Runtime/Main.cpp) · [窗口借用](../../../../Platform/GlfwAccess.hpp) · [输入合同](../../../../Platform/Input.hpp) · [第三方来源](../../../../ThirdParty/README.md)

## Goals / Non-Goals

**Goals**

- 从唯一产品入口驱动真实的 CPU 编辑器帧，明确窗口、会话和输入发布的寿命。
- 让面板显隐和有作用域的动作成为统一路由的真实消费者，为后续视口和领域操作留出窄接入点。

**Non-Goals**

D04-T02 承接 UI RHI 绘制和字体纹理，T03 承接窗口交换，T04 承接测试视口与帧槽，T05 承接真实领域事务。D05 将顺序帧编排接入 RenderGraph，D06 开始接入实际场景与资产。 [D04 至 D05 计划](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d04) · [D06 入口](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d06)

## Decisions

### 1. Runtime 装配，Platform 持有原生环境

在 `Editor/CMakeLists.txt` 建立 `PhysaraEditor`，由现有 `Physara` target 消费。Editor 依赖 Platform 的值合同和窄入口，私有链接现有 `imgui` 和 `glfw` targets。Engine、Platform 和公开 RHI 的消费者均无需包含编辑器或 ImGui 头。只有 `Editor/ImGui` 的私有平台适配接触 `GlfwAccess` 和 GLFW。 [构建与依赖](../../../../Docs/Architecture.md#build) · [现有 targets](../../../../ThirdParty/CMakeLists.txt)

Runtime 在参数和 help 判定、Diagnostics 创建、宿主就绪之后创建 Session。Session 在宿主 owner 线程上持有一个 ImGui context，私有适配借用 `GLFWwindow*`，以 `install_callbacks=true` 接入官方 GLFW 平台 backend。使用平台入口 `ImGui_ImplGlfw_InitForOther` 即可服务这个 CPU 消费者。官方 backend 链到已有的宿主回调并在 Shutdown 恢复它们，原有 user pointer 继续由宿主维护。任何已取得的适配或 context 都按逆序清理，首错通过现有有界诊断路径报告。 [单入口装配](../../../../Docs/Architecture.md#runtime) · [固定 GLFW backend](../../../../ThirdParty/imgui/backends/imgui_impl_glfw.cpp)

`imgui` target 通过公开编译定义 `IMGUI_USE_WCHAR32` 统一核心、GLFW backend、Editor 与 ImGuizmo 使用的 `ImWchar` 布局，使 UI 帧消费保留宿主提交的补充平面标量。编译定义随 ImGui 的使用关系传播，Runtime 不直接使用该 ABI。 [ImGui 目标](../../../../ThirdParty/CMakeLists.txt) · [宿主字符合同](../../../specs/window-input/spec.md#requirement-preserve-key-transitions-and-committed-text-meaning)

### 2. 面板状态归 Session，业务状态沿窄入口读取

在主 viewport 先建 `DockSpaceOverViewport`，使用固定面板表和稳定 ID 提供 Viewport、Hierarchy、Inspector。View 菜单和一个面板显隐快捷键都提交同一 `ActionId`，以便首包就有可观察的动作消费者。Session 持有显隐和会话内布局，面板只持自身展示状态。首包关闭 ImGui 默认的自动 ini 读写，使布局独立于运行目录。后续持久化可沿 ImGui 的工作区保存能力接入。 [面板与动作合同](../../../../Docs/Architecture.md#editor-input) · [布局与场景存储边界](../../../../Docs/Architecture.md#asset-persistence)

选择是可空、类型隔离的身份值。首包没有领域对象可选，初始选择为空。真实领域 owner 接入时负责验证选择的身份及代，面板通过只读数据和字段级动作访问它。当前各空状态只展示可用信息与被禁用的操作。固定面板和类型化动作表足以支持这个工作区，无需提前引入领域存储或插件机制。 [类型化身份](../../../../Engine/Core/Identity.hpp) · [领域事务](../../../../Docs/Architecture.md#edits)

### 3. 业务输入按当帧归属仲裁

GLFW 回调按原生链分别进入 ImGui 和 Platform。Editor 从 `HostFrameInput` 消费业务输入，正常路径的按键和字符由官方 backend 送入 ImGui。平台和 ImGui 的 NewFrame 完成后记录实际键盘占用，布局后合并当前弹出层和活动项，再与视口内容命中一起交给统一路由。布局中关闭弹出层或释放活动项时，已占用的键盘输入在本批次内仍归 UI。作用域和启用条件共同决定动作是否可用。同一业务事件只选一个 owner，使用有序 press/modifier 辨识一次性快捷键，滚轮和指针使用既有逻辑单位。 [窗口输入合同](../../../../Docs/Architecture.md#input) · [视口输入合同](../../../../Docs/Architecture.md#viewport-input)

按输入通道先判文本或模态等实际 UI 占用，再判活动 Gizmo、已生效的相机捕获，最后判可命中的视口或有资格的快捷键。当帧控件和内容矩形决定当前归属，`WantCapture` 只作为辅助信号。T01 由面板动作消费这套路由。Gizmo、相机和领域动作在各自消费者就绪时启用。相对模式经现有 `HostRequest` 提交，观察到实际模式生效才视为捕获，失败交给既有宿主结果路径。 [Editor 输入解释](../../../../Docs/Architecture.md#editor-input) · [宿主请求合同](../../../specs/window-input/spec.md#requirement-apply-requests-through-an-explicit-safe-boundary)

### 4. 失焦和暂停先结束交互，再进入下一帧

结合有序 `FocusEvent`、`IconifyEvent`、像素范围变化和发布末值识别失焦或暂停，覆盖同一批次内失焦又恢复的情形。批次一旦跨过失效边界，即撤销旧拖拽和捕获、跳过本批可触发业务动作的键鼠输入。宿主已在失焦时安排相对模式释放。暂停时若 Editor 仍有生效的捕获，通过现有请求入口归还。恢复后的新按下才可重建捕获，位移基线沿宿主合同重置。 [宿主焦点合同](../../../specs/window-input/spec.md#requirement-reset-held-and-relative-motion-state-at-focus-boundaries)

暂停期间 Runtime 仍 poll 并处理关闭，私有适配停止接受常规 UI 交互输入，清掉已排队事件、键鼠状态及独立的点击历史。官方焦点事件可以独立入队，恢复时按当前焦点事实同步 ImGui。恢复发布已经观察到普通模式时，在本轮末开放输入。若同批跨过零像素边沿且末值仍为相对模式，门控保持到 Runtime 确认归还成功。Runtime 在等待事件前通知会话完成恢复，窗口仍暂停或失焦时继续等待有效发布。 [宿主请求合同](../../../specs/window-input/spec.md#requirement-apply-requests-through-an-explicit-safe-boundary)

适配在开放输入时用已借用的窗口取一次当前逻辑指针位置。归还可能改变绝对坐标，GLFW 也可能过滤重复位置通知。该采样只为在下一批回调前建立 UI 基线。业务输入由 Platform 发布。首个有效单击可以在指针静止时命中，连续有效点击沿用 ImGui 的多击判定。 [私有窗口借用](../../../../Platform/GlfwAccess.hpp) · [ImGui 输入队列行为](../../../../ThirdParty/imgui/imgui.cpp)

### 5. CPU UI 帧沿唯一循环产生

```text
PollEvents --> exit check --> input cleanup
                                  |
                      +-----------+-----------+
                      v                       v
                 renderable               suspended
           NewFrame, panels, route     cancel interaction
                  ImGui::Render              |
                      +-----------+-----------+
                                  v
            apply requests --> complete eligible input recovery --> finite event wait
```

终止轮次在建 UI 帧和等待之前退出。继续运行且窗口可渲染时完成一个 CPU UI 帧，暂停轮次只处理输入清理、平台请求和有限等待，恢复首帧重新建立 UI 时间基准。布局使用逻辑尺寸，`DrawData` 的像素比例来自当前有效 framebuffer 与逻辑尺寸，内容缩放用于独立的 UI 尺寸或字体策略。 [帧循环](../../../../Docs/Architecture.md#frame-flow) · [窗口单位](../../../../Docs/Architecture.md#input)

固定版本要求在无纹理 backend 时先建 CPU 字体 atlas。T01 用默认嵌入字体在首次 NewFrame 前初始化 CPU atlas，输出 `ImGui::Render` 的数据。`DrawData` 保留在 Editor 内，后继 UI 渲染消费者只能在同帧读取，跨 `NewFrame` 或 Session 退出前须拷贝所需数据。T02 有了实际纹理消费者后移除这项过渡初始化，启用正式纹理协议。 [字体 atlas 前置条件](../../../../ThirdParty/imgui/imgui_draw.cpp) · [UI 数据寿命](../../../../Docs/Architecture.md#imgui-renderer)

### 6. 后续工作包沿现有边界接入

| 工作包 | 承接的输入和交付边界 |
|---|---|
| T02 | 消费同一 Session 的 `DrawData`。通过正式 Shader 包和公共 RHI 建立绘制。纹理更新在实际执行且有效提交后确认，失败时清理暂赋 `TexID` 并保留请求以供下一帧重试。 |
| T03 | Runtime 将宿主持有的 `PresentColor` 交给 UI 绘制。关闭列表后登记源保活并执行后端末端复制，`Execute` 插 fence 并 flush。有效帧取得票据后交换，源引用按完成值退休。 |
| T04 | 以真实提交扩展有界帧槽，分离稳定 `ViewId`、当前输出代与窗口目标，将可诊断的测试视口接到现有面板。 |
| T05 | Session 的动作入口把有效操作送入实际 View/编辑器事务。标题和 VSync 请求沿 Platform 边界应用。 |

这些衔接共享 Session 和唯一 Runtime 入口，阶段出口以各包实际证据汇总。 [阶段验收](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d04)

## Risks / Trade-offs

| 风险 | 约束与验证 |
|---|---|
| 回调重复投递或拆除后残留旧会话 | 用真实 GLFW 输入同时观察两个消费者的投递与顺序。拆除后让同一宿主继续 poll，再装配新会话。 |
| 同批次焦点往返或暂停输入恢复成旧动作 | 验证跨失效边界的批次只取消旧交互，恢复后从新输入开始。以动作触发次数和滚轮归属观察结果。 |
| CPU 帧借用或尺寸混用 | 用连续帧和不同逻辑尺寸、像素比例、内容缩放核对消费时点与数据单位。 |
| 提前退出或初始化中途失败 | 核对终止批次无额外 UI 帧和等待，失败时先拆 Editor 再关 Host，并保留有界首错。 |
