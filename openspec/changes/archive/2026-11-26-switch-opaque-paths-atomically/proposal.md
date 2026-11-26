# Proposal

## Why

D10-T03 已让同一 Renderer 执行 Forward 和 Forward+。新路径准备未完成时，当前入口只能保留最近接受的颜色。D10-T04 让编辑器和离屏调用方独立请求路径，在等待期间继续渲染有效计划，并在首个匹配的正常帧接受时确认切换。 [D10-T04](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t04) · [D10-T03 验收](../2026-11-26-add-forward-plus-opaque-path/tasks.md#final-verification)

## What Changes

- 从现有 View Settings 提交主 View 的 Forward / Forward+ 请求，显示最新请求、最近确认的有效路径及当前等待或拒绝原因。主 View 的路径进入既有字段编辑、Undo/Redo 和 v1 文档请求合同。显式离屏 View 继续使用类型化请求。
- 路径待选计划按 View 独立准备。当前有效策略可满足本轮输入时继续正常绘制。就绪候选只能使用一份完整兼容计划构图，其路径与实际接受的输出、相机、画质、Shader 代和提交票据一起发布。
- 保留共享 Scene、材质／几何驻留及兼容的管线与图资源。只替换新计划所需的不兼容资源。以实际接受的路径、生产接口及栅格判断旧历史资格，沿现有提交和完成协议管理在途存储。
- 交付当前单样本画质下的切换和历史兼容性。D10-T05 负责同输入 HDR 等价与多灯成本验收。D11 交付 Deferred，D14/D15 交付 SSR、完整 HDR 历史与 TAA。 [D10-T05](../../../../Docs/Developments/D08-D11-RenderPaths.md#d10-t05) · [D11](../../../../Docs/Developments/D08-D11-RenderPaths.md#dev-d11) · [HDR 历史](../../../../Docs/Developments/D14-D15-ImagePipeline.md#d15-t01)

## Capabilities

### New Capabilities

沿用现有能力。

### Modified Capabilities

- `view-preparation` 规定每 View 的候选准备、有效策略持续绘制、原子确认、历史兼容性和在途保活。 [增量规格](specs/view-preparation/spec.md)
- `editor-domain-edits` 规定路径字段编辑、历史重放和准备／确认状态的展示。 [增量规格](specs/editor-domain-edits/spec.md)
- `scene-document-persistence` 增加主 View 的可省略 v1 路径请求，沿用完整候选恢复。 [增量规格](specs/scene-document-persistence/spec.md)

## Impact

- `Renderer/View` 拥有路径意图、待选状态和已接受记录。`Renderer/Prepare`、`Renderer/Paths` 与 `Runtime` 在已有帧槽内按当前输入准备、选择并提交计划。 [计划准备](../../../../Docs/Architecture.md#plan-readiness) · [提交确认](../../../../Docs/Architecture.md#history-commit)
- `Editor/Panels` 读取 View 快照并提交单字段意图，`EditorSession` 复用现有主 View 编辑与文件操作。 [领域编辑](../../../../Docs/Architecture.md#edits) · [场景持久化](../../../../Docs/Architecture.md#asset-persistence)
- 构图、Shader 准备、帧统计与资源退休继续沿现有合同。 [路径组装](../../../../Docs/Architecture.md#paths) · [图资源](../../../../Docs/Architecture.md#graph-resources) · [帧统计](../../../specs/render-frame-statistics/spec.md)
