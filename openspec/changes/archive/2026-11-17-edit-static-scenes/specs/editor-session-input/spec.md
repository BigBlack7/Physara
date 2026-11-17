# Editor session input delta

## ADDED Requirements

### Requirement: Navigate the main View through its navigation camera

主 View SHALL 支持 Fly、Orbit 和 Pan。可停靠的 View 面板 SHALL 显示当前相机源、操作说明、requested render scale 和最后接受的输出设置及状态。右键按住进入 Fly，鼠标控制朝向，W/A/S/D 控制前后左右，Q/E 沿世界竖轴下降或上升。Alt 加主键按住进入 Orbit，围绕启动时确定的世界锚点调整相机位置及朝向。中键按住进入 Pan，沿相机平面平移并保持朝向。成功捕获后使用宿主观察到的逻辑运动量，持续键位移 SHALL 使用有界帧时间，操作 SHALL 产出有限且满足 View 合同的导航位姿。

导航开始 SHALL 使用当轮有资格的首个按下及其实际逻辑位置，要求该位置命中当前布局中匹配的有效图像。操作 SHALL 依据宿主已生效的相对光标状态开始消费相对运动。捕获期间允许指针越过图像边界，持续操作可等待其自身相机更新的图输出。释放启动按钮 SHALL 正常结束事务，同批中释放后的运动 SHALL 不再推进该位姿。Esc、输入失效、当前视口布局失去使用资格、当前捕获被更高优先级交互取代，或文档及相机目标失效 SHALL 终止捕获，清理输入并归还相对光标；未结束的位姿编辑遵循相同的事务取消合同。重开面板或恢复焦点后需要新的有效按下。

选择包含合法 Camera 的实体 SHALL 经 View 请求启用实体相机。绑定期间模型的相机投影和世界位姿由 Scene 提供，工作区 SHALL 标出当前来源并停用导航手势。切回导航相机 SHALL 使用该 View 已保存的导航位姿。

#### Scenario: Fly, orbit and pan without editing scene transforms
- **WHEN** 在当前有效导航图像内分别开始三种手势并释放，选中的实体具有可用的世界范围
- **THEN** Fly 随输入转向和移动，Orbit 在手势内围绕同一锚点并保持眼点到锚点的距离，Pan 保持朝向并沿相机平面移动；Scene 的创作字段及静态内容版本保持原值

#### Scenario: End an interrupted camera interaction
- **WHEN** 相对捕获期间视口隐藏或同一输入批次发生失焦与恢复，随后恢复有效布局
- **THEN** 旧手势取消、光标归还，迟到位移和 Release 不推进位姿，原 View 上后续的新 Press 从新基线开始

#### Scenario: Select a scene camera and return to the navigation pose
- **WHEN** 选择一个受父级缩放影响的固定宽高比场景相机，在其图像区尝试导航，再显式切回导航相机
- **THEN** 实体相机按已支持的投影和世界相机轴显示，尝试导航不改变实体或导航位姿，切回后使用原导航位姿准备相机及新图像

## MODIFIED Requirements

### Requirement: Dispatch each editor trigger to one eligible action owner

编辑器 SHALL 集中解释当前帧的文本、模态、活动 Gizmo、已捕获相机、视口命中及快捷键作用域。文本或模态消费其拥有的输入，活动 Gizmo 优先于已捕获相机，其余业务输入才进入有资格的视口或快捷键作用域。菜单和快捷键 SHALL 通过同一动作身份执行相同的启用规则。提供快捷键的场景文件、删除、Undo 和 Redo 动作 SHALL 使用这一输入归属及启用合同。对于一次性动作，Repeat SHALL NOT 被解释成新的首次按下。快捷键资格 SHALL 按输入顺序及本轮交互状态判断，以免同一按键既作用于文本、相机或其它控件，又触发文档动作。

#### Scenario: Share a panel action between menu and shortcut
- **WHEN** 可用的面板显隐动作分别由菜单和满足作用域的首次按键触发，按键之后还有原生 Repeat
- **THEN** 两种触发使用同一动作和启用条件，各自只应用一次显隐变更，Repeat 不产生额外变更

#### Scenario: Resolve competing input in the current frame
- **WHEN** 当前文本或模态交互、活动 Gizmo 或已捕获的相机与一个视口或快捷键动作竞争同一输入
- **THEN** 只有按当帧优先级有资格的归属消费该业务输入，一次滚轮输入不会分发给多个业务消费者，上一帧的捕获状态不会单独决定结果

#### Scenario: Route an undo or redo trigger through current input ownership
- **WHEN** Undo 或 Redo 由菜单或快捷键触发，且当前帧可能有文本编辑、活动拖拽或原生 Repeat
- **THEN** 有资格的单次触发应用一次可用历史动作，文本占用的按键和重复事件不改变会话历史，活动事务未结束时历史动作不可用

#### Scenario: Keep destructive shortcuts out of text and modal input
- **WHEN** 用户在路径或 Transform 文本框按 Delete，或在未保存确认期间按文件快捷键，之后在有资格的工作区对有效选择按 Delete
- **THEN** 前两种触发由原 UI 交互消费；最后一次按下只发出一次和菜单 Delete 相同的动作，原生 Repeat 不再删除其它实体
