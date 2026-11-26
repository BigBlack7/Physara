# Proposal

## Why

D11-T01、D11-T02 已验收固定核心 GBuffer 和最近不透明表面路由。目前最终 GBuffer 已能交给后继，核心工作色仍由调用方提供的 Pass 声明。D11-T03 要让 Renderer 从最后表面直接产生受光 HDR，为三路径共用的天空与显示链交付正式颜色和深度。 [工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t03) · [D11-T02 验收](../2026-11-26-add-deferred-surface-routing/tasks.md#final-verification)

## What Changes

- 用 Renderer 内部的显式准备入口组装 `PrepareSurface → 表面效果输入 → ShadeOpaque`。最终 GBuffer 提供同屏逐像素深度及材质，核心光照只处理最后 G3 中具有资格的像素，覆盖有限远平面的有效几何。
- 从当前渲染矩形和相机快照重建观察与受光位置，复用 Forward+ 的灯表、Froxel、逐灯可见性、DFG 和 IBL 聚合。G2 已含预曝光，自发光直接进入工作色。阴影接收偏移使用由几何深度估计的法线，并定义缺乏有效估计时的偏移语义。
- 交付统一 `OpaqueOutput`，用最终只读 D32 接续 Unlit 和已准入的 ForwardExtension，再由共享天空及显示阶段消费。屏幕效果关闭时，给光照提供单位屏幕遮蔽和零屏幕反射贡献。
- 本次提供当前单样本 Direct Lighting 与 Physical Lighting 的内部原生子图。D11-T04 处理 View/Editor 三路径配置和事务切换，D11-T05 处理等价、可视化与成本门槛，D14 接入 AO/SSR 生产节点。 [路径组装](../../../../Docs/Architecture.md#paths) · [不透明着色](../../../../Docs/Architecture.md#opaque-shading) · [后续工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t04)

## Capabilities

### New Capabilities

- `deferred-opaque-rendering` 定义正式核心光照、冻结 View 的资源消费、统一不透明输出和背景合成。 [增量规格](specs/deferred-opaque-rendering/spec.md)

### Modified Capabilities

- `shadow-rendering` 为屏幕着色补充接收几何法线的来源，以及有效局部几何无法重建时的法线偏移行为。 [增量规格](specs/shadow-rendering/spec.md)

## Impact

涉及 Renderer 的表面与光照准备、Shader 接口及产品包、OpenGL RenderGraph 的采样和只读深度消费，以及 `ForwardOutput` 的现有天空、显示和帧交付。复用已验收的 GBuffer 布局、灯光及阴影生产者、Shader 代准备和现有显示 profile。 [几何合同](../../../../Docs/Architecture.md#geometry) · [光照合同](../../../../Docs/Architecture.md#lighting) · [Shader 准备合同](../../../../Docs/Architecture.md#plan-readiness)
