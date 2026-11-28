# Spec Delta

## ADDED Requirements

### Requirement: Persist sparse sheen instance overrides

场景 v1 SHALL 按实例及固定源 Primitive 保存已覆写的 `sheenColorFactor`、`sheenRoughnessFactor` 和 `sheenColor`、`sheenRoughness` 纹理槽。已覆写槽 SHALL 保留有效项目图像引用、用途和分别编辑的采样设置。显式零 SHALL 保留覆盖身份，省略项 SHALL 继承源记录。打开 SHALL 在发布候选前核对类型、范围、材质兼容性、槽用途及可用 UV。

#### Scenario: Reopen an old or extended v1 document

- **WHEN** 用同一入口打开已有 v1，并将包含显式零颜色及两个独立 sheen 槽设置的 v1 保存后重开
- **THEN** 旧文档恢复既有的源继承，新文档恢复目标实例的稀疏覆盖和两槽各自的图像与采样语义

#### Scenario: Reject an incompatible sheen slot during open

- **WHEN** 打开一个 sheen 粗糙度槽引用不兼容用途纹理或需要缺席 UV 的 v1
- **THEN** 打开报告对应目标与原因，活动文档、已确认路径及可用结果保有资格
