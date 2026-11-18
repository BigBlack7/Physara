# Spec Delta

## ADDED Requirements

### Requirement: Persist one optional scene environment request

v1 SHALL 支持可省略的顶层 `environment` 对象。对象存在时 SHALL 保存受控的 `path`、从环境空间到世界空间的单位 xyzw `rotation`、有限非负 `intensity`、独立布尔用途 `lighting` 和 `sky`，其中至少一项用途为真。省略对象 SHALL 表示场景未配置环境。Open SHALL 从给定的项目根解析 `Assets/Environments` 下的普通文件，与当前 Open 操作核对实际源并固定有效 CPU 版本后才整体发布 Scene 与 View。解码失败或环境字段非法时 SHALL 沿现有文档错误合同报告字段或资产。Save SHALL 捕获 Scene 的环境请求和对应文档版本，保存请求字段。标定来自有效源，像素和有效 GPU 状态 SHALL 从拥有者获得。

#### Scenario: Reopen a v1 scene with or without an environment

- **WHEN** 先打开缺少 `environment` 的合法 v1 文件，再保存并重开具有合法非默认环境朝向、强度和单独天空用途的 v1 场景
- **THEN** 第一份文档恢复为未配置环境，后一份恢复所保存的环境请求和值，GPU 就绪状态按当前帧准备另行产生

#### Scenario: Reject an invalid environment without replacing the active document

- **WHEN** 待打开的 v1 文档提交越界环境路径、非法旋转或一个未能成为有效 CPU 版本的源
- **THEN** 打开报告对应的文件和字段或资产错误，先前活动 Scene、View 请求及文档路径仍有效
