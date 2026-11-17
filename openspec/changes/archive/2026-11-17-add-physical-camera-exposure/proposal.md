# Proposal

## Why

D07-T01 已交付共享表面与 BRDF。D07-T02 为每个渲染 View 建立可编辑、可复现的物理相机和预曝光输入，使后续直接光、IBL、自发光和显示处理能接到同一曝光域。 [本工作包](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d07-t02) · [前置验收](../2026-11-17-add-core-surface-brdf/tasks.md#final-verification) · [相机合同](../../../../Docs/Architecture.md#camera)

## What Changes

- View 持有光圈、快门、ISO、曝光补偿、传感器高度、导航焦距及对焦距离。导航透视由镜头关系得到垂直 FOV，场景透视相机保留现有 `yfov` 和投影裁剪合同。
- 为当前相机快照计算标定的 `EV100` 和 `preExposure`。预曝光随生成的 View Shader 接口进入当前帧，最后成功渲染值随匹配的场景输出确认。
- 在 View Settings 接入有单位的物理相机编辑与当前请求、最后接受值的回读。复用现有 View 事务、撤销重做和场景文件流程，允许读取缺少物理相机字段的 v1 文档。
- 生产直接光在 D07-T03 接入预曝光，环境与天空在 D07-T04 接续，D07-T05 消费相应 HDR 域。对焦距离是后续镜头处理的输入。 [D07 阶段计划](../../../../Docs/Developments/D06-D07-SceneAndForward.md#dev-d07)

## Capabilities

### New Capabilities

### Modified Capabilities

- `view-preparation` 增加物理镜头投影选择、曝光计算和每 View 的准备合同。
- `editor-viewport-output` 为实际接受的场景相机快照记录对应的预曝光，并分别公开当前请求与最后成功值。
- `scene-document-persistence` 扩展 v1 主 View 的可选物理相机请求及恢复规则。
- `editor-domain-edits` 增加物理相机字段在 View Settings 中的提交、失败、撤销和重做合同。

## Impact

- 主要落点为 Renderer/View、Renderer/Prepare、静态 Forward 的生成 View 接口、Editor 工作区和场景文件边界。Scene Camera 继续提供投影和世界位姿。 [所有权与持久化](../../../../Docs/Architecture.md#asset-persistence) · [帧准备](../../../../Docs/Architecture.md#view-preparation) · [成功渲染与曝光](../../../../Docs/Architecture.md#temporal-contract)
- 开发验证材料归入 `build/d07-t02/`。验证覆盖 EV 与镜头数值、v1 恢复和编辑事务、实际 Shader 常量读回及场景帧确认边界。 [验证合同](../../../../Docs/Architecture.md#tests)
