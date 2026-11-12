# Editor session input delta

## MODIFIED Requirements

### Requirement: Own a dockable workspace and gate unavailable actions

编辑器会话 SHALL 管理主原生窗口内的停靠区、已注册面板的显隐及可空的类型化选择身份。面板 SHALL 可停靠、组合、隐藏和重新显示。面板通过只读领域数据和受限动作入口访问业务状态。主视口的菜单显隐和面板关闭 SHALL 产生同一会话字段的可撤销编辑。Undo 和 Redo SHALL 依据各自可用记录及活动事务状态启用。领域数据或有效选择缺失时，相关操作 SHALL 显示不可用且不产生领域修改。

#### Scenario: Dock and restore a panel in the same workspace
- **WHEN** 用户在主窗口组合面板，随后通过菜单隐藏并重新打开其中一个面板
- **THEN** 面板保持同一逻辑身份，隐藏时跳过其内容构建，重新打开时回到同一工作区，停靠操作只使用主原生窗口

#### Scenario: Open the workspace without domain data
- **WHEN** 会话没有可用场景、视口输出或有效选择
- **THEN** 相关面板明确显示空状态或准备状态，缺少数据的动作不可用，可用的布局操作仍能执行

#### Scenario: Reopen the viewport with undo
- **WHEN** 用户通过视口关闭按钮隐藏主视口，随后撤销该显隐编辑
- **THEN** 会话重新显示同一停靠面板，视口继续使用原 View 身份和逻辑纹理 token

### Requirement: Dispatch each editor trigger to one eligible action owner

编辑器 SHALL 集中解释当前帧的文本、模态、活动 Gizmo、已捕获相机、视口命中及快捷键作用域。文本或模态消费其拥有的输入，活动 Gizmo 优先于已捕获相机，其余业务输入才进入有资格的视口或快捷键作用域。菜单和快捷键 SHALL 通过同一动作身份执行相同的启用规则。Undo 和 Redo SHALL 使用这一输入归属及启用合同。对于一次性动作，Repeat SHALL NOT 被解释成新的首次按下。

#### Scenario: Share a panel action between menu and shortcut
- **WHEN** 可用的面板显隐动作分别由菜单和满足作用域的首次按键触发，按键之后还有原生 Repeat
- **THEN** 两种触发使用同一动作和启用条件，各自只应用一次显隐变更，Repeat 不产生额外变更

#### Scenario: Resolve competing input in the current frame
- **WHEN** 当前文本或模态交互、活动 Gizmo 或已捕获的相机与一个视口或快捷键动作竞争同一输入
- **THEN** 只有按当帧优先级有资格的归属消费该业务输入，一次滚轮输入不会分发给多个业务消费者，上一帧的捕获状态不会单独决定结果

#### Scenario: Route an undo or redo trigger through current input ownership
- **WHEN** Undo 或 Redo 由菜单或快捷键触发，且当前帧可能有文本编辑、活动拖拽或原生 Repeat
- **THEN** 有资格的单次触发应用一次可用历史动作，文本占用的按键和重复事件不改变会话历史，活动事务未结束时历史动作不可用
