# Proposal

## Why

D09-T01 已交付局部灯的保守阴影计划和成组区域，D09-T02 已在正式 Forward 帧中验证方向光的 D32 生产与查询。D09-T03 将聚光和点光接到同一帧链，使三类灯的阴影可查询，并让灯、投射物编辑和预算降级都能在正式视口核对。 [D09-T03 工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t03) · [D09-T01 验收](../2026-11-24-prepare-shadow-plans/tasks.md#final-verification) · [D09-T02 验收](../2026-11-24-add-stable-directional-csm/tasks.md#final-verification)

## What Changes

- 为聚光交付基于外锥的有限透视投影，为点光交付共用近远面且顺序、朝向明确的六面投影。未指定 `range` 的灯继续按无截断直接光求值，`shadowDistance` 单独限制阴影覆盖。
- 复用静态 geometry、实例、MASK 覆盖和常规阴影深度，通过 RenderGraph 生产并逐层查询 D32。以实际分配记录关联灯、面和纹理层，明确 point 面边界和越界的可见性。
- 在现有 Renderer 面板按 View 选择局部灯诊断。正式 Forward 图按需生产所选灯的完整组，显示实际深度比较及所查面。查询超出范围或区域不可用时返回全可见，无法显示该诊断时沿现有视口路径提供可用输出和原因。
- 静态匹配输入复用完整接受的深度组。灯和 caster 编辑、区域释放或重分配触发相应更新，预算拒绝保留完整直接光输入。方向光仍可通过级联诊断查询。

## Capabilities

### New Capabilities

### Modified Capabilities

- `shadow-planning` — 将有限局部灯体落实为可用于光栅和查询的投影，定义无法表示的局部阴影视图及其保守 caster 覆盖。
- `shadow-rendering` — 交付局部灯的图内深度、同投影可见性查询及匹配的成组接受与复用。
- `view-preparation` — 为各 View 解析带身份的会话诊断请求、当前阴影组和原因，并按实际模式核算附件。
- `static-forward-rendering` — 通过正式 Forward 与 Renderer 面板观察聚光和点光的实际可见性、请求及接受状态。

## Impact

- Renderer/Lighting 持有投影和区域计划，Draw、Forward 与 RenderGraph 负责生产和消费实际层，View 持有请求与被接受的层组，Runtime 确认提交，Editor 只提交请求并显示状态。Shader 继续通过生成接口和现有完整代交付。 [灯与阴影准备](../../../../Docs/Architecture.md#light-preparation) · [阴影生成与查询](../../../../Docs/Architecture.md#shadows) · [共享光照](../../../../Docs/Architecture.md#lighting)
- 沿用现有场景 v1、D32 二维数组及逐层二维采样能力。当前变更交付单点诊断查询。统一 `EvaluateLighting`、接收偏移和完整 Hard/PCF 质量配置由 D09-T04 接续，阴影压力和画质成本门槛由 D09-T05 接续。 [D09-T04](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t04) · [D09-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d09-t05)
