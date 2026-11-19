# static-scene Specification

## Purpose

规定静态 Scene 对实体、实例和创作参数的所有权，以及编辑事务、源模型定位、世界变换和独立版本的可观察合同，为持久化与后续只读帧提取提供一致的 CPU 数据。

## Requirements

### Requirement: Distinguish persistent entities from live handles

Scene SHALL 为每个实体分配文档内唯一的非空稳定字符串 `EntityId`，并在快照和恢复时保留该身份。运行期实体访问 SHALL 校验 Scene 身份以及目标槽和 generation。新建实体使用新 ID，历史恢复原实体时可恢复原 ID 并取得新的运行期 handle。Scene SHALL 提供按稳定 ID 查找当前 handle 的入口。

#### Scenario: Reuse a deleted slot with a fresh generation
- **WHEN** Scene 删除一个已选中的实体，随后复用其槽位创建实体，先前的选择或待处理请求交回旧身份
- **THEN** 新实体取得独立的稳定 ID 和有效 handle，旧身份失效，迟到结果无法影响新实体

#### Scenario: Restore an entity into another scene lifetime
- **WHEN** 已保存的实体在重开的文档中恢复，或者历史在同一文档中撤销删除
- **THEN** 可按原稳定 ID 找到恢复的实体，删除或替换前的运行期身份仍被拒绝

### Requirement: Instantiate independent editable model instances

Scene SHALL 从一个 `CpuReady` 模型的所选源 scene 在单次事务中创建实例根和该 scene 的静态节点。实例 SHALL 持有其有效不可变模型版本。节点和 Primitive 槽的来源 SHALL 在该实例及对应源版本内解析，每个实例内同一源节点最多映射到一个实体。节点的名称、local TRS、层级、物体标志及相机和灯值 SHALL 由 Scene 持有，网格和共享材质 SHALL 从固定版本取得。

#### Scenario: Edit two instances from the same model independently
- **WHEN** 同一模型发布后被实例化两次，其中一个实例的节点变换、灯值或材质覆盖被编辑
- **THEN** 两实例继续共享不可变网格和材质，另一实例的创作数据及源记录保持原值

#### Scenario: Keep an active instance when its asset is refreshed
- **WHEN** AssetManager 为同一路径及所选 scene 发布新版本，或一次新实例化因节点无法转换为支持的 local TRS 而失败
- **THEN** 已有实例仍解析其原来的有效版本，失败的创建不留下部分实例、实体或来源引用

### Requirement: Apply hierarchy mutations as domain transactions

Scene SHALL 通过类型化事务创建、删除、重设父级和编辑实体，验证真实目标、父链接、无环和组件引用后再提交。重设父级 SHALL 明确指定保持 local TRS 或保持 world pose。保持 world pose SHALL 基于截至事务的当前层级，只有新父可逆且求得的 local 可表示为有限 TRS 时才提交。删除子树 SHALL 退役其所有运行期身份。删除包含模型实例根的子树时，事务 SHALL 一并移除该实例由 SourceRef 关联且已移出子树的实体及其后代。

对含相机、方向灯或聚光灯的受影响子树，保持 world pose SHALL 同时保持几何世界变换及这些组件的刚体朝向。若两者无法由同一个合法 local TRS 表达，SHALL 拒绝事务并保留原层级和版本。

#### Scenario: Reject a cycle or unrepresentable reparent
- **WHEN** 请求挂到自身或后代，或者请求保持 world pose 但新父不可逆或所需的 local 含 shear
- **THEN** Scene 返回可判断的失败，既有父子链接、local 值、版本和撤销游标保持有效

#### Scenario: Reparent with an explicit transform policy
- **WHEN** 同一实体先按保持 local 挂到另一父级，再按保持 world pose 挂到允许的父级或根
- **THEN** 首次保留 local 并据新父计算 world，后一次保留原 world 并保存相应的新 local

#### Scenario: Delete an instance with a reparented source node
- **WHEN** 实例的源节点已挂到实例外的父级，随后删除实例根
- **THEN** 实例根、其剩余源实体及其被删除后代同时退出，其他实体不会留下指向已删除根或节点的引用

#### Scenario: Preserve a camera direction on a keep-world reparent
- **WHEN** 含相机或定向光源的子树从带镜像缩放的父级请求保持 world pose 地重挂
- **THEN** 如果局部 TRS 能同时保持子树的世界变换和刚体朝向则整体提交，否则拒绝且保留此前的视线、灯方向、层级和版本

### Requirement: Propagate transforms and bounds from dirty roots

Scene SHALL 按右手坐标和列向量合同组合 `worldFromLocal = worldFromParent * T * R * S`。local SHALL 接受表示范围内的非均匀、负和零缩放，拒绝非法数值、无效旋转和不可表示的 local 变换。Scene SHALL 由编辑或候选构建登记脏根，去掉被脏祖先覆盖的根，按父先子后更新受影响的 world 和有效网格 bounds。世界包围盒 SHALL 包含局部有界几何经完整 world 变换的范围。无局部有效 bounds 的几何 SHALL 报告无相应 bounds。

相机和灯的世界位置 SHALL 取同一世界变换下的实体原点。相机及方向灯、聚光灯的世界朝向 SHALL 从父先子的旋转组合取得，以单位方向表达局部 `-Z`，保留相机局部 `+Y` 的上方向。缩放 SHALL 通过完整父子变换影响原点，灯的 range 和锥角保持标准米制距离及角度。派生位置、朝向和几何 bounds SHALL 随同一事务一致发布。

#### Scenario: Transform bounds under nested signed scale
- **WHEN** 父级变换同时影响带旋转、非均匀及负缩放的子节点，父与子均已登记为脏
- **THEN** Scene 按完整父子组合更新实际子树一次，世界 AABB 覆盖已知局部盒的全部变换角点，反射语义保留给下游

#### Scenario: Advance a clean scene without walking its entities
- **WHEN** Scene 已处理本轮变换且下一轮没有脏根
- **THEN** 增量更新从空队列或版本检查返回，世界结果和变换更新版本原样保持

#### Scenario: Derive a rigid camera pose under a scaled parent
- **WHEN** 具有非均匀、有符号或零轴缩放的父级移动或旋转带相机和聚光灯的子节点
- **THEN** 世界原点按完整层级求值，视线与发射方向仍由单位旋转确定，灯的范围和锥角保留原值

### Requirement: Own sparse material and texture overrides per instance

Scene SHALL 按实体与锁定模型版本内的源 Primitive 槽保存已修改的材质字段和纹理槽字段。有效值 SHALL 用固定模型的源材质与该实例覆盖解析，删除覆盖时恢复源值或对应的标准缺省。纹理覆盖 SHALL 可选择资源、用途对应的采样解释、sampler、现有 UV 集和 UV 变换，解析后仍保留原始网格的 UV 能力。参数域、源槽、纹理用途、必要资产与有效 UV SHALL 在应用前验证。

#### Scenario: Override a field and a texture interpretation without copying the model
- **WHEN** 一个 Primitive 把核心材质因子和一处纹理路径、sampler 或 UV 变换设为实例值，再移除覆盖
- **THEN** 期间只有目标实例的有效描述采用覆盖，移除后相应字段回到固定模型的源语义，源材质、图像及另一实例保持原样

#### Scenario: Reject a texture override incompatible with the target
- **WHEN** 命令定位到不存在的 Primitive，或覆盖使用其未提供的 UV 集或错误的纹理用途
- **THEN** 事务返回定位错误，目标已生效的材质值、纹理引用及版本保持有效

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

### Requirement: Apply and replay environment edits on the owning scene

Scene SHALL 拥有一项可缺省的环境请求。添加、替换、改字段和移除 SHALL 在活动 Scene 上原子应用。存在的请求 SHALL 固定有效 CPU 环境版本，持有单位环境至世界旋转、有限非负倍率及至少一个用途。有效修改 SHALL 留存足够重放的前值与末值及其源引用，Undo/Redo SHALL 在同一 Scene 且当前值满足事务前提时通过相同校验重放。拒绝和同值输入 SHALL 保留当前请求及关联 revision。

#### Scenario: Replay a source replacement without rebinding asset history
- **WHEN** Scene 完成固定环境 A 到固定环境 B 的替换，随后撤销和重做
- **THEN** 两次重放分别恢复已接受的 A 和 B 及该项事务的参数，原场景身份与实体保持有效，随请求取得的 CPU 版本仍可使用

#### Scenario: Refuse to replay an environment transaction into a different scene
- **WHEN** 一个来自退役文档的环境编辑或历史记录试图对活动 Scene 生效
- **THEN** 活动 Scene 拒绝该事务并保留已有环境和版本
