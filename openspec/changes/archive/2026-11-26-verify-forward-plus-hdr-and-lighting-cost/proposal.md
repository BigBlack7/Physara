# Proposal

## Why

[D10-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t05) 承接已归档的 [D10-T01 至 D10-T04](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t01)。D10 阶段出口需要用同一冻结输入的正式 Forward 与 Forward+ 输出证明 HDR 等价，说明灯数增加、候选收缩及退路对实际成本的影响，并为 Deferred 留下可重放的比较基准。

## What Changes

- 用真实 OpenGL 正常提交，从同一冻结 SceneFrame 的两个不透明策略取得显示前线性 HDR 和 Reverse-Z 深度。复现 [D09 Forward 阴影参考](../../../../Docs/Performance/ForwardShadowReference.md)，锁定相机、曝光、有效阴影与 IBL、画布与有效矩形及画质，记录全图误差和可判别的受光标样。
- 用受控照明场景覆盖无显式 range 的点光与聚光、斜向 spot 边界、混合 overflow、有限分箱视域内外、极端相机与空灯。核对实际正常格和完整灯退路，证明可见受光贡献完整。
- 在同画质、同光源规模的接受帧上分开度量 CPU 分箱、Froxel 上传与 GPU 不透明颜色区间的局部灯增量。记录实际工作量、退路比例、异步查询归属及样本分布，解释少灯、多灯和溢出下的代价。
- 交付本 change 的验收规格、实施与审查清单。签收后把已测条件、误差和基准留给 [D11-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t05)。Deferred 的实现与验收由 [D11](../../../../Docs/Developments/D08-D11-RenderPaths.md#dev-d11) 承接。

## Capabilities

### New Capabilities

无。交付 D10 阶段出口的可重放验收及比较基准。

### Modified Capabilities

无。沿用已采纳的 [受光与输出](../../../specs/static-forward-rendering/spec.md)、[Froxel 列表与退路](../../../specs/froxel-light-preparation/spec.md)、[View 接受](../../../specs/view-preparation/spec.md)、[帧统计](../../../specs/render-frame-statistics/spec.md) 和 [参考场景](../../../specs/forward-reference-scene/spec.md) 规格。`skip_specs: true` 适用于本次验证型 change，[验收规格](design.md#acceptance-spec) 定义执行条件与通过判据。若复现需改变产品合同的缺陷，先更新范围及相应增量规格。

## Impact

- 验收使用正式 Runtime、SceneFrame、View、Renderer、RenderGraph、RHI 和 Editor 统计通道。适用范围由实际运行的 Windows x64/MSVC、OpenGL 设备、驱动及画质配置界定。 [合同验证](../../../../Docs/Architecture.md#tests) · [性能测量](../../../../Docs/Architecture.md#benchmark)
- 临时复现输入、归因观测与原始结果归 `build/d10-t05/`。签收后在 `Docs/Performance/` 保存有后续消费者的测量基准，change tasks 留存证据和限制。 [文档归属](../../../../Docs/Architecture.md#project-docs)
- A 完成本地自检后交独立 B 审查及复核。最终 verify、规格处理、归档及 D10 出口按 B 签收的版本办理。 [独立审查闭环](../../../../AGENTS.md#review-loop)
