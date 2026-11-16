# Scene Document Persistence Spec Delta

## Purpose

规定静态场景和主 View 请求怎样由编辑器会话保存为单个可重开的项目文档，以及路径、版本、取消和失败时用户可以依赖的结果。已发布资产保持可由独立实例引用的只读输入。

## ADDED Requirements

### Requirement: Store one versioned scene document and stable references

编辑器 SHALL 使用 `Assets/Scenes` 下的 UTF-8 `*.scene.json` 文件，格式整数 `version` 初始为 1。v1 SHALL 保存稳定实体 ID、名称、可空父级、local TRS、本地可见性和已支持的网格、灯、相机组件。每个模型实例根 SHALL 保存模型路径、所选源 scene 和 `sourceVersion`。其源节点 SHALL 按当前实体记录保存 `instanceEntityId` 和源节点索引，覆盖 SHALL 按源 Primitive 槽保存已编辑的字段和纹理设置。二进制模型、共享材质正文及图像像素 SHALL 由资产提供。文档 SHALL 以记录的实体、层级和组件为准，恢复时保留用户的重设父级与删除结果。

#### Scenario: Reopen a scene after editing an imported hierarchy
- **WHEN** 同一模型有两个独立实例，用户重设其中一支的父级、删除一个源节点、修改灯值和材质／纹理覆盖后保存并重开
- **THEN** 稳定 ID、实例源定位、编辑的层级、被删节点的缺席、灯值和实例有效覆盖一致，固定模型记录继续提供网格及图像正文

#### Scenario: Start a new untitled scene
- **WHEN** 用户新建空文档，随后对该文档完成首次保存
- **THEN** 新建只产生无路径的空 Scene 和默认主 View 请求，所选文件在首次保存成功后才成为该文档的当前路径

### Requirement: Persist only the main View's supported requests

主 View SHALL 提供可持久化请求与对应 revision。v1 `mainView` SHALL 保存导航 `cameraPose`、可空 `cameraEntityId` 和 `requested.renderScale`。实体相机引用 SHALL 指向此文档中含相机组件的实体，为空时提供导航请求。持久化请求恢复后的有效输出 SHALL 继续遵循 View 的准备和发布合同。额外 View、窗口设置、布局、选择、运行期 handle、编辑历史及有效渲染输出 SHALL 保持为各自运行期状态。

#### Scenario: Round-trip a requested value that is still pending
- **WHEN** 主 View 已接收不同于当前有效比率的请求，并选择场景中的相机实体后保存并重开
- **THEN** 主 View 恢复保存的请求比率、导航位姿和可解析的相机实体引用，实际有效值继续按对应输出的接受结果发布

#### Scenario: Reject an invalid main View camera reference
- **WHEN** 文件的 `cameraEntityId` 指向缺失实体或没有相机组件的实体
- **THEN** 候选整体失败，当前活动 Scene、主 View 请求和路径保持有效

### Requirement: Validate the document format before restoration

打开 SHALL 在有界输入和结构预算内验证 JSON 及 v1 字段，定位错误到文件及相应字段、实体或资产。未知版本、未知字段或组件、重复对象键或实体 ID、非法数值、无效 TRS、循环及悬空引用 SHALL 被拒绝。数值 SHALL 在字段表示范围内且有限，透视相机的 `far` 可用 `null` 表示无限。其他 `null` SHALL 仅由各字段声明的可空类型解释，缺省只用于 v1 明确允许省略的字段。编辑器 SHALL 写出可由相同 v1 规则读取的记录。

#### Scenario: Reject malformed, ambiguous or unsupported fields
- **WHEN** 文档使用未支持版本、重复 ID、循环 parent、越界数值或当前 schema 未声明的组件或参数
- **THEN** 返回对应的格式或引用原因，文档不按缺省值或局部成功结果替换活动状态

#### Scenario: Distinguish optional values from null
- **WHEN** 合法透视参数保存无限远面，某材质字段省略以继承源值，另一份输入将该非可空材质字段显式设为 null
- **THEN** 前者重开仍为无限远和源值继承，后一份文档因材质字段无效被拒绝

### Requirement: Resolve scene and asset references from the project root

场景文件 SHALL 受控于项目根的 `Assets/Scenes`。模型引用 SHALL 限于 `Assets/Models`，外置实例纹理 SHALL 限于 `Assets/Textures`。文件操作 SHALL 拒绝绝对路径、越出允许目录的相对路径及实际文件位置越界。资产引用 SHALL 从 Runtime 提供的项目资源根解析。Open SHALL 取得本次操作可验证的必要资产，对每个模型核对所选 scene、保存的源身份和源节点／Primitive 定位。Save 和 Save As SHALL 在写出前验证快照中固定模型及实际依赖仍对应当前源。源冲突 SHALL 指出资产，保留活动实例版本和已保存文件。

#### Scenario: Resolve the same assets from another scene directory
- **WHEN** 场景文件在 `Assets/Scenes` 的子目录被打开或另存，进程工作目录不在项目根
- **THEN** 同一资源引用仍解析到项目根中的模型和纹理，通过绝对或越界路径提交的读写被拒绝

#### Scenario: Detect a model change even on a warm cache
- **WHEN** 活动文档及已有文件引用仍在内存中的模型版本，但磁盘只改动一个实际参与导入的外部 buffer 或图像
- **THEN** 打开报告保存的源版本与当前资产冲突，Save 或 Save As 拒绝写出该旧版本的引用，已固定的实例继续可读

#### Scenario: Open a matching document after the asset cache goes stale
- **WHEN** 已有文件引用磁盘上实际有效的模型版本，而缓存保留该路径和所选 scene 的旧代
- **THEN** Open 取得与磁盘及文件一致的模型候选，按已保存定位恢复，缓存中的旧版本无法决定打开结果

### Requirement: Serialize document operations and publish a complete open

EditorSession SHALL 同时只接受一个新建、打开、保存或另存操作。替换有未保存修改的活动文档前 SHALL 按当前文档状态处理保存、显式放弃或取消。打开从接受时起 SHALL 冻结 Scene、主 View 可持久化请求及相应历史重放，提供进度和取消，继续处理 UI 和非文档动作。只有完整候选通过校验且操作仍匹配活动会话时，主线程 SHALL 在 CPU 帧边界一起替换 Scene 和主 View 请求、更新路径及文档身份并退役旧文档选择和历史。失败或取消 SHALL 恢复编辑，保留原 Scene、路径、主 View 请求和原文档的编辑状态。

#### Scenario: Keep the editor responsive while a document is opening
- **WHEN** 已处理未保存状态的 Open 在等待读取或资产就绪，同期收到文档编辑、第二个文件操作和非文档 UI 动作
- **THEN** 编辑与第二个文件操作被门控，UI 及取消仍可用，只有匹配的完整结果能在边界一起更新 Scene、View 与路径

#### Scenario: Cancel and discard a late open result
- **WHEN** Open 在资产准备或候选校验时被取消，之后后台结果或共享资产完成仍到达
- **THEN** 原活动状态继续可编辑，旧操作无法发布 Scene 或 View，独立资产消费者的已准入请求按原合同继续

#### Scenario: Resolve unsaved changes before opening
- **WHEN** 活动文档有尚未保存的修改，调用者请求 New 或 Open 然后取消替换或明确放弃这些修改
- **THEN** 取消时继续使用当前文档，明确放弃只授权尝试替换，完整候选失败时原文档及其未保存修改仍可编辑

### Requirement: Save an immutable revision with safe file replacement

保存 SHALL 在编辑边界捕获独立快照，包含文档身份、Scene 文档 revision、实体及资产引用和主 View 可持久化配置 revision。编码和写入使用该快照，期间 SHALL 允许后续编辑。写入 SHALL 使用受控目标目录中的独占临时文件，完成编码、写入和关闭后安全替换目标。编码、路径、写入、关闭或提交失败，以及提交前被接受的取消，SHALL 保留原目标文件。文件提交与取消 SHALL 有确定的胜负边界，已进入提交的操作 SHALL 报告实际结果。

#### Scenario: Keep the previous file when save cannot commit
- **WHEN** 目标已存在，写入或安全替换失败，或者保存于进入提交前被取消
- **THEN** 原文件仍可读取且字节保持，未提交的临时内容被收拢，路径与已保存 revision 保持原状态

#### Scenario: Resolve cancellation at the file commit boundary
- **WHEN** 取消请求与文件提交交错，或在进入提交后才到达
- **THEN** 接受的取消使本次操作无目标文件修改，已进入提交的操作返回不可再取消或实际写入结果

### Requirement: Acknowledge only the saved document and operation

EditorSession SHALL 只在当前有效的成功结果到达时登记快照的已保存 Scene 和主 View revision。任一持久化 owner 在捕获后有更新时，现文档 SHALL 继续显示未保存。Save As SHALL 仅在该操作成功后更新活动文档路径。会话关闭、操作失效或已有更新操作时，迟到结果 SHALL 无法变更活动路径、已保存 revision 或 Scene/View。

#### Scenario: Continue editing during a successful Save As
- **WHEN** Save As 捕获 Scene 和主 View 的快照，写入期间又修改实体或 View 请求，随后保存成功
- **THEN** 新路径归当前文档，磁盘只包含捕获的参数，更新后的文档继续显示未保存直至后续版本成功保存

#### Scenario: Leave the current path intact on a rejected Save As completion
- **WHEN** 另存失败、提交前被取消或其结果于会话关闭后才交回
- **THEN** 本次结果无法更新活动路径或清除未保存状态，关闭路径按实际文件提交状态收拢工作
