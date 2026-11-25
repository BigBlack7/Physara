# Proposal

## Why

D09 已在静态 Forward 中交付完整的直接光、阴影和冻结画质基准。D10-T01 为同一个 View 的有限范围灯建立空间候选，使 Forward+ 可以减少局部灯求值，同时保留完整灯表定义的照明。 [D09 阶段出口](../2026-11-25-verify-shadow-regressions-and-resource-pressure/tasks.md#final-verification) · [D10-T01](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t01)

## What Changes

- 从当前冻结 View 的稳定有限相机和有效渲染矩形构造 Froxel 网格。无限远透视沿用有限裁剪远面并采用对数分层，正交采用允许近面为零的线性分层。
- 对有限范围的点光、聚光保守分箱，按现有着色灯表行号压紧候选。方向灯保持独立，无显式范围的局部灯按全局候选交付。
- 通过现有生成接口定义 CPU 与 Shader 共用的网格参数、header 和 32 位索引，以及可定位的不可用结果。CPU 分箱和查询结果可用独立的灯覆盖参考验收。

## Capabilities

### New Capabilities

- `froxel-light-preparation` 定义每 View 的稳定网格、保守候选、32 位数据布局和容量边界。 [增量规格](specs/froxel-light-preparation/spec.md)

### Modified Capabilities

## Impact

- `Renderer/Prepare` 提供同轮的相机、采样范围和可见工作集，`Renderer/Lighting` 构造按需的 CPU 列表，Shader 生成接口提供数据布局及对应的查询输入。沿用 `scene-frame-preparation`、`view-preparation`、`punctual-lighting` 和 `shader-build` 的既有合同。 [相机](../../../../Docs/Architecture.md#view-preparation) · [灯准备](../../../../Docs/Architecture.md#light-preparation) · [完整灯表](../../../specs/punctual-lighting/spec.md)
- 本 change 交付 D10-T01。T02 使用本 ABI 接通容量溢出退路、图内上传和统计，T03 接通共享着色的 Forward+ 变体，T04 交付按 View 的原子切换，T05 验收同输入 HDR 等价与多灯成本。现有 Forward 继续使用完整灯集合。 [D10 后续工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t02)
