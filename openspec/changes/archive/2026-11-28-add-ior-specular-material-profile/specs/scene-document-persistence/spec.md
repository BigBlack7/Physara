# Spec Delta

## ADDED Requirements

### Requirement: Persist sparse IOR and specular instance overrides

场景 v1 SHALL 按选定模型实例的源 Primitive 槽保存已覆写的 `ior`、`specularFactor` 和 `specularColorFactor`，以及已覆写的 `specular` 与 `specularColor` 纹理槽。纹理覆盖保留其项目图像引用、用途和已编辑的采样设置。省略项继承固定的源模型，旧 v1 的缺省表达仍可直接读取。打开 SHALL 在发布候选前验证新字段的类型、数值、源 IOR 兼容模式约束和实际纹理用途。写出 SHALL 由同一规则恢复有效覆盖和数据域。

#### Scenario: Reopen an old or extended v1 document through the same entry point
- **WHEN** 打开没有新增覆盖字段的旧 v1，以及保存后重新打开含合法 IOR/specular 覆盖和纹理设置的 v1
- **THEN** 前者保留源继承，后者恢复各实例的有效值、图像引用和采样语义。非法 IOR 或纹理用途的候选返回可定位错误，当前活动文档继续有效
