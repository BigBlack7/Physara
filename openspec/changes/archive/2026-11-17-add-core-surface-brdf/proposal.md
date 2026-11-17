# Proposal

## Why

D06 已把 glTF 材质与几何接入真实 Forward 视口，现有颜色 Pass 用基色、法线、材质通道和自发光验证输入。D07-T01 需要在这个入口上建立一份可独立验证的核心表面和 BRDF，供后续直接光、IBL 和其他渲染路径共用。 [工作包](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d07-t01) · [D06 阶段出口](../2026-11-17-verify-static-asset-to-window/tasks.md#final-verification) · [共享材质模型](../../../../Docs/Architecture.md#materials)

## What Changes

- 从有效 glTF 材质求得共享的 MaterialInputs、SurfaceGeometry 和 PixelParams。现有预览复用标准纹理与几何求值，深度和颜色共用位置与 OPAQUE/MASK 覆盖。连续材质因子走数据。
- 交付核心 metallic-roughness 的 GGX、height-correlated Smith、Schlick、Lambert 和配套 DFG 能量项。以固定精度处理几何法线导数、粗糙度下限、掠射和退化半角。
- 用双精度 CPU 参考、白炉和方向反照率及原生 GL 读回验证共享模块。以现有 Forward 场景验证纹理、镜像双面法线和深度覆盖的消费链。
- D07-T02 接入物理相机和预曝光，D07-T03 将共享 BRDF 接入直接光，D07-T04 提供正式 DFG 与环境照明，D07-T05 和 T06 完成显示、编辑与阶段验收。 [D07 阶段计划](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d07)

## Capabilities

### New Capabilities

### Modified Capabilities

- `static-surface-evaluation` 增加从有效表面派生核心参数、过滤粗糙度、计算共享 BRDF 和应用 DFG 能量项的合同。 [当前主规范](../../../specs/static-surface-evaluation/spec.md)

## Impact

- 主要修改 Shaders/Material、Shaders/Geometry 和 Shaders/BRDF。Renderer/Materials 提供现有生成接口和有效材质行，Forward 的薄入口组合共享库。影响面以 `StaticForward` 和 `StaticDepth` 包及其正式消费者为准。 [GLSL 分层](../../../../Docs/Architecture.md#shader-runtime) · [包构建](../../../../Docs/Build.md#shader-构建)
- 开发探针与数值标样放在 `build/d07-t01/`。验收分别记录共享求值的数值证据、正式预览回归以及独立 A/B 审查结果。 [验证要求](../../../../Docs/Architecture.md#tests) · [审查闭环](../../../../AGENTS.md#review-loop)
