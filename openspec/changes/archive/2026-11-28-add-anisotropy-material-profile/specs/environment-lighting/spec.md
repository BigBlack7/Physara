# Spec Delta

## ADDED Requirements

### Requirement: Evaluate anisotropic specular IBL with a matched bent reflection direction

各向异性环境镜面 SHALL 从同一着色法线、视向、扩展方向的垂直轴、有效强度和过滤后感知粗糙度求 bent-reflection 方向。SHALL 使用本项目固定的 Filament 近似，在相同世界和环境坐标合同下以该方向采样现有 GGX 预滤波 cube，LOD 来自基底过滤后的感知粗糙度。DFG 与能量补偿按基底 NoV、粗糙度和对应 Fresnel 合同，环境 SH 漫反射沿用基底着色法线。环境强度、遮蔽和预曝光按原有求值位置应用。各向异性基底的环境镜面 SHALL 完整保留，不参加等向基底的 SSR 替换。

零强度及等轴粗糙度边界 SHALL 恢复相应的等向环境响应。均匀单位环境 SHALL 交付与所声明的 split-sum 近似匹配的基底方向反照率。Physical 仅在环境用途和必要产品齐备时加入间接照明。Direct 与无环境 Physical 的直接光继续遵循各自的基底 DFG 规则。

#### Scenario: Resolve a rotated directional environment highlight

- **WHEN** 非均匀环境照射有明确切线轴的拉丝标样，独立改变材质旋转、环境旋转和视向，并提供零强度对照
- **THEN** 各向异性环境高光随预测的弯曲方向采样，零强度回到基底反射，环境朝向与材质方向可分别验证。相同曝光设置下天空与漫反射沿各自的合同

#### Scenario: Retain finite white-furnace response at roughness limits

- **WHEN** 用单位均匀环境检查各向异性及对应等向的介质和金属样点，覆盖粗糙度下限、等轴上限和掠射
- **THEN** HDR 有限，白炉读回符合匹配 DFG 和基底能量约定。等轴上限与等向对照一致
