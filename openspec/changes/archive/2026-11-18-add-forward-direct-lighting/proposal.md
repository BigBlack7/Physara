# Proposal

## Why

D07-T01 已交付共享表面与核心 BRDF，D07-T02 已交付当帧预曝光输入。场景中的标准灯已能导入、保存并进入 View 的可见工作集，D07-T03 让正式 Forward 首次用这些灯产生可量化的直接光。 [D07-T03](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d07-t03) · [表面与 BRDF 验收](../2026-11-17-add-core-surface-brdf/tasks.md#final-verification) · [预曝光验收](../2026-11-17-add-physical-camera-exposure/tasks.md#final-verification)

## What Changes

- 消费当前 `PreparedView` 的完整灯索引。方向灯独立于局部灯，点光和聚光保留原强度、线性颜色、有限范围或无距离截断语义。继续使用 SceneFrame 的世界位置、发射方向和保守裁剪结果。 [物理输入](../../../../Docs/Architecture.md#physical-inputs) · [灯列表](../../../../Docs/Architecture.md#light-preparation)
- 用生成的 Shader 接口把灯记录、有效数量和相机输入交给正式 Forward 颜色 Pass。图声明上传与读取，帧槽管理在途数据。完整灯表无法准备时，沿现有待确认或拒绝路径报告原因。
- 通过统一的 `EvaluateLighting`、共享表面与 BRDF 交付方向光、点光和聚光。受光表面的直接光与自发光使用当帧预曝光，工作附件保留可表示的线性超白值。
- View Settings 的 `Direct Lighting` 模式通过现有视口呈现结果。现有材质诊断、Unlit 基色、MASK 深度覆盖及模式确认语义继续适用。 [直接光与不透明输出](../../../../Docs/Architecture.md#opaque-shading)
- 本包验收物理单位、距离及角衰减、完整候选叠加、零灯、曝光和失效后的回执。环境与 DFG 在 D07-T04 接入，固定显示 profile 在 D07-T05 接入，灯光编辑在 D07-T06 接入，阴影及其他路径按 D09–D11 各自接入。 [D07 工作包](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d07) · [后续路径](../../../../Docs/Developments/D08-D11-RenderPaths.md)

## Capabilities

### New Capabilities

- `punctual-lighting` — 消费每 View 的完整可见灯集合，交付匹配帧的灯数据并规定三类灯的直接入射与聚合。

### Modified Capabilities

- `static-forward-rendering` — 增加可选择的直接光模式，规定正式颜色 Pass 的受光表面、预曝光自发光和视口结果。

## Impact

- 主要落点为 `Engine/Renderer/Lighting`、现有 Forward 帧准备及图、`Shaders/Lighting`、生成的 `StaticForward` 接口和 View Settings。glTF 与 v1 文件沿现有数据合同读写。 [灯光导入](../../../specs/static-gltf-import/spec.md) · [场景帧](../../../specs/scene-frame-preparation/spec.md) · [View 工作集](../../../specs/view-preparation/spec.md)
- `StaticForward` 和 `StaticDepth` 使用同一新接口代，正式包由现有构建生成。开发标样及探针位于 `build/d07-t03/`，验证沿实际 Forward、Runtime 图和编辑器入口。 [构建与运行](../../../../Docs/Build.md) · [Shader 合同](../../../../Docs/Architecture.md#shader-build)
