# Proposal

## Why

D04-T01 已建立 EditorSession 与输入路由，D04-T04 已让主视口持有稳定身份并呈现诊断颜色。D04-T05 需要让编辑意图在帧边界修改权威状态，使拖拽可撤销、视口能解释请求为何尚未生效、窗口设置能报告原生应用结果。 [D04-T05 工作包](../../../../Docs/Developments/D03-D05-FramePipeline.md#d04-t05) · [D04-T01 验收](../2026-11-10-establish-editor-session-input/tasks.md#final-verification) · [D04-T04 验收](../2026-11-11-establish-frame-slots-and-viewport-output/tasks.md#final-verification)

## What Changes

- 以主 View 的渲染比例和主视口的面板显隐接通字段级编辑。离散操作各形成一次历史记录。拖拽逐帧应用最新值，释放时合并为一次撤销，失效时取消预览。菜单和有效快捷键沿现有动作路由触发撤销与重做。
- View 保有请求比例及最后接受的有效比例和输出尺寸。Editor 按本帧内容矩形、framebuffer 比例和请求比例准备实际诊断视口。资源暂不可用时显示 Pending，超出能力或预算时给出原因。有效比例随匹配的诊断输出获得提交接受而发布。
- EditorSession 管理面板显隐和编辑历史。DeviceHost 分别应用窗口标题与 VSync，提供请求值、原生确认值及失败原因。面板从各 owner 的只读快照显示状态。

## Capabilities

### New Capabilities

- `editor-domain-edits` 定义主 View 和会话布局的字段级编辑、连续事务、撤销重做及会话结束。

### Modified Capabilities

- `editor-session-input` 将主视口显隐、撤销和重做接入停靠工作区与当前帧动作路由。
- `editor-viewport-output` 将实际渲染比例纳入离屏尺寸请求，公开 requested、effective、Pending 和失败的边界。
- `native-window-host` 为活动窗口增加各自独立的标题与 VSync 请求，记录原生确认及可诊断失败。

## Impact

- `Editor/Commands` 提供两类已实现编辑的事务和历史，Session 组织应用，Panels 产生意图，Viewport 生成封存帧的像素请求。Renderer/View 负责接受比例请求和发布诊断输出。Platform 私有边界核对 Win32 标题与 WGL 交换区间。Runtime 负责按主循环顺序连接这些入口。 [领域命令](../../../../Docs/Architecture.md#edits) · [视口输入](../../../../Docs/Architecture.md#viewport-input) · [配置准备](../../../../Docs/Architecture.md#plan-readiness)
- D05 接入整帧 RenderGraph，D06-T06 扩展到场景字段和相机操作，渲染路径准备与切换依其后续工作包交付。本 change 提供这些工作可以消费的会话编辑和请求边界。 [D05](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d05) · [D06-T06](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t06) · [路径切换合同](../../../../Docs/Architecture.md#plan-readiness)
