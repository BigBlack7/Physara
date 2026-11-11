# Proposal

## Why

D04-T02 已能通过公共 RHI 合成离屏 UI，唯一产品入口仍只推进 CPU 编辑器帧。D04-T03 要让同一编辑器出现在实际窗口中，并让末端颜色复制、提交接受和资源退休沿同一帧的身份推进。 [D04-T03 工作包](../../../../Docs/Developments/D03-D05-FramePipeline.md#d04-t03) · [D04-T02 验收](../2026-11-10-establish-imgui-rhi-renderer/tasks.md#final-verification)

## What Changes

- 宿主按呈现槽取得匹配当前窗口像素范围的 `PresentColor`。Runtime 初始化底色，复用正式 UI Shader 包和 Editor 会话的公共 RHI 录制入口。
- 关闭列表后由 OpenGL 专用互操作复制有效颜色。登记本次源访问的保活后，现有 Graphics 提交插入 fence 并 flush，宿主才交换窗口缓冲。
- 取消失效目标和失败帧，保留已发工作至真实完成或受控退出。依据实际默认附件编码和物理行序复制，分别观察 `PresentWait` 和提交完成。

## Capabilities

### New Capabilities

- `opengl-window-presentation` 承载窗口目标取得、末端复制、交换资格、颜色与行序以及提交退休合同。

### Modified Capabilities

- `runtime-lifecycle` 将唯一产品入口从 CPU UI 帧推进到正式 RHI UI 合成和窗口呈现，明确实际呈现与启动失败的可观察结果。

## Impact

- Platform 持有呈现目标、取得身份和交换结果。RHI/OpenGL 增加窄互操作，复用原生状态缓存及在途包。Runtime 装配同配置的 UI 包、Editor 和验证设备，并组织帧内顺序。 [呈现合同](../../../../Docs/Architecture.md#present) · [主循环合同](../../../../Docs/Architecture.md#frame-flow)
- 本包的窗口帧由底色和已交付的 Docking UI 组成。通用 FrameSlot、连续 resize/DPI 的阶段验收及 ViewOutput 身份归 D04-T04，图编译和执行归 D05。 [后续工作包](../../../../Docs/Developments/D03-D05-FramePipeline.md#d04-t04) · [图执行](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d05)
