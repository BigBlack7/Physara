# Render Frame Statistics Delta

## ADDED Requirements

### Requirement: Report froxel pressure and fallback traversal cost per view

Renderer SHALL 为显式请求 Froxel 的 View 提供有界只读摘要，关联当前 Scene、View 和准备输入。摘要 SHALL 报告网格可用状态及原因、有效索引额度、实际压入的索引条目、总 cell 数、overflow cell 数和溢出或无有效 cell 时每次退路查询遍历的完整可见局部灯行数。可见灯裁剪完成后，后续准备拒绝的结果 SHALL 保有局部灯遍历量、拒绝原因和当前输入身份。

统计面板 SHALL 通过现有只读通道显示这些事实，单位为 cell 或灯行条目。单次退路遍历量 SHALL 标为计算量，原生上传字节及 GPU 时间 SHALL 各按实际执行和提交口径报告。失败、取消或被裁剪的 View SHALL 保持该 View 最近正常接受记录的原身份。统计显示 SHALL 只产生界面构建工作。

#### Scenario: Observe a budget-limited froxel view alongside an independent view

- **WHEN** 两路 View 使用同一场景，其中一路以小预算产生 overflow，且随后只有另一 View 获得新的正常接受
- **THEN** 两路可按各自输入身份核对有效额度、压入条目、溢出格数和退路遍历量。未接受的一路仍保有最近有效的提交记录，诊断显隐不会改变准备、录制或等待行为

#### Scenario: Retain traversal cost when a culled view is rejected

- **WHEN** 同一 Froxel 请求分别在布局时早退、完成可见灯裁剪后被附件限制拒绝、以及完整准备
- **THEN** 布局早退的遍历量保持未知，已裁剪的两轮各交付当前完整可见局部灯行数，附件拒绝保有当前原因
