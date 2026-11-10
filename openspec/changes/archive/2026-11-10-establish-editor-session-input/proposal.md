# Proposal

## Why

D03 已通过 Shader/RHI 的原生图像门槛，当前产品入口仍只消费窗口事件。D04-T01 在同一个 Runtime 接入编辑器会话、停靠面板和统一输入路由，为随后接入 UI 绘制提供真实的 CPU 帧消费者链。 [D04-T01](../../../../Docs/Developments/D03-D05-FramePipeline.md#d04-t01) · [编辑器输入合同](../../../../Docs/Architecture.md#editor-input)

## What Changes

- 在可见与隐藏启动路径上装配同一个 `EditorSession`，接入仓库已固定的 Dear ImGui docking 与官方 GLFW 回调链。Session 在宿主仍存活时拆除适配，恢复原回调。
- 在主窗口建立停靠区、面板显隐和可空的类型化选择。首批面板为 Viewport、Hierarchy 和 Inspector，依赖未就绪领域数据的操作显示不可用。
- 让面板和快捷键通过同一个有作用域和启用条件的动作入口。当前帧集中判定文本、模态、Gizmo、相机捕获及视口输入的归属。失焦和暂停会取消捕获并清理积压输入。
- 在继续运行的可渲染窗口轮次生成 CPU `DrawData`。D04-T02 消费该数据进行 RHI 绘制，T03 接入窗口呈现，T04 接入帧槽和视口输出，T05 落地领域事务与设置请求。 [D04 阶段计划](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d04)

## Capabilities

### New Capabilities

- `editor-session-input`: 编辑器会话、回调接合与恢复、单窗口布局、动作路由和 CPU UI 帧借用。

### Modified Capabilities

- `runtime-lifecycle`: 在唯一产品入口装配并驱动编辑器，明确终止轮次、暂停恢复与逆序释放的可观察行为。

## Impact

- 实现入口为 `Editor/Session`、`Editor/Panels`、`Editor/ImGui`、模块 CMake 和 `Runtime/Main.cpp`。Editor 使用 `HostFrameInput` 和 `HostRequest` 值合同，只有私有平台适配借用原生窗口。 [依赖方向](../../../../Docs/Architecture.md#overview)
- 复用 Platform 已有输入发布和延迟请求接口。窗口所有权、输入单位、回调恢复和请求结果以 [window-input](../../../specs/window-input/spec.md) 为准。
- 验收覆盖受影响的 Debug/Release 产品、真实 GLFW 回调和恢复路径，以及 CPU 布局、路由和数据借用。D04 阶段的像素、窗口呈现及在途寿命由各自后续工作包验收。
