# Spec Delta

## ADDED Requirements

### Requirement: Prepare and admit the clearcoat color roles

产品 Shader 目录 SHALL 为清漆定义有限的受光颜色角色，与全灯或 Froxel、Direct 或 Physical、实际环境和阴影组合对应。核心与 IOR/specular 基底 SHALL 由清漆角色处理其相应清漆材质数据。角色 SHALL 具有清漆强度、粗糙度、法线的活动纹理和配套 sampler，并为所需的两个表面框架使用匹配的生成接口。核心及仅 IOR/specular 颜色、独立表面、深度、阴影和诊断 SHALL 按对应消费者的阶段活动资源合同准备。

冷包与开发热候选 SHALL 对所有相关角色核对相同 ABI、阶段、资源类型、槽位及绑定能力。未选的清漆角色也 SHALL 通过候选准入。准备与发布 SHALL 按实际预算推进，失败时给出可定位的角色和资源原因，旧完整代及其已经持有的输出继续沿提交合同服务。

#### Scenario: Prepare a packaged clearcoat program on real device limits
- **WHEN** 打包产品在当前设备首次为可见清漆材质准备受光颜色，另一个 View 只需要表面准备
- **THEN** 受光色角色使用配对的材质表、清漆纹理及实际设备允许的绑定，独立表面只消费其所需资源。准备就绪后接受的颜色使用该角色和有效材质

#### Scenario: Reject a missing clearcoat binding before cold or hot publication
- **WHEN** 冷包缺少所需清漆活动绑定，随后未被 View 选中的热候选包含同类错误，再提供完整候选
- **THEN** 冷包和热候选均定位到相应角色、阶段和资源，原有效代保持可用资格，合法恢复只在同一代的真实正常提交后确认新颜色
