# Proposal

## Why

D09-T01 已从每个 View 的稳定相机和冻结场景交付有预算的阴影计划，但方向光只有一个 CPU 拟合段。D09-T02 要把主方向光的计划接入正式帧图，让视口可以验证稳定级联、画外遮挡和级联过渡。 [D09-T01 验收](../2026-11-24-prepare-shadow-plans/tasks.md#final-verification) · [D09-T02](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t02)

## What Changes

- 在稳定有限主视锥内规划 1–4 级方向光阴影。为各级生成包含过渡带的接收范围、保守 caster、固定尺度的 texel 对齐投影和可追溯的区域版本。
- 以共享静态几何、实例及 MASK 覆盖生产 D32 深度层。图按层声明写入和采样，阴影使用独立的常规深度约定和 caster bias。
- 在实际 Forward View 中查询范围、级联和过渡，在 Renderer 面板提供会话级诊断。诊断颜色显示选级和混合，明暗显示实际深度比较结果。
- 保持逻辑分配受现有每 View 预算约束，从已接受的图输出复用匹配深度。禁用、超限及失败状态均可追溯到所属 View 和处理阶段。

## Capabilities

### New Capabilities

- `shadow-rendering` — 图内方向光深度生产、级联可见性查询及跨帧接受合同。

### Modified Capabilities

- `shadow-planning` — 方向光由单段扩展为成组级联，明确每级稳定拟合、caster 和区域依赖。
- `view-preparation` — 为每 View 校验级数、透视切分比例和过渡宽度，请求仍由 View 持有。
- `static-forward-rendering` — 通过正式场景帧交付可定位的方向光级联诊断。
- `render-graph-execution` — 允许已定义的只读数组层组按受支持的单层视图交接，实际采样继续逐层授权。

## Impact

- Renderer/Lighting 准备级联和预算，Draw 与 Passes 消费 caster，Forward 连接查询和 ViewOutput。Shader 从生成接口消费投影、层及区间数据，随现有产品程序代准备。Renderer 面板提交会话诊断请求并显示实际接受状态。 [准备合同](../../../../Docs/Architecture.md#light-preparation) · [几何合同](../../../../Docs/Architecture.md#geometry) · [阴影合同](../../../../Docs/Architecture.md#shadows)
- D09-T03 承接聚光与点光的 GPU 深度和查询，D09-T04 承接 Hard/PCF 质量及共享光照查询，D09-T05 完成画质和资源压力回归。 [后续工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t03)
