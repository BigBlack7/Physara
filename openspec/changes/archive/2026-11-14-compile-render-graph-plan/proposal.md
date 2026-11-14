# Proposal

## Why

D05-T01 已能声明上传、清屏、UI 合成和输出所用的精确版本与范围。D05-T02 需要把这些记录编译成可检查的执行计划，使后续执行器能确定哪些 Pass 必须运行、何时可覆盖旧内容，以及首次使用时需要准备哪些资源。 [工作包](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t02) · [架构 §6.3](../../../../Docs/Architecture.md#graph-compile) · [图声明合同](../../../specs/render-graph-contract/spec.md)

## What Changes

- 增加纯 CPU 编译入口。根据版本与精确范围追溯实际内容生产者，建立 RAW、WAR 和 WAW 约束，报告未定义内容、非法 feedback、不可满足的覆盖顺序或环。
- 从明确的导出和读回副作用追溯活性。对存活 Pass 做稳定拓扑排序，生成首末使用和图内物理分配计划。计划受描述、给定能力与显式瞬态预算约束。
- 输出带图代的计划，保留实际使用的导入、外部输出、附件与清除信息，以及每个 Pass 的版本、用途和范围。编译失败返回诊断，成功时交付完整计划。

## Capabilities

### New Capabilities

- `render-graph-compilation`：已封存图的内容依赖、活性、确定顺序、物理计划与编译诊断。

### Modified Capabilities

## Impact

- 在 `Engine/Renderer/RenderGraph` 的现有 CPU 目标中扩展编译合同。声明图继续持有导入引用和 Pass 数据。独立 CPU 消费者核对结果、拒绝路径与 D05-T01 兼容性。 [声明入口](../../../../Engine/Renderer/RenderGraph/RenderGraph.hpp) · [构建说明](../../../../Docs/Build.md#构建)
- D05-T03 消费计划并接入首次物化与唯一同步链。D05-T04 将诊断视口、UI 和窗口接入图。D05-T05 接手编译或执行失败后的回退及取消。 [后续工作包](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t03)
