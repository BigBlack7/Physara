# Spec Delta

## ADDED Requirements

### Requirement: Display local shadow visibility for the selected light

Forward SHALL 通过正式 ViewOutput 交付当前 View 所选聚光或点光的线性阴影诊断。诊断 SHALL 使用当前接受场景对应的有效深度组和查询数据，以明暗显示实际可见性，点光另以颜色标识所查面，覆盖外用中性显示。主 View SHALL 可从现有 Renderer 面板提交选择及结束诊断，离屏 View SHALL 接受所属 View 的独立请求。结束诊断后 SHALL 按该 View 原请求的普通模式显示，方向光级联诊断 SHALL 保持可选。

面板 SHALL 区分当前所选灯、计划质量与预算原因，及最近一次正常接受的诊断和所属票据。当前无有效分配或诊断不可用时 SHALL 可按可用的普通模式出图；录制或提交失败时 SHALL 沿既有视口退路显示。仅当本轮正常图实际采样并完整接受诊断时，其结果 SHALL 被标为当前有效。普通受光模式 SHALL 按原有完整灯输入求值。

#### Scenario: Show accepted spot and point visibility in the viewport

- **WHEN** 用户从 Renderer 面板先选择一个有实际遮挡的聚光，再选择一个有跨面接收区的点光，最后结束诊断
- **THEN** 各次正常接受的视口颜色反映对应灯的遮挡及点光面，面板显示匹配的灯和接受状态，退出后恢复之前请求的普通画面

#### Scenario: Explain a budget rejection without losing the light or the viewport

- **WHEN** 一个 View 请求点光诊断，但在主方向光和聚光取得区域后剩余预算不足六层，随后调整阴影设置使其全组准入
- **THEN** 拒绝时面板显示当前灯的预算原因，普通受光输出仍包含该点光。若新诊断候选失败，上次接受画面及其票据仍可辨认。当前完整组实际获接受后才显示有效的六面查询
