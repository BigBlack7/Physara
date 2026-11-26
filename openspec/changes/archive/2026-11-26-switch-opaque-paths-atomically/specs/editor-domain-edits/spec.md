# Editor Domain Edits Delta

## ADDED Requirements

### Requirement: Edit and inspect the main View opaque strategy

View Settings SHALL 提供 Forward 和 Forward+ 两种主 View 路径请求。选择、Undo 和 Redo SHALL 通过同一当前文档、Scene 和 View 作用域的字段事务生效。每次接受有净变化的请求 SHALL 形成一条可重放编辑，并更新主 View 的持久化 revision。重复同值、目标失效、文档冻结或字段非法 SHALL 保持原请求和历史。渲染计划的确认或拒绝 SHALL 独立于该字段编辑。

View Settings SHALL 同时呈现主 View 的当前请求、有效路径及准备原因。有效路径 SHALL 来自当前 Scene 已接受的受光计划，首次确认前显示尚未确认。等待或拒绝时 SHALL 保留实际仍有效的路径及候选原因。若某种预览或诊断只渲染与路径无关的输出，界面 SHALL 表明路径请求应用于受光输出。

#### Scenario: Apply and replay an opaque path edit

- **WHEN** 用户从 View Settings 选择另一条不透明路径，然后 Undo 和 Redo
- **THEN** 各次操作通过当前文档的 View 请求生效，能读取实际请求与其最新准备状态。有效路径随匹配的受光帧确认，选择、重放和渲染确认使用各自的真实结果

#### Scenario: Keep the independently confirmed path visible during preparation

- **WHEN** 启用了普通受光预览，Forward+ 请求正在等待或被编译拒绝，Forward 仍能执行
- **THEN** 控件显示 Forward+ 请求和原因，标明实际有效路径仍为 Forward。候选被正常接受后，面板显示对应的 Forward+ 生效状态

#### Scenario: Respect frozen or replaced documents

- **WHEN** 用户尝试在 Open 冻结期间编辑或重放路径字段，或旧文档提交的意图在替换后才到达
- **THEN** 原意图无法改变当前请求和编辑历史，旧计划回执无法为另一 Scene 确认路径
