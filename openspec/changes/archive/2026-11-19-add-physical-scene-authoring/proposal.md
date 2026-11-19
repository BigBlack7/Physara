# Proposal

## Why

D07-T03 至 T05 已将直接光、IBL 与固定 SDR 显示链交付到主 View。D07-T06 让用户在编辑器内调整这些物理输入，并通过同一个可打开的参考场景验证画面、数值与提交成本，为后续阶段提供共同基线。 [D07-T06](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d07-t06) · [物理创作参数](../../../../Docs/Architecture.md#physical-inputs)

## What Changes

- 在 Inspector 编辑选中实例的核心材质因子和纹理槽覆盖、方向灯／点光／聚光参数，以及当前 Scene 的环境资源、朝向、强度和用途。在 Inspector 中复用主 View 的物理曝光编辑，保留现有 View Settings 入口。运行期修改走帧边界事务与同一撤销历史，创作值由 v1 场景 JSON 保存，共享 glTF/GLB 由 Asset 只读提供。
- 展示所选模型版本的导入支持说明、项目纹理和环境的加载进度或失败，以及主 View 最后正常接受的场景状态。等待替换时，用户可以识别各阶段的请求与有效内容。
- 交付一个使用现有 Open Scene 入口的 Forward 参考场景，包含材质球、米制几何、已知灯光及自制非对称 HDR 环境。固定依赖内容、投影与曝光，制定显示前 HDR 和显示／窗口颜色的量化检查，覆盖 D07 阶段出口。
- 在正式 Runtime 和统计面板提供实际 CPU 阶段、Renderer/RHI 提交及驻留计数。GPU 时长沿现有 OpenGL TimerQuery 异步取得，只有匹配计时结果就绪才提供数值；同口径性能比较按实际测量另行留存。 [统计合同](../../../../Docs/Architecture.md#diagnostics) · [测量合同](../../../../Docs/Architecture.md#benchmark)

## Capabilities

### New Capabilities

- `forward-reference-scene` — 随项目交付的可编辑 Forward 参考场景、输入身份及可复现的 HDR／窗口验证口径。
- `render-frame-statistics` — 正常场景帧的 CPU/Renderer/RHI 事实、资源数量和有界的异步 GPU 时长。

### Modified Capabilities

- `editor-domain-edits` — 材质、灯和环境的字段级 Inspector 编辑，主 View 曝光入口及可见的导入和资源状态。
- `static-scene` — 当前 Scene 的环境请求编辑和重放，以及对应的文档与渲染内容版本变化。

## Impact

- Editor/Panels、Session、Commands 消费 Asset/Scene/View 的只读快照与事务。新增环境编辑沿 Scene owner 发布，纹理和环境文件继续沿 Asset 的既有路径校验、解码和版本固定入口解析。 [编辑事务](../../../../Docs/Architecture.md#edits) · [v1 场景合同](../../../../Docs/Architecture.md#asset-persistence)
- `Assets/Scenes` 交付 v1 参考文档及所需的自制资产与内容清单。Runtime 通过既有项目资源根打开。原生检查通过当前 Forward HDR、显示后 ViewOutput 和实际窗口取证，开发材料归入 `build/d07-t06/`。
- Runtime 汇合真实事实，Renderer 与 OpenGL RHI 各自记录所属计数，Editor 消费有界只读快照。沿既有提交和关闭合同使用 TimerQuery。 [既有 OpenGL 查询](../../../specs/opengl-completion-queries/spec.md) · [D08-T05 后续传输与查询](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t05)
