# editor-domain-edits Specification

## Purpose

规定编辑器会话如何把控件和快捷键产生的领域意图应用到实际 View 和工作区状态，并定义连续编辑、撤销重做、失败及会话结束时可观察的行为。

## Requirements

### Requirement: Apply field-level editor commands at a frame boundary

编辑器会话 SHALL 在封存 CPU UI 后、视口准备和录制前应用当前批次的有效编辑。View SHALL 持有主视口请求渲染比例，EditorSession SHALL 持有主视口显隐状态和会话编辑历史。面板 SHALL 读取相应 owner 的快照并提交字段级意图。应用入口 SHALL 校验目标身份和字段值。渲染比例 SHALL 为 `[0.5, 1.0]` 内的有限值，初始请求为 `1.0`，主视口初始可见。用户发起的离散编辑实际改变字段时 SHALL 形成一条历史。CPU UI 在槽忙时仍可推进，已经封存并进入录制的工作只消费属于自己的准备结果。

#### Scenario: Apply edits to separate owners
- **WHEN** 用户改变主 View 比例，或通过工作区动作改变主视口显隐
- **THEN** 相应 owner 在帧边界接收字段变化，后续快照反映新的请求比例或显隐状态，两个字段及窗口原生设置各自独立

#### Scenario: Reject an invalid or expired edit
- **WHEN** 编辑目标属于失效 View 或会话，或比例为非有限值或越界值
- **THEN** 应用入口返回有界拒绝原因，已接受的请求、有效输出和撤销重做历史保持有效

### Requirement: Coalesce one continuous viewport edit

一次渲染比例拖拽 SHALL 从同一目标和字段的首个原值开始。在每次有效的 CPU 帧边界 SHALL 应用该帧最后一个合法预览值。正常结束 SHALL 在末值与原值不同的情况下形成一条可撤销记录。取消 SHALL 通过同一字段入口恢复原请求并结束本次事务。活动事务期间 SHALL 暂停历史重放。

#### Scenario: Preview several values and finish once
- **WHEN** 用户在同一滑块上完成跨数帧的拖拽，中间给出多个比例
- **THEN** owner 收到各有效帧最后的比例，释放后一次 Undo 恢复拖拽前的请求，一次 Redo 恢复最终请求

#### Scenario: Cancel an interrupted edit
- **WHEN** 活动拖拽收到取消输入，或输入失效边沿使活动交互终止
- **THEN** 该事务恢复首个请求值并丢弃未提交记录，迟到的拖拽或释放事件无法再次应用旧编辑，既有撤销重做记录保持有效

#### Scenario: Complete an edit with no net change
- **WHEN** 用户结束拖拽时比例已回到开始时的值，或离散操作提交当前值
- **THEN** 历史条数和可重做分支保持原状

### Requirement: Replay completed edits with undo and redo

会话 SHALL 在有界历史中按完成顺序保存实际编辑及其原值和末值。Undo 和 Redo SHALL 通过原字段应用入口重放，并在该请求被 owner 接受后移动历史记录。新完成的有效编辑 SHALL 清除被分叉的 Redo 记录。输出准备结果和窗口原生设置 SHALL 各自管理其状态。

#### Scenario: Undo and redo an interleaved sequence
- **WHEN** 比例和主视口显隐分别完成有效编辑，用户连续执行 Undo 再执行 Redo
- **THEN** 两个 owner 按反序撤销和原序重做，每次仅修改对应字段，视口有效输出继续按当前比例请求的实际准备和接受结果发布

#### Scenario: Replace the redo branch with a new edit
- **WHEN** Undo 后存在可重做记录，用户完成另一项实际改变字段的编辑
- **THEN** 新编辑成为最新可撤销记录，原分支的 Redo 不再可用

### Requirement: Retire editor edits on session shutdown

结束会话 SHALL 停止产生和消费该会话的编辑意图，并丢弃未完成事务、未消费请求及会话历史。新会话 SHALL 建立独立的编辑状态。已经提交的图形工作继续由其原票据和依赖保活合同管理。

#### Scenario: Close a session during an unfinished edit
- **WHEN** 关闭意图或有界运行结束发生在事务未提交时，随后建立另一编辑器会话
- **THEN** 关闭后无旧会话编辑或原生请求被应用，重新接入后的 Undo 和 Redo 从空历史开始，旧输入不能命中新会话的动作
