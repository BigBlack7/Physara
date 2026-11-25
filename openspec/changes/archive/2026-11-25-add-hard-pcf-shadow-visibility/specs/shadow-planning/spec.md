# shadow-planning Spec Delta

## ADDED Requirements

### Requirement: Plan the complete shadow filter footprint

阴影规划 SHALL 覆盖当前基线允许的最大采样核及接收偏移。各深度区域 SHALL 交付同一次计算的有效接收范围、带余量的栅格范围、投影和世界 texel 尺度。余量 SHALL 涵盖吸附、交付精度和接收偏移造成的位移，并使保守 caster 筛选保留实际比较核可达的遮挡物。方向光 SHALL 按有效存储尺度稳定吸附，并保持过渡两级的覆盖。聚光 SHALL 保留名义外锥测试，点光各面 SHALL 覆盖标称面边及交角所需的核。无法按交付精度提供所需覆盖时 SHALL 记录该组不可用及原因。

在其他有效生产输入和布局相同时，Hard 与 PCF 的切换 SHALL 复用已拟合投影、分配和内容版本。分辨率或生产覆盖改变时 SHALL 更新受影响的区域及所需内容。所有余量 SHALL 计入有效阴影层尺寸及现有逻辑预算。

#### Scenario: Keep the maximum footprint valid at a receiver boundary

- **WHEN** 有效接收点紧邻级联接收边缘、聚光外锥或点光三面交角，且一个遮挡物位于主可见集外并覆盖过滤可达范围
- **THEN** 拟合的投影、区域及候选覆盖允许最大核访问其所属组的有效 texel，包括受接收偏移影响的最外样本。名义光照范围及其外侧的中性查询语义保持。

#### Scenario: Change filtering without requiring new depth contents

- **WHEN** 同一冻结场景以相同有效区域和相机在 Hard 与 PCF 间切换
- **THEN** 过滤选择更新，原层的投影、区域分配和所需深度版本保持匹配，改变实际生产覆盖的编辑才使相应版本失效。
