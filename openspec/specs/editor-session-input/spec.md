# editor-session-input Specification

## Purpose

定义编辑器会话在主窗口中接收输入、管理停靠面板并生成 CPU UI 帧的合同。界定原生宿主、编辑器动作与后续绘制消费者各自可依赖的行为及数据寿命。

## Requirements

### Requirement: Attach the editor to the live native host

编辑器 SHALL 在现有主线程宿主就绪后接入输入，并维持宿主原有的回调关联和窗口所有权。链上受支持的每次原生输入 SHALL 分别送达平台事件发布与 UI 输入消费一次。编辑器 SHALL 在宿主释放窗口前恢复原回调，启动中途失败时也须解除已经完成的接入。

#### Scenario: Deliver one input to both consumers
- **WHEN** 已接入的主窗口在正常活动期间收到一个按键边沿及其独立字符事件
- **THEN** 平台发布保留二者的实际顺序，UI 分别收到各一次，宿主的原生用户数据关联保持有效

#### Scenario: Detach and attach another session
- **WHEN** 编辑器拆除输入适配后宿主仍存活，随后在同一宿主上启动新的编辑器会话
- **THEN** 拆除后平台可以独立继续发布输入，新会话可重新接入且输入只到达当前会话，宿主仍可按原合同退出

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

### Requirement: Reset editor interaction when input becomes invalid

编辑器 SHALL 在失焦、窗口暂停或当前捕获失效时取消相关交互。批次内发生失焦或进入暂停时，该批次中会导致旧交互继续的键鼠输入和运动量 SHALL 被丢弃，即使批次结束时窗口已经恢复。恢复处理 SHALL 清除停用期间的待处理交互输入，仅由新有效输入发起捕获。相对光标模式的实际状态 SHALL 以宿主的有效观察为准。

#### Scenario: Lose and regain focus in one publication
- **WHEN** 同一输入批次包含失焦、旧捕获的输入及重新获得焦点
- **THEN** 编辑器终止旧捕获，跳过该批次的旧拖拽和快捷键，后续无新触发时保持释放，首份有效位移以新基线开始

#### Scenario: Restore after accumulating input while suspended
- **WHEN** 窗口最小化或像素范围为零，期间仍有回调，之后恢复可渲染
- **THEN** 恢复后的 UI 帧不重放暂停期间的键鼠或字符，业务输入从新批次继续，捕获需要新的有效触发

#### Scenario: Restore a stationary pointer after deferred cursor release
- **WHEN** 同批进入零像素并恢复为可渲染且聚焦，宿主在会话推进后确认将相对模式归还为普通模式
- **THEN** 即使没有新的移动或进入通知，首个有效按钮输入也按归还后当前的逻辑位置命中 UI，跨过失效边沿的发布不触发旧交互

### Requirement: Publish a borrowed CPU UI frame

在继续运行的可渲染轮次，编辑器 SHALL 基于当前输入和布局封存一个 CPU UI 帧。布局和命中 SHALL 使用逻辑坐标，像素裁剪所需比例 SHALL 来自有效的 framebuffer 和逻辑尺寸，内容缩放保持独立。输入事件只在当前发布借用内消费。UI 帧数据只在当前会话的下一次 UI 帧开始或会话退出前有效，跨该边界的消费者 SHALL 先取得所需数据的独立所有权。暂停轮次 SHALL 明确表示没有新 UI 帧。

#### Scenario: Publish a frame using current window metrics
- **WHEN** 可渲染窗口的逻辑尺寸、像素尺寸或内容缩放发生变化，运行继续生成下一份 UI 数据
- **THEN** 面板在当前逻辑空间生成绘制数据，像素比例与本次观察对应，消费者可在本帧读取 `DrawData`

#### Scenario: Consume consecutive editor frames
- **WHEN** 一份 UI 帧已封存并开始下一份 UI 帧，或会话在此之前结束
- **THEN** 上一份借用到期，后续消费只使用新的数据或自己持有的副本

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
