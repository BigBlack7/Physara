# Spec Delta

## ADDED Requirements

### Requirement: Prepare and admit the finite sheen lighting roles

产品 Shader SHALL 为 sheen 及其与清漆共用的颜色声明有限角色，按全灯或 Froxel、Direct 或 Physical 的实际照明需求、阴影匹配相应消费者。色角色 SHALL 具有配对材质表、两槽及其采样器、活动 UV 输入和匹配 DFG 读。环境 sheen 角色 SHALL 具有匹配的预滤波环境活动读。组合角色 SHALL 同时满足清漆两套法线框架的阶段合同。独立表面、深度、阴影和诊断仅按其实际输入准入。

产品冷包与开发热候选 SHALL 按一致的 ABI、活动资源、阶段、槽位、绑定类型和真实设备限制准入所声明的角色。当前未选角色的缺项也 SHALL 拒绝不完整候选并报告原因。活动与候选的准备 SHALL 按有界预算执行，整代匹配后才允许新正常提交确认。

#### Scenario: Prepare a reflected sheen role and its same-generation resources

- **WHEN** 在真实设备上准备单独 sheen 及含清漆、阴影和 IBL 的 sheen 颜色，并同时存在独立表面用途
- **THEN** 各角色通过对应生成 ABI、两阶段活动资源与设备准入。所用颜色从实际绑定的相符材质、DFG、遮挡和环境交付，独立表面仅消费自身资源

#### Scenario: Refuse a missing sheen binding in both cold and unselected hot roles

- **WHEN** 一个冷包和一个热候选的 sheen 角色缺少必需的活动绑定，热错误角色当前未被选择，随后从合法输入得到完整候选
- **THEN** 冷、热准入定位角色与缺项，已有可用代保留有效输出资格，匹配候选在整代就绪后由新正常帧确认
