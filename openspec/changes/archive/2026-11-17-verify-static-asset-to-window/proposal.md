# Proposal

## Why

D06-T01 至 T06 已分别验收资产、场景、Forward 视口和编辑工作流。[D06-T07](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t07) 需要在同一组静态输入上证明这些合同经过正式窗口呈现仍然成立，并验证导入和图形资源在取消、失败、换尺寸及退出时的寿命，作为 D06 的阶段出口。

## What Changes

- 建立一个可复用的非对称小场景，贯通标准 glTF 解码、独立双实例、准备、图内上传、Forward、UI 合成和 OpenGL 窗口交换。用已知解码值、视口和窗口像素验证多 Primitive/材质、UV0/UV1、缺源切线与稀疏 accessor、镜像实例、遮挡及两类投影。
- 在同一正式编辑工作流中核对变换、删除、撤销、保存和重开。为同源重复导入、取消和失败选择代表性序列，跟踪请求、CPU 固定版本和场景提交的独立身份。
- 对仍有图形工作待退休的删除/resize，以及导入尚未结束时的关闭，取得真实完成及退出证据。把可复现输入、可判定的预期和当次支持 profile 记入 change。
- 实施依赖为 [D05 阶段出口](../2026-11-15-handle-frame-graph-failures/tasks.md#final-verification) 与已归档的 [D06-T02](../2026-11-16-import-static-gltf-models/tasks.md#final-verification)、[D06-T05](../2026-11-16-render-static-scenes-forward/tasks.md#final-verification)、[D06-T06](../2026-11-17-edit-static-scenes/tasks.md#final-verification)。物理光照、曝光和 IBL 由 [D07](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d07) 在 D06 出口通过后接续。

## Capabilities

### New Capabilities

无。本工作包交付 D06 集成验收，测试材料归工作包的临时构建目录。

### Modified Capabilities

无。采用原生 `skip_specs: true`。以现有主规格规定的产品结果为判定标准，逐组映射到 [设计与验证矩阵](design.md#verification-matrix)。

## Impact

- 验证沿正式 AssetManager、Scene、Renderer、EditorSession、Runtime 与 DeviceHost 的消费者边界运行。平台范围是当前 Windows x64、MSVC 和真实 OpenGL 4.6 Core，覆盖单样本 Forward 的 Unlit 和显式材质预览。
- 本地场景、探针、脚本及原始证据位于 `build/d06-t07/`。持久工作包记录包含输入和产物身份、实际复现命令、场景结果及范围限制，遵循 [验证与关闭合同](../../../../Docs/Architecture.md#tests) 和 [完整关闭](../../../../Docs/Architecture.md#shutdown)。
- 如发现已采纳合同的实现偏差，按最小影响面修正并回归，先在 design 记录可复现失败和处理范围。需要改变产品可观察行为时，按 OpenSpec 修订范围和相应 delta 后实施。
