# rhi-common-validation Spec Delta

## ADDED Requirements

### Requirement: Validate compute candidates and dispatch before delegation

OpenGL Validation SHALL 使用有效 Graphics 即时列表的公共输入合同，在委托前校验 Compute 阶段、必需 Pipeline/绑定、可判定的 storage 角色、范围和调度协议。底层 SHALL 校验真实设备归属、活动接口及 native limits。非法的候选不得建立可执行 Compute 状态或发出底层 Dispatch。GraphManaged 下候选拒绝 SHALL 撤销旧的绘制和计算执行资格，并保留当前授权与已发前缀。普通录制沿用候选验证失败和原生失败的既有状态合同。超出已交付 Compute profile 的调用 SHALL 按支持范围明确拒绝。

#### Scenario: Reject a public compute candidate before native execution

- **WHEN** 在 raw 与 Validation 路径先使用合法 Compute 状态，再提交缺少活动绑定、错误存储角色或错误工作组数的候选或调用
- **THEN** 非法工作均无原生 Dispatch。公共预检可判定的候选错误在 Validation 层不委托底层，GraphManaged 不能在失败后使用旧状态发出额外命令
