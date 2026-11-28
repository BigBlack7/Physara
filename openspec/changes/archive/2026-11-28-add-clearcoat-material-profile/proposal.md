# Proposal

## Why

D12-T01 已交付 IOR/specular 的有效材质、着色与三路径准入。D12-T02 要让带 `KHR_materials_clearcoat` 的静态不透明材质在同一链路交付清漆高光、底层衰减和正确的可见表面，使清漆的直接光、环境光和实例编辑一起可验收。 [D12-T02](../../../../Docs/Developments/D12-D13-Materials.md#d12-t02) · [D12-T01 验收](../2026-11-28-add-ior-specular-material-profile/tasks.md#final-verification)

## What Changes

- 接入 glTF/GLB 的 clearcoat 声明、强度、感知粗糙度和独立法线槽。读取线性 R/G 通道、normal scale、每槽采样及 UV 变换，报告有效的必需扩展和不合规材质。
- 在 Inspector 与 v1 场景编辑、保存和恢复清漆稀疏覆盖。选定实例的有效材质驱动可见路由和历史失效，长生命周期的固定源模型继续供各实例继承。
- 在已有 OPAQUE/MASK 受光范围内，让清漆层覆盖核心 metallic-roughness 或 D12-T01 的 IOR/specular 基底。两层分别取得法线和粗糙度，清漆使用固定 IOR 1.5，在 Direct Lighting、Physical Lighting 及基底 emission 中应用一致的层权重。
- 以有限的清漆颜色角色交付 Forward、Forward+ 和 Deferred 的工作色。Deferred 沿用四附件核心 GBuffer、D32 和 SurfaceAux，以同一冻结几何与 MASK 判定交付扩展覆盖和 HDR。
- 把实际清漆颜色的活动资源、冷包与候选准入纳入现有 Shader 代流程。独立表面、深度、阴影和诊断按各自的消费范围准备。

## Capabilities

### New Capabilities

### Modified Capabilities

- `static-gltf-import` 读取 clearcoat 标准字段、纹理、切线前提和 CPU 支持报告。
- `editor-domain-edits` 编辑清漆因子、独立法线及槽位，并交付实例覆盖、失败原因和历史语义。
- `scene-document-persistence` 在 v1 中往返保存清漆字段与图像引用。
- `static-surface-evaluation` 定义双法线、清漆直接波瓣和基底出射权重。
- `environment-lighting` 交付与清漆视角、粗糙度和预积分匹配的 Physical Lighting。
- `static-forward-rendering` 根据生效的层与基底选择受光角色，并将清漆颜色接入已有辅助表面覆盖。
- `shader-generation-reload` 明确清漆颜色的结构角色和各阶段准入。

## Impact

- Asset、Scene 和 Editor 消费新增标准字段及实例覆盖。Renderer Draw/Materials、StaticForward 生成接口、Shader 目录和 Runtime 装配消费同一材质身份及绑定配方。 [材质与能量合同](../../../../Docs/Architecture.md#materials) · [Shader 就绪合同](../../../../Docs/Architecture.md#plan-readiness)
- 静态几何、共享 BRDF、逐灯和环境光消费者使用独立的 coat frame 及匹配的 DFG。验证复用现有参考环境和三路径读回，添加可区分双法线、基底和 emission 权重的清漆标样。 [静态几何](../../../../Docs/Architecture.md#geometry) · [光照聚合](../../../../Docs/Architecture.md#lighting) · [同输入比较](../../../../Docs/Performance/ThreePathReference.md#quality)
- 其它材质波瓣及阶段级组合验收由 D12-T03 至 T06 负责，透明和透射由 D13 负责。 [后续范围](../../../../Docs/Developments/D12-D13-Materials.md#d12-t03)
