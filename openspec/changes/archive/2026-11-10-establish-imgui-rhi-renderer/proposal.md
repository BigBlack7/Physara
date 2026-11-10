# Proposal

## Why

D04-T01 已能从唯一产品入口生成带停靠布局和输入状态的 CPU UI 帧。D04-T02 将这份 `DrawData` 转成公共 RHI 可执行的绘制与纹理更新，让下一包能把同一 UI 合成到窗口呈现目标。 [当前工作包](../../../../Docs/Developments/D03-D05-FramePipeline.md#d04-t02) · [D04-T01 验收](../2026-11-10-establish-editor-session-input/tasks.md#final-verification)

## What Changes

- 在 Editor 内接入正式的 ImGui 绘制消费者，向调用者提供非零离屏颜色目标上的 RHI 录制入口。接通现有 Shader 包、VB/IB 上传、逻辑投影、像素裁剪、索引和顶点偏移及 reset-state 命令。
- 接入固定 ImGui 的纹理 create/update/destroy 协议。以稳定且有代的 UI 身份解析资源，独立持有上传字节及区域。上传执行且随本次有效提交接受后确认状态，删除沿真实在途引用退休。
- 在线性域进行 straight-alpha 合成，以非对称离屏图像同时验收剪裁、索引位宽、sRGB 颜色与字体覆盖率。 [UI 绘制合同](../../../../Docs/Architecture.md#imgui-renderer) · [本帧纹理解析](../../../../Docs/Architecture.md#editor-output)

## Capabilities

### New Capabilities

- `editor-ui-rendering` 定义封存 UI 帧的公共 RHI 消费、绘制命令、颜色合成、纹理发布及退休行为。

### Modified Capabilities

## Impact

- 正式实现由 `Editor/ImGui`、`Editor/Session` 和 `Shaders/UI` 承担，接口声明进入 `Engine/Shader/Interface`。调用者提供公共 RHI 目标和提交结果。验收通过实际 OpenGL 设备的离屏目标，开发检查材料集中在 `build/d04-t02/`。 [构建合同](../../../../Docs/Architecture.md#shader-build) · [借用的 CPU 帧](../../../specs/editor-session-input/spec.md#requirement-publish-a-borrowed-cpu-ui-frame)
- D04-T03 装配 `PresentColor`、窗口末端复制和交换，D04-T04 承接有界帧槽及 View 输出身份，D05 接入图依赖。D04-T02 交付它们使用的 UI 绘制与纹理提交边界。 [阶段划分](../../../../Docs/Developments/D03-D05-FramePipeline.md#dev-d04)
