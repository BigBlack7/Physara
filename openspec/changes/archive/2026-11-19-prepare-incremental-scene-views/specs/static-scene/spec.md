# Spec Delta

## ADDED Requirements

### Requirement: Publish bounded render changes from committed scene state

Scene SHALL 随已接受事务发布对应于同一 Scene 身份和最终静态内容版本的变更。对象或灯的增删、源 Primitive 布局变化及活动实例有效模型代切换 SHALL 使渲染结构代失效。结构稳定时，变更 SHALL 标识有变化的对象、Primitive 源槽、有效材质覆盖或灯，包含从父级变换传播到实际渲染子记录的变化。环境变更 SHALL 提供新内容版本和对应的有效环境，保持无关对象、Primitive 和灯记录可复用。

连续事务 SHALL 按受影响的身份合并同一结构内的变更，待发布数据的规模 SHALL 受当前实体及源槽规模约束。撤销和重做 SHALL 走同一发布合同。失败、同值、纯名称和纯相机修改 SHALL 保留原待发布的静态变更及结构代。仅发布同路径新文件资产时，既有实例 SHALL 继续使用其已生效的固定资产代。

#### Scenario: Coalesce edits across input iterations without a frame slot

- **WHEN** 正常输入继续但帧槽暂不可用，同一父级、其下一个灯和一个 Primitive 覆盖被重复修改，期间还发生撤销重做、同值编辑和纯相机移动
- **THEN** 后续准备取得最终已接受状态所需的局部变更，覆盖实际受影响的渲染后代，每个受影响身份按用途合并，待发布规模与编辑次数无关

#### Scenario: Identify an actual change of the extraction layout

- **WHEN** 同路径的新模型先由资产服务发布，随后用户提交实例增删或活动实例采用新固定模型版本
- **THEN** 单独的资产发布保留现有实例和布局，已接受的结构变化提供新结构代，失效的旧索引不能用于解释新记录
