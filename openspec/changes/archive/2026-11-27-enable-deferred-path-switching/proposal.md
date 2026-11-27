# Proposal

## Why

D11-T03 已验收正式 Deferred 子图，主 View 和离屏 View 的正常路径请求仍只接受 Forward 与 Forward+。D11-T04 将 Deferred 纳入现有帧前准备、正常提交和状态展示，使同一静态场景可以请求并切换三种不透明策略。 [D11-T04](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t04) · [D11-T03 验收](../2026-11-27-add-deferred-lighting-subgraph/tasks.md#final-verification)

## What Changes

- 在 View Settings 和显式离屏入口开放 Deferred 请求。主 View 沿用路径字段编辑、Undo/Redo 和 v1 保存恢复，文件使用可省略的 `mainView.requested.opaquePath`，新增值为 `deferred`。
- 将三策略注册到固定的表面准备和不透明光照入口。选用策略的完整计划决定路径、目标布局、材质路由、Shader 角色和实际资源消费者。三路使用同一场景工作集、兼容驻留和共享天空及显示阶段。
- 三路在单样本 Physical Lighting 下消费 None 或 FXAA。Direct Lighting 延续 LinearClamp/None，材质或阴影诊断继续按其自身合同交付。基线明确反馈不支持的配置，Deferred+MSAA 能力为 false，TAA 在 D15 验收后开放。当前关闭的屏幕效果沿用既有默认光照输入。 [计划兼容性](../../../../Docs/Architecture.md#plan-readiness) · [设备能力](../../../../Docs/Architecture.md#capabilities)
- 每个 View 独立准备候选并保持可执行的有效策略。匹配候选实际完成正常提交后，同步确认路径、生产身份、颜色及配对深度、有效配置与相关历史资格。失败和不支持原因与最新请求关联。
- 在同一会话验证三路径的真实出图、挂起与恢复、快速切换、多 View、相机和尺寸变化，以及兼容资源的复用与在途保活。D11-T05 承接整场等价、可视化和成本门槛。 [D11-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d11-t05)

## Capabilities

### New Capabilities

沿用现有能力。

### Modified Capabilities

- `view-preparation` 扩展每 View 的路径请求、能力解析、实际策略组装和事务确认，以实际生产身份决定历史资格。 [增量规格](specs/view-preparation/spec.md)
- `editor-domain-edits` 扩展主 View 路径字段的编辑与重放，展示本 View 的请求、有效策略和具体原因。 [增量规格](specs/editor-domain-edits/spec.md)
- `editor-viewport-output` 将有效显示配置与图像映射合同覆盖到三路径的正常场景预览。 [增量规格](specs/editor-viewport-output/spec.md)
- `scene-document-persistence` 扩展 v1 路径值域，恢复编辑已接受的主 View 请求并延后确认有效渲染策略。 [增量规格](specs/scene-document-persistence/spec.md)

## Impact

- `Renderer/View`、`Renderer/Prepare`、`Renderer/Paths` 和 `Runtime` 接续 D10-T04 的请求、候选和接受链。选中的路径使用 T01 至 T03 已验收的 GBuffer、辅助路由与 Deferred 照明。 [双路径验收](../2026-11-26-switch-opaque-paths-atomically/tasks.md#final-verification) · [路径组装](../../../../Docs/Architecture.md#paths)
- `Engine/Shader` 的产品角色、生成布局和原生准备预算供三策略共同消费，FrameSlot 保活与 RenderGraph 退休沿用现有协议。 [Shader 准备](../../../specs/shader-generation-reload/spec.md) · [提交确认](../../../../Docs/Architecture.md#history-commit)
- `Editor/Panels` 和 `Editor/Session` 扩展现有字段与文档值域。新版本继续读取已有 v1 文档。较旧的两路径版本会按其严格读取规则拒绝含 `deferred` 的文档。 [场景存储](../../../../Docs/Architecture.md#asset-persistence)
