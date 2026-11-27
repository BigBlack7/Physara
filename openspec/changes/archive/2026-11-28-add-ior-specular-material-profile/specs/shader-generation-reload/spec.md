# Spec Delta

## ADDED Requirements

### Requirement: Prepare and admit IOR and specular color roles

产品 Shader 目录 SHALL 为受支持的 IOR/specular 颜色角色声明有限的结构变体。角色 SHALL 按使用的全灯或 Froxel、直接光或物理环境、阴影资源匹配现有消费者，并具有扩展颜色实际使用的 specular 强度和颜色纹理及采样器。仅需几何覆盖或表面数据的角色 SHALL 以其实际资源配方准备。核心颜色角色的活动资源合同仅包含自身所需输入。

打包冷准备与热候选 SHALL 校验相同的 ABI、角色和阶段活动资源。即使当前 View 未选择扩展角色，缺项或类型、阶段、槽位不兼容的候选 SHALL 给出具体原因。待执行的 View 只消费就绪的匹配色角色和材质代。发布失败 SHALL 沿现有机制继续使用旧完整代及其在途持有，后续匹配候选在帧边界确认。

#### Scenario: Reject an incompatible IOR and specular program in both preparation paths
- **WHEN** 冷包缺少扩展颜色角色必需的 specular 纹理或采样器，热候选包含同类缺项且角色当前未被 View 选用，随后提供兼容版本
- **THEN** 两种准入均定位相应角色与资源，热拒绝期间旧完整代仍可用于已有计划，合法候选发布后的正常提交使用匹配的新 Shader 代与实际纹理
