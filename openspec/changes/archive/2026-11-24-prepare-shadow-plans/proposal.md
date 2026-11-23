# Proposal

## Why

D08 已验收共享 SceneFrame、独立 View 准备和完整灯列表。D09-T01 要为阴影深度生产建立 CPU 计划，让每个 View 能从可见灯追溯到有限阴影视图、画外投射物和有预算的存储地址。 [D08 阶段出口](../2026-11-23-verify-execution-chain-and-idle-cost/tasks.md#final-verification) · [D09-T01](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t01)

## What Changes

- 从稳定有限相机、当前灯和同一份 SceneFrame 生成方向光、聚光和点光的阴影视图与保守投射物集合，保留能遮挡可见接收区域的画外几何。
- 定义每 View 的逻辑分层分配、质量档、层与字节上限，以及预算不足时确定的降级顺序。点光六面成组分配，灯光求值仍消费完整可见灯集合。
- 以稳定灯身份、区域代与所需内容版本交付计划。复用冻结场景的计划，对相关编辑、灯删除、尺寸变化和禁用更新相应状态。
- 通过 View 的会话请求和现有统计面板显示 CPU 计划、使用额度与原因，让计划分配、缓存复用和已接受的场景画面各自可核对。

## Capabilities

### New Capabilities

- `shadow-planning` — 保守阴影视图、稳定索引、预算分配和版本化缓存。

### Modified Capabilities

- `view-preparation` — 独立解析每 View 的阴影请求，交付与本次相机和 SceneFrame 匹配的计划。
- `render-frame-statistics` — 从现有只读通道查看 CPU 计划、额度、变化与拒绝原因。

## Impact

- Renderer/Lighting 负责规划与分配，Prepare 接入同一正常帧，View 持有会话请求与计划状态，Runtime 和 Editor/Panels 通过现有统计通道交付观测。当前 Forward 以完整灯表继续出图。 [灯准备](../../../../Docs/Architecture.md#light-preparation) · [阴影合同](../../../../Docs/Architecture.md#shadows) · [统计合同](../../../../Docs/Architecture.md#diagnostics)
- 本包交付 CPU 计划。D09-T02 接续 CSM 划分、稳定化与深度 Pass，D09-T03 接续聚光投影、点光面变换与采样地址，D09-T04 接续统一 Hard/PCF 查询，D09-T05 验收阴影图像与资源压力。 [D09 后续边界](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t02)
