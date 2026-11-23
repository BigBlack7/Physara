# Proposal

## Why

D08 的各项能力已经分包验收。[D08-T08](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t08) 需要在当前正式输入上证明它们组合后仍按正确的版本、提交和完成边界运行，并量出冻结帧的成本，作为 D09 阴影链的入口依据。

## What Changes

- 以固定静态场景串联暖机、冻结、稀疏编辑、双 View、连续加载、尺寸往返和 Shader 重载。核对实际上传、创建、绑定、批次、池重用和在途退休，分别追到对应的 View、图、资源代和提交。
- 通过真实 OpenGL 验证 Compute、间接绘制、传输和读回的消费次序。比较统计面板的显隐结果，验证 GPU 计时从 Pending 到 Ready 的原提交身份及非阻塞读取。
- 对增量准备和 Draw 批处理分别做同画质对照，记录 CPU、GPU 和等待分布以及实际操作量。对未证实收益的复杂优化，将复现数据和重评范围送回对应工作包。
- 前置为已归档的 [D08-T01](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t01) 至 [D08-T07](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t07)，其中 [D08-T03](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t03) 至 T07 是本包的直接依赖。[验收矩阵](design.md#verification-matrix) 将前置规格和本轮证据逐组对应。

## Capabilities

### New Capabilities

无。本次交付 D08 阶段出口的验收规格及证据计划。

### Modified Capabilities

无。采用原生 `skip_specs: true`，以现有主规格的产品合同判定验收结果。若重现的缺陷确需改变已采纳行为，先修订变更范围及相关规格，再实施。

## Impact

- 验收覆盖当前 Windows x64/MSVC、单队列 OpenGL 4.6 Core 及现有静态 Forward 路径。观测沿 Runtime、SceneFrame、View、Renderer、RenderGraph、RHI 和 Editor 的实际调用边界进行。证据标明每项检查实际覆盖的构建配置、设备和驱动。
- 回放、输入、对照构建和原始输出归 `build/d08-t08/`。证据身份、结果及限制留在本 change 的 tasks，遵循[统计](../../../../Docs/Architecture.md#diagnostics)、[合同验证](../../../../Docs/Architecture.md#tests)和[性能测量](../../../../Docs/Architecture.md#benchmark)。
- D08 阶段出口由独立复核和最终 verify 确认。阶段结论引用所测的支持范围、待重评优化及可复现证据。[D09 入口](../../../../Docs/Developments/D08-D11-RenderPaths.md#dev-d09)
