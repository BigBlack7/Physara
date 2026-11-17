# Editor Domain Edits Spec Delta

## ADDED Requirements

### Requirement: Edit physical camera fields through the main View transaction

View Settings SHALL 提供主 View 的光圈 f 数、快门秒数、ISO、曝光补偿档数、传感器高度、导航焦距与对焦距离输入。传感器高度和焦距 SHALL 以毫米显示和编辑，对焦距离 SHALL 以米显示和编辑。场景透视 Camera SHALL 可读回源垂直 FOV 与按传感器推得的有效焦距，导航焦距输入只在导航相机生效。正交 Camera SHALL 展示其投影类型并继续允许 View 曝光设置。

编辑器 SHALL 在已有编辑边界向当前 Scene/View 所属的 View 事务提交相应字段。成功的有净变化提交 SHALL 形成一条可撤销重做的记录。冻结、目标过期或非法输入 SHALL 通过现有诊断报告原因，保留此前接受的请求和历史。

#### Scenario: Edit, undo and redo physical camera controls

- **WHEN** 用户在主 View 中提交新的镜头或曝光值，随后执行 Undo 和 Redo
- **THEN** View 请求和对应的派生值随每次成功重放恢复，保存状态记录主 View 的请求变化，活动 Scene 的创作参数和其他 View 各自保持原值

#### Scenario: Keep accepted controls after a rejected physical camera edit

- **WHEN** 用户提交零快门，或打开场景文件期间尝试提交物理相机文档编辑
- **THEN** 会话报告拒绝原因，已接受参数、最后成功渲染值和 Undo/Redo 记录保持有效
