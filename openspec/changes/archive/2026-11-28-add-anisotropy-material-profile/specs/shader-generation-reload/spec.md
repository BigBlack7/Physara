# Spec Delta

## ADDED Requirements

### Requirement: Admit the finite anisotropy color roles with their real device bindings

产品 SHALL 声明一组有限的各向异性颜色角色，覆盖全灯及 Froxel、Direct 或 Physical 的实际环境需求和阴影配方。色角色 SHALL 使用匹配的生成材质布局、方向纹理和采样器、所需的 UV 与切线阶段接口。环境及阴影资源 SHALL 服从相应模式的活动绑定。其它角色依其实际消费资源准入。

打包冷准备和开发热重载 SHALL 核对角色身份、匹配的 ABI、活动阶段输入、资源类型和槽位，并通过真实设备绑定限制。同代候选中当前未被选择的各向异性角色 SHALL 完成资源准入。完整活动代及其候选 SHALL 按既有有界缓存与帧前准备预算推进。只有对应角色全部完整兼容的候选代 SHALL 发布；失败 SHALL 保有旧有效代并报告角色及原因。

#### Scenario: Prepare the new roles and a complete source reload

- **WHEN** 在正式产品及开发热重载入口分别请求各向异性受光，并在已有活动代时提交合法完整候选
- **THEN** 实际色角色使用配对的阶段接口、资源和当前设备配方，候选在记录的缓存和帧前预算内整体发布。其颜色身份随匹配的正常提交更新

#### Scenario: Refuse a broken anisotropy role in a cold or unselected hot package

- **WHEN** 一个冷包或热候选中对应色角色缺必需方向图资源、切线阶段输入或超出真实绑定限制，且热错误角色当前尚未被 View 选择
- **THEN** 受影响的冷角色和完整热候选分别被拒绝并报告对应缺项或限制。旧有效代及独立可用的表面交付保持有效
