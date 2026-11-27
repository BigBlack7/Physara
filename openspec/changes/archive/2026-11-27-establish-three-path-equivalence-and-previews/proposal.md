# Proposal

## Why

D11-T01 至 D11-T04 已交付核心 GBuffer、辅助表面、Deferred 受光和三路径事务切换。D11-T05 要在相同冻结输入上核对三条路径的覆盖与工作域 HDR，让编辑者能检查真实表面，并形成可供后续阶段复用的三路径成本基准。 [工作包与阶段出口](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t05) · [三路径切换验收](../2026-11-27-enable-deferred-path-switching/tasks.md#final-verification)

## What Changes

- 以单样本 Physical Lighting 建立同口径原生回归。比较正常接受的预曝光线性 RGBA16F 和配对 Reverse-Z D32，覆盖核心/Unlit、MASK、静态实例、阴影与环境、无范围局部灯和 Froxel 溢出。用格式精度、重建误差和独立受光标样判定 Deferred 的差异。 [行为验证](../../../../Docs/Architecture.md#tests) · [Forward 阴影参考](../../../../Docs/Performance/ForwardShadowReference.md) · [Forward+ 画质参考](../../../../Docs/Performance/ForwardPlusReference.md#replay)
- 在主 View 提供按需表面预览。读取实际生产的 D32 或最终 GBuffer，以类型化 Pass 生成普通 UI 颜色图，展示核心字段、法线、粗糙度及可见表面的路由。预览在自身不可用时提供原因，普通颜色仍按原有提交规则确认。 [视口与预览](../../../../Docs/Architecture.md#editor-output) · [表面合同](../../../../Docs/Architecture.md#geometry)
- 在正常接受帧的既有统计中报告附件逻辑字节、图声明的读写量指标和 `ForwardExtension` 占比。以固定画质的多灯组记录 CPU/GPU 分布、等待、实际操作与 Froxel 退路，留存本阶段可重建的首份三路径比较。 [测量合同](../../../../Docs/Architecture.md#benchmark)

## Capabilities

### New Capabilities

### Modified Capabilities

- `editor-viewport-output` 增加表面诊断的类型、来源、有效矩形、实际可见需求和占位合同。 [现有规格](../../../specs/editor-viewport-output/spec.md)
- `view-preparation` 增加当前支持的三条不透明策略在同一冻结有效输入下的覆盖、深度和 HDR 等价要求。 [现有规格](../../../specs/view-preparation/spec.md)
- `render-frame-statistics` 增加接受 View 的附件与路由工作量，并规定三路径比较所用统计的来源、单位与配对条件。 [现有规格](../../../specs/render-frame-statistics/spec.md)

## Impact

- Renderer 从当帧已选子图提供类型化表面输入及工作量。Shader 构建、包准入和原生准备纳入有限的诊断角色，沿用既有表面布局与产品代际合同。 [路径组装](../../../../Docs/Architecture.md#paths) · [当前表面输出](../../../../Engine/Renderer/Paths/Forward.hpp)
- Editor 通过显式图采样显示额外的诊断颜色。Runtime 和现有 Frame Statistics 传递匹配的只读事实。路径、场景文件和单样本 None/FXAA 的配置边界沿用 D11-T04，标准扩展和时域能力依后续工作包推进。 [配置与切换](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t04) · [诊断统计](../../../../Docs/Architecture.md#diagnostics)
- 原生回放复用当前产品模块与 Windows x64/OpenGL 宿主。临时夹具和原始测量归入 `build/d11-t05/`。待独立复核、最终验证和归档后，由实测结果更新 D11 阶段出口及长期比较依据。 [开发约定](../../../../AGENTS.md#workflow) · [文档归属](../../../../Docs/Architecture.md#project-docs)
