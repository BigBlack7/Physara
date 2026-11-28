# Proposal

## Why

[D12-T03](../../../../Docs/Developments/D12-D13-Materials.md#d12-t03) 要交付标准 glTF 布料绒光的完整链路。当前导入和受光实现已支持 IOR/specular 与 clearcoat，环境 DFG 已生成 cloth 通道，材质采样、层合成、环境预滤波和实际颜色交付仍缺少 sheen 消费者。 [材质合同](../../../../Docs/Architecture.md#materials) · [光照合同](../../../../Docs/Architecture.md#lighting)

## What Changes

- 导入 lit OPAQUE/MASK 的 `KHR_materials_sheen`，保留标准颜色、粗糙度和两个纹理槽。接入 Inspector、实例稀疏覆盖和既有场景 v1 的保存与打开。
- 以共享 Charlie/Neubelt 波瓣和配套 DFG 控制基底能量。清漆位于 sheen 之上，基底沿用核心或 IOR/specular 响应，并保留已有发射合同。 [固定 Khronos 规范](https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_sheen/README.md)
- 用现有环境缓存按受光需求生成 Charlie 预滤波产物，保持环境源、朝向、强度和曝光语义。Direct 和无环境的 Physical 也交付使用匹配 DFG 的 sheen 逐灯响应。
- 通过现有 Forward、Forward+ 及 Deferred ForwardExtension 交付颜色。复用覆盖与 SurfaceAux 表面合同，核对实际 Shader 准入、环境更换和历史失效。

## Capabilities

### New Capabilities

### Modified Capabilities

- `static-gltf-import` — 导入并报告 sheen 的标准输入、排斥关系和纹理语义。
- `editor-domain-edits` — 编辑实例 sheen 字段和纹理，归因请求、准备与已接受受光。
- `scene-document-persistence` — 在 v1 中保存并校验 sheen 稀疏覆盖。
- `static-surface-evaluation` — 共享 Charlie/Neubelt、方向能量预算及其在清漆下方的合成。
- `environment-lighting` — 按实际需求交付匹配的 sheen DFG 和预滤波环境，确认完整候选。
- `static-forward-rendering` — 交付单独的 sheen 及其与已支持基底、清漆的组合。
- `shader-generation-reload` — 准入有限的 sheen 颜色角色及其实际活动资源。

## Impact

- 影响 Asset、Scene、EditorSession、Inspector、Renderer 材质/环境准备与图绑定、共享 Shader 和生成 ABI。复用 T01 和 T02 的静态几何、alpha、阴影和路由合同。 [T01 验收](../2026-11-28-add-ior-specular-material-profile/tasks.md#final-verification) · [T02 验收](../2026-11-28-add-clearcoat-material-profile/tasks.md#final-verification)
- 交付范围限于已支持的不透明静态几何和定向布料反射。D12-T04/T05 负责后续波瓣，D12-T06 负责跨波瓣组合与阶段能力回归，D13 负责透明和透射。 [开发计划](../../../../Docs/Developments/D12-D13-Materials.md#dev-d12)
