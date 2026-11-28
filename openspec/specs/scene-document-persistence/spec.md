# scene-document-persistence Specification

## Purpose

规定静态场景和主 View 请求怎样由编辑器会话保存为单个可重开的项目文档，以及路径、版本、取消和失败时用户可以依赖的结果。已发布资产保持可由独立实例引用的只读输入。

## Requirements

### Requirement: Store one versioned scene document and stable references

编辑器 SHALL 使用 `Assets/Scenes` 下的 UTF-8 `*.scene.json` 文件，格式整数 `version` 初始为 1。v1 SHALL 保存稳定实体 ID、名称、可空父级、local TRS、本地可见性和已支持的网格、灯、相机组件。每个模型实例根 SHALL 保存模型路径、所选源 scene 和 `sourceVersion`。其源节点 SHALL 按当前实体记录保存 `instanceEntityId` 和源节点索引，覆盖 SHALL 按源 Primitive 槽保存已编辑的字段和纹理设置。二进制模型、共享材质正文及图像像素 SHALL 由资产提供。文档 SHALL 以记录的实体、层级和组件为准，恢复时保留用户的重设父级与删除结果。

#### Scenario: Reopen a scene after editing an imported hierarchy
- **WHEN** 同一模型有两个独立实例，用户重设其中一支的父级、删除一个源节点、修改灯值和材质／纹理覆盖后保存并重开
- **THEN** 稳定 ID、实例源定位、编辑的层级、被删节点的缺席、灯值和实例有效覆盖一致，固定模型记录继续提供网格及图像正文

#### Scenario: Start a new untitled scene
- **WHEN** 用户新建空文档，随后对该文档完成首次保存
- **THEN** 新建只产生无路径的空 Scene 和默认主 View 请求，所选文件在首次保存成功后才成为该文档的当前路径

### Requirement: Persist only the main View's supported requests

主 View SHALL 提供可持久化请求与对应 revision。v1 `mainView` SHALL 保存导航 `cameraPose`、可空 `cameraEntityId` 和 `requested.renderScale`。实体相机引用 SHALL 指向此文档中含相机组件的实体，为空时提供导航请求。持久化请求恢复后的有效输出 SHALL 继续遵循 View 的准备和发布合同。

v1 `mainView.requested.physicalCamera` SHALL 为可省略的物理请求对象。缺少对象时 SHALL 使用与新建主 View 相同的物理缺省值。对象存在时 SHALL 要求完整的有限数值字段 `fNumber`、`shutterSeconds`、`iso`、`exposureCompensationEv`、`sensorHeightMeters`、`navigationFocalLengthMeters` 和 `focusDistanceMeters`，且通过 View 的物理请求校验。保存 SHALL 写入已接受的完整请求。派生 FOV、EV100、预曝光和最后渲染值 SHALL 从其所属运行期状态获得。

v1 `mainView.requested.antiAliasing` SHALL 是可省略的字符串，支持 `none` 和 `fxaa`，缺省为新建主 View 的 None。保存 SHALL 捕获有效请求的值，允许省略缺省的 `none`。显式未知值、`null` 或类型不符 SHALL 定位到该字段并拒绝整个 Open 候选。显示 profile、实际 AA 和渲染及输出像素布局 SHALL 从当前准备与成功接受的帧获得。

v1 `mainView.requested.opaquePath` SHALL 是可省略的字符串，支持 `forward`、`forward-plus` 和 `deferred`，缺省为新建主 View 的 Forward。保存 SHALL 捕获已接受的路径请求，允许省略缺省的 `forward`。显式未知值、`null` 或类型不符 SHALL 定位到该字段并拒绝整个 Open 候选。打开成功时 SHALL 随完整文档恢复请求，实际有效路径由随后匹配的正常受光输出确认。

额外 View、窗口设置、布局、选择、运行期 handle、编辑历史及有效渲染输出 SHALL 保持为各自运行期状态。待选计划、准备诊断、有效路径和历史资格 SHALL 由运行期帧准备与接受合同管理。

#### Scenario: Round-trip a requested value that is still pending
- **WHEN** 主 View 已接收不同于当前有效比率的请求，并选择场景中的相机实体后保存并重开
- **THEN** 主 View 恢复保存的请求比率、导航位姿和可解析的相机实体引用，实际有效值继续按对应输出的接受结果发布

#### Scenario: Reject an invalid main View camera reference
- **WHEN** 文件的 `cameraEntityId` 指向缺失实体或没有相机组件的实体
- **THEN** 候选整体失败，当前活动 Scene、主 View 请求和路径保持有效

#### Scenario: Open a v1 document that predates physical camera settings

- **WHEN** 现有 v1 文件缺少 `physicalCamera`，而 `mainView` 的其余字段合法
- **THEN** 场景和主 View 作为完整候选恢复，保留原相机选择、导航位姿和 render scale，并采用新建主 View 的物理缺省值

#### Scenario: Restore the exact physical camera request after saving

- **WHEN** 主 View 的合法非默认镜头和曝光请求已生效，其新渲染结果尚未确认，用户保存后重开
- **THEN** 文档恢复捕获时的七个请求值及其单位语义，由准备合同重新计算派生值，并由新的匹配场景结果确认渲染状态

#### Scenario: Reject an explicitly invalid physical camera object

- **WHEN** v1 文件的 `physicalCamera` 是 `null`、字段缺失、带未知字段或其物理数值无法接受
- **THEN** 打开失败并给出文件与相关字段原因，原活动 Scene、主 View 请求、路径及编辑状态保持有效

#### Scenario: Restore FXAA from a v1 request before a frame is accepted

- **WHEN** 用户先打开省略 `antiAliasing` 的合法 v1 文件，再选择 FXAA，在新场景帧仍待确认时保存并重开
- **THEN** 首次打开采用 None，重开恢复捕获的 FXAA 请求；实际 AA 由重开后第一次匹配的正常场景结果确认

#### Scenario: Reject an unsupported AA field without publishing the document

- **WHEN** 待打开文件将主 View 的 `antiAliasing` 指定为 `null` 或未支持的值
- **THEN** 文件和该字段的原因可见，原 Scene、主 View 请求、活动路径和已接受输出继续有效

#### Scenario: Restore an opaque path request independently of the accepted path

- **WHEN** 用户打开缺少 `opaquePath` 的 v1 文档，请求 Forward+ 后在有效路径仍为 Forward 时保存并重开
- **THEN** 初次打开的请求为 Forward，重开后恢复保存的 Forward+ 请求并在有效路径尚未确认时显示真实状态。首次有效受光结果使用重开后的当前输入确认

#### Scenario: Reject an invalid v1 opaque path without publishing the document

- **WHEN** v1 文件的 `opaquePath` 是 `null`、非字符串或超出支持值域
- **THEN** Open 诊断包含对应文件及 `mainView.requested.opaquePath`，原活动场景、文档路径、主 View 请求和接受记录依照失败恢复合同继续有效

#### Scenario: Restore a deferred request with FXAA before its first accepted frame

- **WHEN** 主 View 的 Deferred 与 FXAA 编辑已接受，Deferred 渲染仍在等待或被当前设备配置拒绝，用户保存并重开文档
- **THEN** v1 文档恢复 `deferred` 和 `fxaa` 请求，设备准备状态和有效路径由当前主 View 在新场景下的输入及正常回执确认。文件只保有请求值

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

### Requirement: Persist one optional scene environment request

v1 SHALL 支持可省略的顶层 `environment` 对象。对象存在时 SHALL 保存受控的 `path`、从环境空间到世界空间的单位 xyzw `rotation`、有限非负 `intensity`、独立布尔用途 `lighting` 和 `sky`，其中至少一项用途为真。省略对象 SHALL 表示场景未配置环境。Open SHALL 从给定的项目根解析 `Assets/Environments` 下的普通文件，与当前 Open 操作核对实际源并固定有效 CPU 版本后才整体发布 Scene 与 View。解码失败或环境字段非法时 SHALL 沿现有文档错误合同报告字段或资产。Save SHALL 捕获 Scene 的环境请求和对应文档版本，保存请求字段。标定来自有效源，像素和有效 GPU 状态 SHALL 从拥有者获得。

#### Scenario: Reopen a v1 scene with or without an environment

- **WHEN** 先打开缺少 `environment` 的合法 v1 文件，再保存并重开具有合法非默认环境朝向、强度和单独天空用途的 v1 场景
- **THEN** 第一份文档恢复为未配置环境，后一份恢复所保存的环境请求和值，GPU 就绪状态按当前帧准备另行产生

#### Scenario: Reject an invalid environment without replacing the active document

- **WHEN** 待打开的 v1 文档提交越界环境路径、非法旋转或一个未能成为有效 CPU 版本的源
- **THEN** 打开报告对应的文件和字段或资产错误，先前活动 Scene、View 请求及文档路径仍有效

### Requirement: Persist sparse IOR and specular instance overrides

场景 v1 SHALL 按选定模型实例的源 Primitive 槽保存已覆写的 `ior`、`specularFactor` 和 `specularColorFactor`，以及已覆写的 `specular` 与 `specularColor` 纹理槽。纹理覆盖保留其项目图像引用、用途和已编辑的采样设置。省略项继承固定的源模型，旧 v1 的缺省表达仍可直接读取。打开 SHALL 在发布候选前验证新字段的类型、数值、源 IOR 兼容模式约束和实际纹理用途。写出 SHALL 由同一规则恢复有效覆盖和数据域。

#### Scenario: Reopen an old or extended v1 document through the same entry point
- **WHEN** 打开没有新增覆盖字段的旧 v1，以及保存后重新打开含合法 IOR/specular 覆盖和纹理设置的 v1
- **THEN** 前者保留源继承，后者恢复各实例的有效值、图像引用和采样语义。非法 IOR 或纹理用途的候选返回可定位错误，当前活动文档继续有效

### Requirement: Persist sparse clearcoat instance overrides

场景 v1 SHALL 按模型实例和源 Primitive 槽保存已覆写的 `clearcoatFactor`、`clearcoatRoughnessFactor`、`clearcoatNormalScale` 及 `clearcoat`、`clearcoatRoughness`、`clearcoatNormal` 纹理槽。已覆写纹理 SHALL 保留项目图像引用、与槽匹配的用途及独立采样设置。省略项继承固定源模型，显式零因子保留覆盖身份。写出和重新打开 SHALL 使用同一有效材质规则恢复基底和清漆的各自覆盖。

打开 SHALL 在发布文档候选前校验字段类型、数值、纹理用途、材质组合及目标几何的有效 UV 和切线条件。含清漆覆盖的 v1 与省略清漆字段的既有 v1 SHALL 走现有文档入口。

#### Scenario: Reopen existing and extended v1 documents with sparse clearcoat values
- **WHEN** 打开缺少清漆覆盖字段的旧 v1，并通过同一入口保存和重新打开带有显式零因子、清漆法线槽及采样编辑的 v1
- **THEN** 旧文档继续继承源材质，新文档恢复目标实例的显式值、图像用途、采样和基底覆盖

#### Scenario: Reject an incompatible clearcoat override before document publication
- **WHEN** 待打开的 v1 将颜色用途图像引用写入清漆法线槽，或其清漆贴图需要目标 Primitive 缺失的 UV 或切线空间
- **THEN** 打开交付对应字段或槽的原因，当前活动文档、路径和可用结果继续有效
### Requirement: Persist sparse sheen instance overrides

场景 v1 SHALL 按实例及固定源 Primitive 保存已覆写的 `sheenColorFactor`、`sheenRoughnessFactor` 和 `sheenColor`、`sheenRoughness` 纹理槽。已覆写槽 SHALL 保留有效项目图像引用、用途和分别编辑的采样设置。显式零 SHALL 保留覆盖身份，省略项 SHALL 继承源记录。打开 SHALL 在发布候选前核对类型、范围、材质兼容性、槽用途及可用 UV。

#### Scenario: Reopen an old or extended v1 document

- **WHEN** 用同一入口打开已有 v1，并将包含显式零颜色及两个独立 sheen 槽设置的 v1 保存后重开
- **THEN** 旧文档恢复既有的源继承，新文档恢复目标实例的稀疏覆盖和两槽各自的图像与采样语义

#### Scenario: Reject an incompatible sheen slot during open

- **WHEN** 打开一个 sheen 粗糙度槽引用不兼容用途纹理或需要缺席 UV 的 v1
- **THEN** 打开报告对应目标与原因，活动文档、已确认路径及可用结果保有资格

### Requirement: Persist sparse anisotropy instance overrides in scene v1

场景 v1 SHALL 按目标实例及固定源 Primitive 保存已覆写的 `anisotropyStrength`、`anisotropyRotation` 及 `anisotropy` 纹理槽。显式零保持覆盖身份，省略字段继承固定源值。旋转以弧度保存。已覆写槽 SHALL 保存兼容的项目图像用途和已有的稀疏采样设置。打开候选 SHALL 在整体发布前依同一有效材质合同核对字段、范围、用途、有效 UV、可用切线及颜色组合。

#### Scenario: Round-trip extended v1 beside existing v1 documents

- **WHEN** 打开既有 v1，再保存并重新打开有显式零强度、非零弧度旋转和方向纹理设置的 v1
- **THEN** 两者通过同一文档入口，旧文档按源继承，新文档恢复所记录实例的零覆盖、角度、纹理和采样选择

#### Scenario: Keep the active document when anisotropy cannot be restored

- **WHEN** 打开候选中的方向槽用途错误，或合成后的目标 Primitive 缺乏该覆盖所需的切线空间
- **THEN** 报告对应字段或目标原因，活动场景、已确认路径及可用输出继续有效
