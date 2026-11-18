# Proposal

## Why

D07-T01 至 T03 已交付共享核心 BRDF、当帧物理曝光和可量化的直接光。D07-T04 需要把 HDR 环境、DFG、漫反射与镜面 IBL 以及天空接到同一个预曝光 HDR 输出，为后续显示和参考场景提供输入。 [D07-T04](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d07-t04) · [BRDF 能量合同](../2026-11-17-add-core-surface-brdf/design.md) · [直接光验收](../2026-11-18-add-forward-direct-lighting/tasks.md#final-verification)

## What Changes

- 从 `Assets/Environments` 加载有界的线性 HDR EXR，发布可固定的 CPU 版本及亮度标定。v1 场景可保存一个环境请求，显式选择照明与天空用途，恢复朝向和强度。 [环境资产](../../../../Docs/Architecture.md#asset-images) · [场景文档](../../../../Docs/Architecture.md#asset-persistence)
- 建立按源内容和烘焙 profile 查找的 `EnvironmentCache`，用冷路径生成基础 cube、GGX 预滤波层和 SH9，并共享固定 Filament 编码的 DFG。图声明并交付实际采样的面和 mip。 [环境准备](../../../../Docs/Architecture.md#light-preparation) · [光照合同](../../../../Docs/Architecture.md#lighting)
- 将独立浮点 cube 采样贯通 Shader 生成接口、包反射、图绑定及 OpenGL 视图。按设备的实际 cube 格式能力、尺寸和活动采样数量接收请求。
- 在现有 View Settings 提供 `Physical Lighting`。受光表面共用核心 BRDF、逐灯求值和同一 DFG 补偿，合成漫反射、镜面 IBL、直接光和自发光。天空覆盖有效图像矩形内的背景位置。视口显示请求、已接受环境和等待或拒绝原因。
- 本包交付可读回的预曝光工作色与现有视口链。固定显示 profile 和 FXAA 属于 D07-T05，物理 Inspector、参考场景和阶段出口属于 D07-T06。屏幕 AO、阴影及其他路径各沿其工作包接入。 [D07 后续范围](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d07-t05) · [屏幕效果与其他路径](../../../../Docs/Developments/D08-D11-RenderPaths.md)

## Capabilities

### New Capabilities

- `environment-lighting` — 生成匹配的环境资源与 DFG，规定缓存身份、间接光求值与完整环境代的交付。

### Modified Capabilities

- `cpu-asset-publication` — 发布受控 EXR 的版本化线性 HDR 像素与标定状态。
- `scene-document-persistence` — 保存和恢复可省略的场景环境请求，并在 Open 时取得匹配的 CPU 源版本。
- `shader-build` — 在生成接口、canonical IR 和包反射中保留浮点采样纹理的二维或 cube 维度。
- `opengl-shader-pipelines` — 准备活动的 cube 采样对及完整六面视图，在 Draw 前核对活动类型与实际绑定。
- `static-forward-rendering` — 提供正式的 `Physical Lighting` 模式及独立天空合成，沿用现有几何覆盖和显示出口。
- `editor-viewport-output` — 由匹配的正常帧确认有效环境，显示待处理请求与上次接受结果。

## Impact

- `Engine/Asset`、`Scene`、`Editor/Session` 承接 EXR、类型化环境请求和 v1 文档恢复。TinyEXR 复用仓内固定依赖。 [当前资产入口](../../../../Engine/Asset/AssetManager.hpp) · [固定依赖](../../../../ThirdParty/README.md)
- `Renderer/Lighting`、Forward 图、生成 Shader 接口及 `Shaders/BRDF`、`Lighting`、Forward Pass 承接预计算、驻留与 HDR 消费。`RHI/OpenGL`、Platform 能力桥和 Runtime 图 profile 交付真实 cube 采样能力。
- 正式构建生成匹配 ABI 的颜色和天空包。CPU 数值、包反射和原生 HDR 读回的开发材料集中在 `build/d07-t04/`。 [Shader 构建](../../../../Docs/Build.md) · [验证合同](../../../../Docs/Architecture.md#tests)
