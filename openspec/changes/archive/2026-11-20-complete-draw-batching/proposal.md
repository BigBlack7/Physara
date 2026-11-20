# Proposal

## Why

D08-T02 已让同版几何共享页、有效材质共享驻留存储，Forward 仍为每个可绘 Primitive 单独设置图形状态并录制直接绘制。D08-T03 将可共享完整状态的表面组织成实例和间接批次，同时保留各实例的几何、材质和覆盖结果。 [D08-T03 工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t03) · [D08-T02 验收](../2026-11-20-complete-versioned-gpu-residency/tasks.md#final-verification)

## What Changes

- 在 Forward 准备期按完整的 Pipeline、纹理/采样器、几何页和数据绑定对不透明工作集排序。稳定处理相同排序值、不同资源代和较大材质索引。兼容的相邻表面共用批次，同一索引区间的相邻实例共用命令。
- 将对象和有效材质行交给只读表，由每 View 的实例索引选择。保留 OPAQUE、MASK、Unlit、镜像绕序和深度/颜色合同，相同采样绑定下的数值覆盖仍落到各实例自己的材质行。
- 通过现有场景上传包生产实例数据和固定数量的 indexed indirect 参数。图按精确范围建立依赖，OpenGL 4.6 即时列表按真实能力执行单命令及多命令间接 Draw，并用现有提交协议保活。
- 在已接受场景统计中区分有效绘制项、批次、间接记录、实际原生 Draw 与绑定，以及实例和参数上传。验收以相同有序记录展开的直接绘制为对照，核对像素、深度、索引语义和调用数。

## Capabilities

### New Capabilities

本工作包沿用已有 capability。

### Modified Capabilities

- `static-forward-rendering` 定义有序 Draw、完整批次兼容性及实例数据的实际表面消费。
- `static-gpu-residency` 定义可索引对象与材质行的驻留、绑定范围和接受后的复用。
- `render-graph-contract` 定义 indirect 参数的独立读用途、字节范围和生产依赖。
- `opengl-device-resources` 支持带 indirect 参数用途的普通 Buffer，明确已交付与仍不可用的执行范围。
- `opengl-immediate-draw` 定义固定数量的 indexed indirect/MDI 执行、范围校验和保活。
- `render-frame-statistics` 定义批次、已发命令和批次数据上传的统计口径。

## Impact

- 实施涉及 Renderer/Draw、Materials、Forward、RenderGraph、现有 Shader 接口生成、OpenGL/Validation 和 Runtime 到 Editor 的只读统计。沿用公开 `GraphicsState`、`DrawIndexedIndirectArguments` 和 `DrawIndexedIndirect`。
- 本轮范围为已支持的静态 Forward 几何与 CPU 生产的固定数量 indexed 参数。透明 Draw 的一般路由归后续路径工作，Compute 生产 indirect 参数及其同步归 D08-T04。参数计数缓冲、非索引间接绘制及 bindless 属现有未交付范围。 [D08 阶段计划](../../../../Docs/Developments/D08-D11-RenderPaths.md#dev-d08) · [Draw 合同](../../../../Docs/Architecture.md#draws) · [OpenGL 能力合同](../../../../Docs/Architecture.md#capabilities)
