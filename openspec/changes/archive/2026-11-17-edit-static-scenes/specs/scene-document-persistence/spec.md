# Scene document persistence delta

## ADDED Requirements

### Requirement: Operate scene documents from the product workspace

正式工作区 SHALL 提供 New、Open、Save 和 Save As。当活动文档尚无路径时，Save SHALL 要求输入 Save As 目标；已有路径时，Save SHALL 使用已确认的当前路径。Open 和 Save As SHALL 接收 UTF-8 项目相对的 `Assets/Scenes/*.scene.json` 路径，使用现有文档边界校验。工作区 SHALL 显示当前路径或未命名状态、未保存状态、实际文件操作进度、可用的取消动作和拒绝原因。保存中 SHALL 继续允许场景及主 View 编辑；打开期间 SHALL 遵循文档冻结及候选发布合同。

替换有未保存修改的文档时，工作区 SHALL 提供保存后继续、明确放弃或取消。保存后继续 SHALL 只在匹配的保存成功且该快照仍是活动文档的当前持久化版本时提交原 New/Open 请求；失败、取消、文档身份失配或保存期间有更新时 SHALL 保留当前文档，提示用户再次处理。放弃 SHALL 仅授权尝试替换已确认身份及版本的文档。首次保存或另存的路径 SHALL 在实际成功后显示为活跃路径。

#### Scenario: Save and reopen from the product workspace
- **WHEN** 用户从工作区给未命名文档首次保存，修改后使用 Save，再通过 New 和 Open 打开它
- **THEN** 文件保存与实际路径由完成结果确认，重开还原稳定实体、层级、本地 TRS、主 View 导航位姿、当前可选相机实体和请求渲染比例，选中状态和旧文档历史已退役

#### Scenario: Keep edits made during a save before replacing a scene
- **WHEN** 用户从未保存提示选择保存后继续，在这次保存处理期间又提交一项场景或主 View 修改
- **THEN** 成功结果只确认所捕获的版本与对应保存路径，文档仍为未保存且原 New/Open 不继续；可对新版本再次执行替换操作

#### Scenario: Preserve the active document when a UI file action fails or is cancelled
- **WHEN** 用户取消未保存提示，或在放弃当前修改后尝试打开一个无法通过校验的文档
- **THEN** 取消不发起替换；打开失败显示原因并恢复编辑，已有 Scene、当前路径、保存基线和主 View 请求仍有效
