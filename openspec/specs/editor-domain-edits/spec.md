# editor-domain-edits Specification

## Purpose

规定编辑器会话如何把控件和快捷键产生的领域意图应用到实际 View 和工作区状态，并定义连续编辑、撤销重做、失败及会话结束时可观察的行为。

## Requirements

### Requirement: Apply field-level editor commands at a frame boundary

编辑器会话 SHALL 在封存 CPU UI 后、视口准备和录制前应用当前批次的有效编辑。View SHALL 持有主视口请求渲染比例及持久化相机请求，Scene SHALL 持有实体、层级及实例创作值，EditorSession SHALL 持有主视口显隐状态、选择和会话编辑历史。面板 SHALL 读取相应 owner 的快照并提交字段级意图。应用入口 SHALL 校验目标身份和字段值。渲染比例 SHALL 为 `[0.5, 1.0]` 内的有限值，初始请求为 `1.0`，主视口初始可见。用户发起的离散编辑实际改变字段时 SHALL 形成一条历史。Open 冻结期间 SHALL 拒绝文档字段的新编辑和相应重放，仍处理非文档动作。CPU UI 在槽忙时仍可推进，已经封存并进入录制的工作只消费属于自己的准备结果。

#### Scenario: Apply edits to separate owners
- **WHEN** 用户改变主 View 比例，或通过工作区动作改变主视口显隐
- **THEN** 相应 owner 在帧边界接收字段变化，后续快照反映新的请求比例或显隐状态，两个字段及窗口原生设置各自独立

#### Scenario: Reject an invalid or expired edit
- **WHEN** 编辑目标属于失效 View、Scene 或会话，或比例为非有限值或越界值
- **THEN** 应用入口返回有界拒绝原因，已接受的请求、有效输出和撤销重做历史保持有效

#### Scenario: Retire selection and camera references with a deleted entity
- **WHEN** 一项有效 Scene 事务删除当前选中的实体或主 View 引用的相机
- **THEN** 当前选择与旧运行期请求身份失效，受影响主 View 同一编辑边界回到导航相机，场景快照与 View 请求保持可一起保存

### Requirement: Coalesce one continuous viewport edit

一次渲染比例拖拽或导航位姿手势 SHALL 从同一目标和字段的首个原值开始。一次 Inspector 的本地 Transform 手势 SHALL 遵循同一事务协议。每次有效的 CPU 帧边界 SHALL 应用该帧最后一个合法预览值。正常结束 SHALL 在末值与原值不同的情况下形成一条可撤销记录。取消 SHALL 通过同一字段入口恢复原值并结束本次事务。目标已退役时 SHALL 结束事务且不得向新目标写入首值。会话 SHALL 在一次活动事务结束前暂停历史重放。同一事务修改的字段之外的独立值 SHALL 保持有效。

#### Scenario: Preview several values and finish once
- **WHEN** 用户在同一滑块上完成跨数帧的拖拽，中间给出多个比例
- **THEN** owner 收到各有效帧最后的比例，释放后一次 Undo 恢复拖拽前的请求，一次 Redo 恢复最终请求

#### Scenario: Cancel an interrupted edit
- **WHEN** 活动拖拽收到取消输入，或输入失效边沿使活动交互终止
- **THEN** 该事务恢复首个请求值并丢弃未提交记录，迟到的拖拽或释放事件无法再次应用旧编辑，既有撤销重做记录保持有效

#### Scenario: Complete an edit with no net change
- **WHEN** 用户结束拖拽时比例已回到开始时的值，或离散操作提交当前值
- **THEN** 历史条数和可重做分支保持原状

#### Scenario: Commit an Inspector value when switching controls
- **WHEN** 用户在 Inspector 的一个轴输入有效数值，点击另一个字段或同一向量的另一轴，并修改该新控件
- **THEN** 旧控件的末值先完成提交，新手势从已接受的状态开始，两次有净变化的编辑按各自完成顺序撤销和重做

#### Scenario: Replay a completed navigation gesture
- **WHEN** 一次已捕获的导航手势连续更新主 View 位姿并正常结束，随后执行 Undo 和 Redo
- **THEN** 一条历史通过 View 请求分别恢复起始和最终位姿，Scene 文档和静态内容版本保持原值，变化由 View 的相机及持久化 revision 表达

### Requirement: Replay completed edits with undo and redo

会话 SHALL 在有界历史中按完成顺序保存实际编辑及其可重放的原值和末值。Undo 和 Redo SHALL 通过原字段或 Scene 领域事务入口重放，并在该请求被 owner 接受后移动历史记录。恢复实体时 SHALL 重建运行期身份，通过稳定 ID 复原本条记录负责的领域引用，包括受该事务影响的主 View 相机请求。新完成的有效编辑 SHALL 清除被分叉的 Redo 记录。成功切换文档 SHALL 退役旧编辑历史及选择，保留会话实际布局。输出准备结果和窗口原生设置 SHALL 各自管理其状态。

#### Scenario: Undo and redo an interleaved sequence
- **WHEN** 比例和主视口显隐分别完成有效编辑，用户连续执行 Undo 再执行 Redo
- **THEN** 两个 owner 按反序撤销和原序重做，每次仅修改对应字段，视口有效输出继续按当前比例请求的实际准备和接受结果发布

#### Scenario: Replace the redo branch with a new edit
- **WHEN** Undo 后存在可重做记录，用户完成另一项实际改变字段的编辑
- **THEN** 新编辑成为最新可撤销记录，原分支的 Redo 不再可用

#### Scenario: Replay a scene edit after restoring its target
- **WHEN** 实体被编辑后删除，用户撤销删除再撤销该字段编辑，然后依次重做
- **THEN** 恢复的实体取得新 handle，历史在同一文档按稳定 ID 通过相同领域入口应用，删除和对应相机引用处理随重放保持一致

#### Scenario: Expire the previous document's edit history
- **WHEN** 编辑记录、选择或旧文档请求仍存在，另一文档成功打开或新建
- **THEN** 旧记录与身份无法作用于新文档，即使新文档恰好含同名或同 ID 实体，当前工作区布局保持原状

### Requirement: Retire editor edits on session shutdown

结束会话 SHALL 停止产生和消费该会话的编辑意图，并丢弃未完成事务、未消费请求及会话历史。新会话 SHALL 建立独立的编辑状态。已经提交的图形工作继续由其原票据和依赖保活合同管理。

#### Scenario: Close a session during an unfinished edit
- **WHEN** 关闭意图或有界运行结束发生在事务未提交时，随后建立另一编辑器会话
- **THEN** 关闭后无旧会话编辑或原生请求被应用，重新接入后的 Undo 和 Redo 从空历史开始，旧输入不能命中新会话的动作

### Requirement: Select and edit entities in the active hierarchy

层级树 SHALL 展示活动 Scene 已提交的父子关系，并以独立于显示名称的身份区分每一行。选择一行 SHALL 解析为当前 Scene 的存活实体。Transform Inspector SHALL 读取和编辑选中实体的本地平移、旋转及有符号缩放。编辑 SHALL 通过 Scene 事务更新指定的本地字段，保留其它创作值，并按当前父变换传播世界数据。拒绝输入 SHALL 显示原因并保留最近接受的字段值。Inspector 的连续操作 SHALL 遵循视口字段编辑的单次手势、取消及撤销重做合同。

工作区 Delete SHALL 对当前有效选择提交 Scene 子树删除。成功后 SHALL 在同一编辑边界更新层级并退役无效的选择和相机引用。Undo 和 Redo SHALL 通过现有领域事务恢复与删除相应场景记录；恢复实体后须通过层级树重新选择。

#### Scenario: Inspect either of two equally named entities
- **WHEN** 两个实体显示名称相同，其中一个位于父实体下，用户通过层级树依次选择它们
- **THEN** 每次 Inspector 显示相应实体的本地字段，选择不推进文档或渲染内容版本

#### Scenario: Edit and replay a transform under a scaled parent
- **WHEN** 用户连续拖动带非均匀负缩放父级下的子实体局部轴，随后执行 Undo 和 Redo
- **THEN** 父实体和子实体其他本地字段保持原值，世界变换和可见几何随有效预览及每次重放改变，一次 Undo 恢复开始值，一次 Redo 恢复末值

#### Scenario: Delete the selection and retire references
- **WHEN** 用户通过删除动作移除当前选择的子树，其中包括主 View 使用的场景相机，随后撤销并重做
- **THEN** 删除时 Inspector 清空且 View 回退到自己的导航位姿；撤销恢复实体及相机引用并使用新的运行期实体身份，选择保持空；重做再次移除相应实体和引用

### Requirement: Instantiate models from an eligible editor import

工作区 SHALL 从资源根下的 `Assets/Models` 接受只读 glTF/GLB 路径与可选的源 scene 下标，通过共享资产服务请求模型。会话 SHALL 至多持有一个待实例化的导入意图，提交时同时核对操作、活动 Scene 和准确的资产请求身份。只有匹配的已发布 CPU 版本且文档可编辑时，才 SHALL 在可提交的编辑边界原子创建一棵独立实例子树，作为一笔可撤销编辑。模型导入 SHALL 显示正在加载、CPU 就绪待提交、已实例化、失败或取消的实际进度与可用诊断。已实例化状态 SHALL 与视口的 GPU 等待、拒绝和实际输出确认独立。

取消导入、已接受的 New/Open 或会话终止 SHALL 撤销该导入意图的实例发布资格，保留其它消费者的共享资产请求。准入失败、模型加载失败、请求代失效或实例化失败 SHALL 显示有界原因，活动 Scene 和历史保持有效。正常再次导入同一源可复用不可变记录并创建新的独立实例。

#### Scenario: Import a project model into the active scene
- **WHEN** 用户导入一个有效模型，CPU 记录发布后正有一项尚未结束的领域手势
- **THEN** 会话显示 CPU 就绪且等待实例化，手势结束后的有效编辑边界只提交一棵完整实例，导入报告可查看，一次 Undo 移除该实例，视口颜色仍须经自己的接受回执确认

#### Scenario: Reuse the same asset for two imports
- **WHEN** 用户完成一次导入后，再明确导入同一路径和源 scene
- **THEN** 两次各创建独立实体和各自的历史，选中并修改其中一个实例的创作值只改变该实例

#### Scenario: Discard a cancelled or superseded import
- **WHEN** 用户取消一个待实例化导入或已接受 New/Open，之后原模型的 CPU 结果仍到达
- **THEN** 原请求无法向任何活动文档添加实例或历史，其它使用同一资产请求的消费者仍可按原合同取得结果

#### Scenario: Report a rejected model without a partial instance
- **WHEN** 导入路径越界、模型无法加载，或该请求因其它资产操作推进源代而失效
- **THEN** 导入显示对应的失败原因，已有实体和选择保持有效，失败请求留下的旧 CPU 版本不能成为本次实例
