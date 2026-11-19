# static-scene Delta

## ADDED Requirements

### Requirement: Apply and replay environment edits on the owning scene

Scene SHALL 拥有一项可缺省的环境请求。添加、替换、改字段和移除 SHALL 在活动 Scene 上原子应用。存在的请求 SHALL 固定有效 CPU 环境版本，持有单位环境至世界旋转、有限非负倍率及至少一个用途。有效修改 SHALL 留存足够重放的前值与末值及其源引用，Undo/Redo SHALL 在同一 Scene 且当前值满足事务前提时通过相同校验重放。拒绝和同值输入 SHALL 保留当前请求及关联 revision。

#### Scenario: Replay a source replacement without rebinding asset history
- **WHEN** Scene 完成固定环境 A 到固定环境 B 的替换，随后撤销和重做
- **THEN** 两次重放分别恢复已接受的 A 和 B 及该项事务的参数，原场景身份与实体保持有效，随请求取得的 CPU 版本仍可使用

#### Scenario: Refuse to replay an environment transaction into a different scene
- **WHEN** 一个来自退役文档的环境编辑或历史记录试图对活动 Scene 生效
- **THEN** 活动 Scene 拒绝该事务并保留已有环境和版本

## MODIFIED Requirements

### Requirement: Advance distinct revisions on accepted state changes

Scene SHALL 分别记录持久化实体及环境配置的 `DocumentRevision`、影响像素的非相机静态状态 `RenderContentRevision`，以及影响相机输入的 `CameraRevision`。View SHALL 独立管理其相机请求版本及可持久化配置的 revision。成功应用、撤销、重做 SHALL 按实际变更通过同一归属入口推进对应 revision，无效请求与同值编辑保持 revision。

相机投影或其世界刚体位姿变化 SHALL 推进 Scene 的相机版本，View 导航位姿及相机选择变化 SHALL 推进该 View 的相机请求版本。场景内容版本 SHALL 反映同一事务涉及的渲染对象或灯的变化，包括改变相机实体自身的可渲染组件、灯的有效位置或方向、以及其渲染子树的世界状态。有效环境源、朝向、强度、用途改变或移除 SHALL 推进 Scene 的文档和渲染内容 revision，相机相关状态保持原值。

#### Scenario: Separate naming, camera and render content changes
- **WHEN** 依次重命名实体、调整纯相机实体或主 View 导航位姿，再移动同时带有网格或灯的相机实体或影响渲染子树的祖先
- **THEN** 重命名只改变 Scene 文档版本，纯相机变化推进相关相机和持久化版本，最后一项同时影响对应的相机和静态内容版本

#### Scenario: Invalidate only versions implicated by replay
- **WHEN** 用户撤销和重做已接受的静态参数编辑，其间又提交一次同值或失败的修改
- **THEN** 重放产生与普通领域编辑一致的对应 revision 变化，同值或失败修改保留已接受的状态和版本

#### Scenario: Track a changed direction despite singular geometry scale
- **WHEN** 一个相机实体兼具方向灯或聚光灯，层级的零缩放使组合几何矩阵保持原值，但实体旋转改变了实际视线和灯方向
- **THEN** Scene 仍发布新的世界朝向，并推进相机和静态内容版本；只有纯相机的同类变化时保留静态内容版本

#### Scenario: Track a persisted environment edit as static content
- **WHEN** 用户对活动 Scene 的环境作出有效修改，随后保存并重开
- **THEN** 修改推进所属 Scene 的文档与渲染内容版本，后续帧快照取得新请求；重开恢复所保存的源和字段，相机与主 View 持久化请求仍由各自 owner 处理
