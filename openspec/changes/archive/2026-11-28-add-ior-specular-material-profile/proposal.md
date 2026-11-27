# Proposal

## Why

[D12-T01 · 实现 IOR 与 specular profile](../../../../Docs/Developments/D12-D13-Materials.md#d12-t01) 为标准不透明扩展建立首条从 glTF 到受光输出的链。D11 已验收三路径与辅助表面合成，实际受光的 `ForwardExtension` 材质仍待接入。 [D11 三路径基准](../../../../Docs/Performance/ThreePathReference.md#quality)

## What Changes

- 支持 `KHR_materials_ior` 和 `KHR_materials_specular` 的独立声明及合法组合。保存规范缺省、显式零 IOR 兼容模式、两种纹理槽与各自的取样合同。导入报告标明 CPU 支持范围。
- 扩充权威材质 ABI 与有限的受光 feature profile。根据采样后的介质 Fresnel 端点和金属度分配直接光、环境镜面及漫反射能量。三条路径共用同一模型和现有的预曝光、阴影与环境输入。
- 接通受支持的 OPAQUE、MASK 扩展材质。Forward 使用全灯表，Forward+ 与 Deferred 的 `ForwardExtension` 使用 Froxel。Deferred 扩展沿 `SurfaceAux` 贡献最近表面，并按最终 D32 合成颜色。
- 在 Inspector 编辑和清除实例材质与纹理覆盖，随既有场景 v1 文件保存和恢复。以真实 OpenGL 读回核对导入、着色、覆盖和 Shader 准入。

## Capabilities

### New Capabilities

### Modified Capabilities

- `static-gltf-import` 接收标准 IOR 和 specular 描述，校验组合、字段、纹理和支持声明。 [增量规格](specs/static-gltf-import/spec.md)
- `static-surface-evaluation` 定义扩展取样、介质反射和能量分配，以及金属响应的混合。 [增量规格](specs/static-surface-evaluation/spec.md)
- `environment-lighting` 定义扩展在现有 DFG 和环境产品上的受光响应。 [增量规格](specs/environment-lighting/spec.md)
- `static-forward-rendering` 增加三路径实际受光、遮罩与辅助表面交付行为。 [增量规格](specs/static-forward-rendering/spec.md)
- `shader-generation-reload` 补充扩展颜色角色的包准入、帧前就绪和失败恢复。 [增量规格](specs/shader-generation-reload/spec.md)
- `editor-domain-edits` 增加 IOR 与 specular 实例编辑、继承和渲染状态反馈。 [增量规格](specs/editor-domain-edits/spec.md)
- `scene-document-persistence` 增加可省略的 v1 实例字段与纹理槽。 [增量规格](specs/scene-document-persistence/spec.md)

## Impact

- Asset 负责源材质与图像语义，Scene 和 Editor 负责实例覆盖与文档事务。Renderer 负责驻留、路由、批次与图消费，Shader 构建负责生成接口、有限产品包和活动资源准入。
- 已交付的四附件核心 GBuffer、D32 和辅助颜色合成提供本包的表面合同。D12-T02 至 T06 在各自工作包交付其它不透明波瓣及阶段组合验收，D13 负责透明与透射。 [图与覆盖合同](../../../../Docs/Architecture.md#geometry) · [后续工作包](../../../../Docs/Developments/D12-D13-Materials.md#d12-t02)

