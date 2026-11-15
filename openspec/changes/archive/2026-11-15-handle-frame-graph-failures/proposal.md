# Proposal

## Why

[D05-T04](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t04) 已把编辑器帧接入统一 RenderGraph, 当前产品仍在普通图错误后结束运行, 尚无编译失败后的安全 UI 图。本 change 覆盖 [D05-T05](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t05) 的完整目标: 可恢复错误保留编辑器和最后有效输出, 无法继续的工作按真实提交与关闭协议收拢, 为 D05 阶段出口补齐失败退路。

## What Changes

- 区分准备、构图/编译、执行、关闭、末端复制及提交失败, 保留有界首错与可定位的阶段、Pass/资源信息。普通帧取消继续事件和退休推进; Fatal 锁存停止新工作, 与设备/context 是否可用分别处理。
- 主图声明、封存或编译失败且尚未开放即时列表时, 最多重新构建一次独立 UI-only 图。销毁失败候选与其帧内绑定, 显式导入同一 View 上次接受且未被覆写的输出; 没有有效旧输出时显示带原因的占位。
- fallback 沿既有 UI Shader、上传、编译、执行和窗口呈现链完成。其成功只确认自身实际完成的 UI 工作, 不发布失败主图的候选设置、上传版本或场景历史, 使用 `NoSceneOutputs` 语义。
- fallback 准备/编译失败时取消取得; 已开始执行后不追加 fallback。可合法关闭并提交的失败前缀按 ticket/Event 退休, 失去可靠状态的窗口目标在完成后换代。不能取得可靠关闭、提交或完成证明时受控退出。
- 保持 UI 更新可重试及帧槽、取得 token、已有输出和在途引用的明确 owner。补齐准备期 Fatal 的即时门控, 不通过重新捕获错误计数把失败候选改记成功。
- 用受影响的现有消费者与有限故障注入检查正常帧、独立 fallback、普通取消、前缀退休及致命退出; 保留 A/B 独立审查、最终 verify 与 D05 出口核对任务。

本次继续采用现有单窗口、三个帧槽、单 Graphics 即时列表及诊断 View profile。真实 Scene、TAA/SSR 历史、产品截图/拾取、通用资源池、设备透明恢复和 Vulkan 实现仍按后续工作包推进。读回准备失败在临时图消费者中验证当前编排边界, 不提前引入产品读回系统。 [主循环](../../../../Docs/Architecture.md#frame-flow) · [图编译失败](../../../../Docs/Architecture.md#graph-compile) · [取消与关闭](../../../../Docs/Architecture.md#shutdown)

## Capabilities

### New Capabilities

无。本次扩展现有编辑器帧图、View 输出及宿主生命周期合同。

### Modified Capabilities

- `editor-frame-graph`: 明确错误分类、独立安全 UI 图、尝试间诊断与 Fatal 门控; 区分失败主图和成功 fallback 的发布资格。
- `editor-viewport-output`: 安全 UI 图可显式使用同一 View 的最后接受输出, 即使其尺寸/请求代与当前请求不同; 显示旧输出状态并保持有效设置不变。
- `opengl-window-presentation`: 明确执行前取消与有票据前缀取消的完成责任, 在安全完成后重建失败目标, 只对无法证明安全的路径停止新帧。
- `runtime-lifecycle`: 可恢复帧错误经诊断和取消后继续主循环; 保留启动失败、Fatal、无法收拢及关闭失败的退出码和所有权顺序。

## Impact

- `Runtime/Main.cpp`: 组织正常候选、单次 fallback、取消/终止及提交后发布, 不另建 Renderer。
- `Engine/Renderer/View.*`、`Engine/Renderer/FrameGraph.*`: 提供已接受输出的拥有型只读快照, 沿现有图声明组织清除与上传。
- `Editor/Session`、`Editor/ImGui` 及直接消费诊断的 `Editor/Viewport` / `Editor/Panels`: 明确正常与安全 UI 输入, 重建准备值和帧内绑定, 显示失败与旧输出状态。
- `Platform/DeviceHost*`、`Platform/RhiDeviceHost.cpp`、`Platform/Presentation.*`: 在已有消息适配器锁存 Fatal, 处理活动取得与失败目标的退休/换代。沿用 RHI/RenderGraph 已有错误和保活合同; 不增加公共 RHI 错误框架。
- 更新本 change、开发计划状态和实现后的必要使用说明; 架构指南、主 specs 及历史归档在本次规划中保持不变。开发验证材料只写入 `build/d05-t05/`, 不新增依赖、测试框架或正式故障开关。
