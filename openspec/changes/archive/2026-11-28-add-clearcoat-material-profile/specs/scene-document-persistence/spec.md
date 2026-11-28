# Spec Delta

## ADDED Requirements

### Requirement: Persist sparse clearcoat instance overrides

场景 v1 SHALL 按模型实例和源 Primitive 槽保存已覆写的 `clearcoatFactor`、`clearcoatRoughnessFactor`、`clearcoatNormalScale` 及 `clearcoat`、`clearcoatRoughness`、`clearcoatNormal` 纹理槽。已覆写纹理 SHALL 保留项目图像引用、与槽匹配的用途及独立采样设置。省略项继承固定源模型，显式零因子保留覆盖身份。写出和重新打开 SHALL 使用同一有效材质规则恢复基底和清漆的各自覆盖。

打开 SHALL 在发布文档候选前校验字段类型、数值、纹理用途、材质组合及目标几何的有效 UV 和切线条件。含清漆覆盖的 v1 与省略清漆字段的既有 v1 SHALL 走现有文档入口。

#### Scenario: Reopen existing and extended v1 documents with sparse clearcoat values
- **WHEN** 打开缺少清漆覆盖字段的旧 v1，并通过同一入口保存和重新打开带有显式零因子、清漆法线槽及采样编辑的 v1
- **THEN** 旧文档继续继承源材质，新文档恢复目标实例的显式值、图像用途、采样和基底覆盖

#### Scenario: Reject an incompatible clearcoat override before document publication
- **WHEN** 待打开的 v1 将颜色用途图像引用写入清漆法线槽，或其清漆贴图需要目标 Primitive 缺失的 UV 或切线空间
- **THEN** 打开交付对应字段或槽的原因，当前活动文档、路径和可用结果继续有效
