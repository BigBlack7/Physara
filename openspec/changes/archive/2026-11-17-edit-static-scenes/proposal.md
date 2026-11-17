# Proposal

## Why

场景文档、相机准备和静态 Forward 已经接通，正式程序中的层级树仍是空占位，文件、实体编辑和导航缺少可操作入口。[D06-T06](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t06) 要让用户从同一编辑器加载模型、编辑真实场景、查看更新后的几何，并保存后继续工作。 [D06-T03](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t03) · [D06-T04](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t04) · [D06-T05](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t05)

## What Changes

- 层级树展示活动 Scene 的父子关系和当前选择；Transform Inspector 编辑本地 TRS。删除子树、导入实例、改变属性和主 View 请求使用同一会话事务及撤销重做入口。
- 正式 UI 提供场景 New、Open、Save、Save As，以及项目内 glTF/GLB 的导入和取消。显示未保存、文件进度、导入结果及 View 输出状态。导入的 CPU 结果匹配当前操作和文档后才可创建独立实例。
- 主视口接入 fly、orbit、pan 和已有相机实体选择。导航资格来自当前 UI 归属及匹配的有效图像，隐显及尺寸变化走现有 View 请求和输出代。JSON v1 恢复导航位姿、可选相机实体和 requested render scale；额外 View 的请求属于会话。
- D06-T07 负责本阶段的集成门槛，D07 负责物理 Forward，D16 负责完整资产浏览与重导入、Gizmo、GPU 拾取及扩展的编辑工作流。 [D06-T07](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t07) · [D07](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d07) · [D16](../../../../Docs/Developments/D16-D17-EditorAndDelivery.md#dev-d16)

## Capabilities

### New Capabilities

### Modified Capabilities

- `editor-domain-edits` — 接通层级选择、删除、本地 TRS、模型实例导入及连续字段撤销。
- `editor-session-input` — 统一文件、删除及现有菜单与快捷键派发，接通三种相机导航和失效时的捕获归还。
- `editor-viewport-output` — 用当前布局和匹配的已接受图像支持视口汇入的输入，延续像素范围、回执和退休合同。
- `scene-document-persistence` — 公开文件操作及未保存处置，验证从工作区保存与恢复主 View 的请求。
- `runtime-lifecycle` — 在同一 CPU 轮次推进文件和模型导入，接收窗口光标请求，场景编辑由既有准备及呈现链消费。

## Impact

- 主要落点为 Editor/Panels、Commands、Viewport、Session 和 Runtime 的 CPU 轮次。领域状态沿现有 Scene、View 和 Asset 入口发布，Renderer 继续消费当轮场景与相机快照。 [领域事务](../../../../Docs/Architecture.md#edits) · [视口输入](../../../../Docs/Architecture.md#viewport-input) · [视口输出](../../../../Docs/Architecture.md#editor-output)
- 从正式编辑器操作验证保存重开、带非均匀及负缩放父级的本地变换和撤销、选中实体删除、三种导航及视口恢复。用可重现的同一小型 glTF 场景核对实际 OpenGL 输出和请求身份；开发验证材料放入 `build/d06-t06/`。
