# Proposal

## Why

D10-T01 已为每个 View 交付稳定 Froxel 网格和 32 位 CPU 候选。D10-T02 让同一光照在索引预算不足时仍能完整求值，并为候选数据建立可验证的 GPU 生产和消费顺序。 [开发计划](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t02) · [D10-T01 验收](../2026-11-25-prepare-cpu-froxel-light-lists/tasks.md#final-verification)

## What Changes

- 按稳定的 cell 顺序分配索引预算。每格交付完整的候选区间或明确的 overflow header。有溢出或查询落在网格覆盖外时，调用共享的完整可见灯求值。
- 通过本帧拥有的上传包交付 View 参数、headers、indices 和全局局部灯行号。消费者从图中读取同一 View 的对应写版本，并继续使用配对的完整灯表和阴影引用。
- 在现有只读诊断中报告按 View 的溢出、索引条目、退路原因和单次退路查询的灯遍历量。原生 OpenGL 验收通过小预算触发真实 overflow，并读回与完整灯列表对照的预曝光 HDR。

## Capabilities

### New Capabilities

### Modified Capabilities

- `froxel-light-preparation` 将索引池不足定义为可消费的逐格溢出，并补齐拥有型图上传和共享着色消费合同。 [增量规格](specs/froxel-light-preparation/spec.md)
- `render-frame-statistics` 增加 Froxel 预算、溢出和完整灯遍历量的按 View 诊断。 [增量规格](specs/render-frame-statistics/spec.md)

## Impact

- `Renderer/Lighting` 负责列表、数据交付和退路选择。帧槽的 `UploadBatch`、现有 RenderGraph 上传辅助及 RHI 访问计划承担字节所有权、写读依赖和退休。 `Shaders/Lighting` 共享绝对灯行、阴影索引和单灯求值。 [灯列表](../../../../Docs/Architecture.md#light-preparation) · [GPU 驻留与上传包](../../../../Docs/Architecture.md#gpu-residency) · [上传与状态](../../../../Docs/Architecture.md#uploads) · [光照聚合](../../../../Docs/Architecture.md#lighting)
- 本工作包由显式请求的 View 和原生验证消费者验收。D10-T03 接入 Forward+ 子图及 Pipeline 准备，D10-T04 完成有效路径和历史的原子切换，D10-T05 完成两条交付路径的 HDR 与多灯成本对比。 [D10 开发计划](../../../../Docs/Developments/D08-D11-RenderPaths.md#dev-d10)
