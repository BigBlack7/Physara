# Spec Delta

## ADDED Requirements

### Requirement: Persist sparse anisotropy instance overrides in scene v1

场景 v1 SHALL 按目标实例及固定源 Primitive 保存已覆写的 `anisotropyStrength`、`anisotropyRotation` 及 `anisotropy` 纹理槽。显式零保持覆盖身份，省略字段继承固定源值。旋转以弧度保存。已覆写槽 SHALL 保存兼容的项目图像用途和已有的稀疏采样设置。打开候选 SHALL 在整体发布前依同一有效材质合同核对字段、范围、用途、有效 UV、可用切线及颜色组合。

#### Scenario: Round-trip extended v1 beside existing v1 documents

- **WHEN** 打开既有 v1，再保存并重新打开有显式零强度、非零弧度旋转和方向纹理设置的 v1
- **THEN** 两者通过同一文档入口，旧文档按源继承，新文档恢复所记录实例的零覆盖、角度、纹理和采样选择

#### Scenario: Keep the active document when anisotropy cannot be restored

- **WHEN** 打开候选中的方向槽用途错误，或合成后的目标 Primitive 缺乏该覆盖所需的切线空间
- **THEN** 报告对应字段或目标原因，活动场景、已确认路径及可用输出继续有效
