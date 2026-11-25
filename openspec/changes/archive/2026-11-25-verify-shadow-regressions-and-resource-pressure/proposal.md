# Proposal

## Why

[D09-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t05) 需要证明四项已验收的阴影能力在同一正式 Forward 执行链上组合后仍满足覆盖、预算和跨帧寿命合同。D09 在可重放的图像证据和原生压力结果齐备后验收，届时将同一输入的 Forward 画质与成本交给后续路径比较。

## What Changes

- 固定稳定 CSM、级联过渡、聚光锥边、点光六面和接缝的接收标样。核对薄片、MASK、镜像、静态实例及画外 caster 在实际 D32、可见性和 HDR 中的结果。
- 将多灯超额、区域再分配、关闭与恢复阴影、View 尺寸往返及在途退休串为受控回放。将准入与降级、图层访问、提交完成、池容量及原生命令逐项对应。
- 采集 Off、Hard 和 PCF 的画质与成本，包含一个仅改变阴影分辨率的对照。记录实际画质、接受帧和取样条件，供 [D10-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t05) 和 [D11-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t05) 重放相同阴影输入。
- 复用 [D09-T01 至 D09-T04](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t01) 的已采纳合同和身份匹配的证据。用 [验收规格](design.md#verification-matrix) 对应 D09 阶段出口的每一项条件。

## Capabilities

### New Capabilities

无。本工作包交付可重现的阶段验收证据与 Forward 参考。

### Modified Capabilities

无。使用 `skip_specs: true`，按现有阴影、光照、图访问、池与完成合同判定结果。若复现的问题要求改变已采纳行为，先修订 change 的范围与对应规格。

## Impact

- 验收范围为当前 Windows x64/MSVC、真实 OpenGL 4.6 Graphics 及静态 Forward。使用 Runtime、Renderer、RenderGraph、RHI 的正式路径，记录实际覆盖的构建配置、设备、驱动和场景。
- 回放代码、标样和原始输出归 `build/d09-t05/`。将复现所需的输入和口径、定量结果及已知限制留在 change 与可复用的参考记录内。 [合同验证](../../../../Docs/Architecture.md#tests) · [测量合同](../../../../Docs/Architecture.md#benchmark)
- A 自检及交接后由 B 独立审查和复核。由 A 在 B 签收版上执行最终验证，以实际结果更新 D09 出口和 [D10 入口](../../../../Docs/Developments/D08-D11-RenderPaths.md#dev-d10)。 [独立审查闭环](../../../../AGENTS.md#review-loop)
