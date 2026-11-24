# Spec Delta

## ADDED Requirements

### Requirement: Resolve the selected local shadow group for the current view

View SHALL 持有独立的会话诊断选择，初始使用普通画面。选择 SHALL 能在普通画面、方向光级联和一个聚光或点光之间切换。类型化请求 SHALL 以当前 View、Scene 与带 generation 的灯身份验收，拒绝外来、失效或类型错误的选择并保留原请求。主 View 和离屏 View SHALL 沿相同的身份与当前输入合同处理。场景文档继续沿 v1 合同读写。

正式准备 SHALL 从当前冻结 Scene 和匹配的 CPU 计划解析所选灯及完整的阴影区域。当该灯被删除、转为其它类型、失去覆盖、预算不足或阴影禁用时，当前结果 SHALL 带可定位原因及空局部深度工作集。普通显示仍可用时 SHALL 继续交付有效的相机及完整灯输入。附件预检及阴影分配结算 SHALL 对应同一次实际显示配置与该 View 上次已交付的分配。

#### Scenario: Change one view selection and reject a stale light identity

- **WHEN** 同一 Scene 的主 View 和离屏 View 分别选择聚光和点光，随后其中一个灯被删除、其槽位由另一个对象复用，并收到旧灯选择请求
- **THEN** 原灯的诊断变为当前不可用，旧身份无法选中新对象。另一 View 的选择、计划及可接受输出继续独立解析

#### Scenario: Settle an offscreen layout with an unavailable local shadow group

- **WHEN** 一个离屏 View 请求所选局部灯的线性诊断，尺寸可容纳该诊断但超出其普通 FXAA 配置的附件预算，随后该灯失去阴影准入
- **THEN** 有完整组的候选按实际诊断配置准备。失去准入后，超限普通候选报告附件及阴影原因，原有规划区域按本 View 的已交付状态结算。重复拒绝不会重复释放，尺寸或请求恢复后按当前输入重新准备
