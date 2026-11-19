# Proposal

## Why

D07-T02 与 D07-T04 已交付同帧预曝光、直接光和 IBL 的工作附件，当前视口将超白工作色直接限幅。D07-T05 接通正式 SDR 显示链，使高亮和色彩在视口及窗口有可复验的参考，并交付单样本 None/FXAA 与基础空间缩放。 [工作包](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d07-t05) · [显示架构](../../../../Docs/Architecture.md#display-output) · [HDR 验收](../2026-11-18-add-forward-environment-lighting/tasks.md#final-verification)

## What Changes

- Physical Lighting 使用固定 Filament ACESToneMapper/ColorGrading profile，将当前预曝光 linear Rec.709/D65 工作色映射为 display-linear SDR。Forward 继续交付独立的显示前 HDR 比较输入及其曝光、相机和采样范围。
- 在显示变换后按请求执行基础 FXAA，再按面板需要做线性空间缩放。正常场景 View 明确渲染栅格、输出栅格和有效图像范围，窗口目标仍由 framebuffer 像素决定。
- View Settings 提供 None/FXAA，展示请求与最后接受的实际 AA、profile 和尺寸。v1 场景中的合法主 View AA 请求可保存和恢复，缺省为 None。诊断预览提供线性限幅的数值参考。
- 通过实际 Forward、UI 和窗口链核对颜色纹理解码、线性数据、自发光、高亮、边缘、留边和 sRGB 编码。物理 Inspector、参考场景集及 D07 阶段出口由 D07-T06 承接。 [颜色约定](../../../../Docs/Architecture.md#conventions) · [D07-T06](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d07-t06)

## Capabilities

### New Capabilities

- `display-post-processing` — 固定 SDR 显示 profile、感知亮度 FXAA、基础空间缩放，以及同帧 HDR 和显示输出的消费合同。

### Modified Capabilities

- `static-forward-rendering` — Physical Lighting 从限幅预览进入固定显示 profile，保留原有材质诊断和显示前工作色。
- `view-preparation` — 为同一投影明确显示画布和渲染栅格各自的有效矩形、采样尺寸及映射。
- `editor-viewport-output` — 在同一 View 和 token 下按帧解析两种尺寸，并用实际正常帧确认显示 profile、AA 和布局。
- `scene-document-persistence` — 主 View 的 v1 请求支持可省略的抗锯齿选项。

## Impact

- Renderer/PostProcess 与 Shaders/PostProcess 承接固定 LUT、显示变换、FXAA 和缩放；Forward 提供工作色、深度和实际 View 布局，Runtime 沿既有图和包入口组装。
- View、Editor View Settings 与场景文件边界承接类型化 AA 请求、真实接受状态和兼容恢复。UI 在线性域合成，Platform 继续负责窗口编码和呈现。 [模块边界](../../../../Docs/Architecture.md#editor-output) · [生成包入口](../../../../Docs/Build.md)
