# Proposal

## Why

D04-T03 已让 Docking 编辑器经真实 OpenGL 窗口呈现。当前宿主只有一个呈现槽，视口也只有等待文本。D04-T04 需要让 CPU 帧数据、窗口目标和视口输出按实际提交有界复用，为后续整帧 RenderGraph 提供可观察的输入与寿命合同。 [D04-T04 工作包](../../../../Docs/Developments/D03-D05-FramePipeline.md#d04-t04) · [D04-T03 验收](../2026-11-11-establish-opengl-window-presentation/tasks.md#final-verification)

## What Changes

- 将真实 UI 几何和上传数据接入 FrameArena。Runtime 调度初始上限为 3 的 FrameSlot，每槽的 CPU 存储和窗口目标按同一次提交的真实完成复用。槽用尽时保持有界背压，完成证明失败时保留已发工作并进入受控退出。
- 为主视口建立稳定的 `ViewId` 和逻辑 `UiTextureId`。每帧按最新有效内容矩形解析离屏输出代，视口 resize、字体 atlas 换代和窗口目标换代均沿原提交退休。
- 正式入口校验基础 UI 程序和字体。没有场景输出时仍呈现清屏后的 Docking UI，视口显示有明确标识的测试图或带原因的占位。`NoSceneOutputs` 保持 View 历史状态。
- 核对持续窗口及视口 resize、DPI、最小化恢复和最后一帧在途关闭。输入、布局和资源请求在帧边界确定，录制消费本帧已准备的数据。

## Capabilities

### New Capabilities

- `frame-slot-lifecycle` 规定 FrameArena 的容量和借用边界、槽的背压、接受票据与回收、暂停及在途退出。
- `editor-viewport-output` 规定主视口身份、输出代的帧内绑定、视口尺寸请求和 `NoSceneOutputs` 下的可见行为。

### Modified Capabilities

- `opengl-window-presentation` 让窗口目标取得显式对应 Runtime 帧槽，区分同一窗口不同槽的取得，并在该槽完成后按当前像素尺寸重建。
- `runtime-lifecycle` 让唯一产品入口呈现含诊断视口的编辑器，并明确基础字体初始化失败的进程结果。

## Impact

- Runtime 编排帧槽、输入、提交和退出。Engine/Core 提供短期 CPU 分配，Engine/Renderer 管理最小 View 输出。Editor/Viewport 发布布局请求，Editor/ImGui 解析本帧纹理。Platform/DeviceHost 按帧槽持有 PresentColor，使用现有 RHI 设备完成观察和提交保活。 [帧槽合同](../../../../Docs/Architecture.md#frame-state) · [视口纹理合同](../../../../Docs/Architecture.md#editor-output) · [历史与退休](../../../../Docs/Architecture.md#history-commit)
- DeviceHost 的 C++ 取得入口接收槽索引，Runtime 和受影响的开发夹具随接口更新。领域编辑命令与渲染配置请求归 D04-T05，RenderGraph 接入归 D05，场景数据与物理输出归 D06 及后续工作包。 [D04-T05](../../../../Docs/Developments/D03-D05-FramePipeline.md#d04-t05) · [D05](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d05) · [D06](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d06)
