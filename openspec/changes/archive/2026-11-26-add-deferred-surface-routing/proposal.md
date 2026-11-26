# Proposal

## Why

D11-T01 已能输出最近的核心材质表面。D11-T02 要让同一 View 中遮住核心几何的 Unlit 和受支持的不透明扩展也参与表面准备，使深度、法线、粗糙度、核心照明资格和后续颜色在每个像素对应同一个最近表面。 [工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t02) · [D11-T01 验收](../2026-11-26-implement-core-gbuffer-codec/tasks.md#final-verification)

## What Changes

- 在冻结 View 的 Draw 工作集按有效材质解析核心、Unlit 和受支持的前向扩展路由。沿用静态实例、材质覆盖、MASK、双面和有序批次的几何合同。
- 在请求 Deferred 表面准备时，先绘核心 GBuffer，再由 SurfaceAux 接续写同一 D32、G1 和 G3。保留四颜色附件合计 18 B/像素的布局，明确区分背景、核心和两类辅助表面，包括有限远平面深度为零的情况。
- 交付核心照明资格判定，以及在最终只读深度上向同一工作色合成 ForwardUnlit/ForwardExtension 的图入口。以当前可用的核心 profile 和 glTF Unlit 验证遮挡、共面顺序、覆盖与颜色，供 D12 的材质扩展沿用。
- D11-T03 接续屏幕位置重建、共享 DeferredLighting 和天空拼接。D11-T04 开放三路径配置与切换，D11-T05 交付预览和等价、成本门槛。屏幕效果在 D14 消费最近表面合同。 [路径组装](../../../../Docs/Architecture.md#paths) · [几何与 SurfaceAux](../../../../Docs/Architecture.md#geometry)

## Capabilities

### New Capabilities

### Modified Capabilities

- `core-gbuffer` 定义最终表面分类、SurfaceAux 的匹配图版本和 ForwardUnlit/ForwardExtension 的等深合成。 [增量规格](specs/core-gbuffer/spec.md)

## Impact

涉及 Renderer 的 Draw 批次、材质驻留、路径图组装和 Shader 角色准备，以及生成的静态表面接口和产品包。新 Pass 复用现有混合附件、整数 fetch 和只读深度能力。普通 Forward 与 Forward+ 仍通过各自的正式帧入口，复用已解析材质与几何。 [Draw 合同](../../../../Docs/Architecture.md#draws) · [Shader 准备合同](../../../../Docs/Architecture.md#plan-readiness)
