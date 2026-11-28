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

### Requirement: Edit physical camera fields through the main View transaction

View Settings SHALL 提供主 View 的光圈 f 数、快门秒数、ISO、曝光补偿档数、传感器高度、导航焦距与对焦距离输入。Inspector SHALL 提供主 View 的光圈、快门、ISO 和曝光补偿，展示当前 EV100、请求预曝光及最后正常接受的场景曝光。两处 SHALL 读取同一主 View 权威请求并通过同一字段事务和历史编辑，面板切换不得应用退役文档的输入草稿。传感器高度和焦距 SHALL 以毫米显示和编辑，对焦距离 SHALL 以米显示和编辑。场景透视 Camera SHALL 可读回源垂直 FOV 与按传感器推得的有效焦距，导航焦距输入只在导航相机生效。正交 Camera SHALL 展示其投影类型并继续允许 View 曝光设置。

编辑器 SHALL 在已有编辑边界向当前 Scene/View 所属的 View 事务提交相应字段。成功的有净变化提交 SHALL 形成一条可撤销重做的记录。冻结、目标过期或非法输入 SHALL 通过现有诊断报告原因，保留此前接受的请求和历史。

#### Scenario: Edit, undo and redo physical camera controls

- **WHEN** 用户在主 View 中提交新的镜头或曝光值，随后执行 Undo 和 Redo
- **THEN** View 请求和对应的派生值随每次成功重放恢复，保存状态记录主 View 的请求变化，活动 Scene 的创作参数和其他 View 各自保持原值

#### Scenario: Keep accepted controls after a rejected physical camera edit

- **WHEN** 用户提交零快门，或打开场景文件期间尝试提交物理相机文档编辑
- **THEN** 会话报告拒绝原因，已接受参数、最后成功渲染值和 Undo/Redo 记录保持有效

#### Scenario: Share exposure across both main View entry points

- **WHEN** 用户在 Inspector 提交 +1 EV，再通过现有 View Settings 查看并撤销该编辑
- **THEN** 两处展示同一请求及历史结果，最后正常接受的曝光独立确认；选择其它实体不改变曝光请求或 glTF 相机参数

### Requirement: Edit surface fields on the selected model instance

Inspector SHALL 按选中实体的固定源版本列出可编辑的源 Primitive 槽，并区分继承值与实例覆盖。编辑 SHALL 支持线性 base color RGBA、metallic、roughness、normal scale、occlusion strength、线性 emissive 和 emissive strength，以及当前表面 profile 的 alpha 和 double-sided 字段。对于使用 metallic-roughness 的 Primitive，Inspector SHALL 提供 IOR、specular 强度和线性 RGB 颜色，以及 specular 强度与颜色槽。每个被支持的纹理槽 SHALL 可选用 `Assets/Textures` 下的项目图像并编辑适用的 sampler、已有 UV 集和 UV 变换，色彩和通道解释 SHALL 由槽用途决定。Inspector SHALL 允许清除单个材质字段或纹理槽的覆盖并显示继承后的有效值。已导入材质的预览限制 SHALL 与可创作字段分开显示。

离散提交、清除、Undo 和 Redo SHALL 通过同一 Scene 编辑边界更新目标实例的稀疏覆盖。请求 SHALL 绑定文档、实体代、固定模型版本及源 Primitive 和字段或纹理槽。其它字段、其它实例与共享 glTF/GLB SHALL 保持原值。字段或采样组合无效时 SHALL 报告被拒绝的目标及原因。

从源 glTF 继承 IOR=0 的材质 SHALL 显示兼容模式，其源 IOR 字段为只读，specular 强度、颜色和纹理可按标准编辑。普通实例 IOR 覆盖 SHALL 使用有限可表示的 `ior >= 1`。声明或编辑扩展字段之后，界面 SHALL 可辨认继承或覆盖、当前准备状态及最后正常接受的着色结果。改变有效材质的编辑 SHALL 沿静态内容版本合同使相关 View 的不兼容历史失效。

Inspector SHALL 为当前 lit metallic-roughness 实例提供 clearcoat 强度、感知粗糙度和独立清漆 normal scale，以及强度、粗糙度、清漆法线三个槽的继承值和稀疏编辑。提交、清除和恢复 SHALL 使用同一个有效材质校验，按目标 Primitive 检查槽用途、UV、切线空间、因子与材质组合。清漆与基底法线的字段和纹理 SHALL 可独立编辑。已声明或覆盖的清漆 SHALL 在 Inspector 中显示对应实际准备状态及最后正常接受的颜色身份。

Inspector SHALL 对兼容的 lit metallic-roughness 实例提供 sheen 线性 RGB、感知粗糙度、颜色槽和粗糙度槽的继承或稀疏编辑。有效零颜色、继承值和已有独立槽 SHALL 可辨认。提交、清除与恢复 SHALL 经同一材质校验。面板 SHALL 根据当前请求、实际准备和最后接受的正常受光帧显示 sheen 状态。

#### Scenario: Edit and reset one inherited surface field
- **WHEN** 同一模型被实例化两次，用户在其中一个 Primitive 上写入 roughness 覆盖、清除该字段，然后撤销清除
- **THEN** 该实例后续被接受的帧先使用覆盖，再使用源 roughness，最后恢复覆盖；该 Primitive 的其它覆盖、另一实例及共享源保持原值

#### Scenario: Bind a valid texture to its slot and reject an incompatible edit
- **WHEN** 用户将项目纹理用于支持该采样组合的 base color 槽，然后为该槽提交目标 Primitive 缺失的 UV 集
- **THEN** 成功项以 sRGB 颜色语义采样，拒绝项指出有效 UV 的限制，已接受纹理、Scene 版本和历史保持有效。线性数据槽沿自身的用途解释图像

#### Scenario: Edit, clear and undo one instance of an extended material
- **WHEN** 两个实例共用同一固定源，用户修改一个实例的 specular 颜色和图像槽，清除覆盖，再撤销清除
- **THEN** 该实例在随后各次正常接受的帧使用当前有效参数与纹理解释，各次有效内容变化按 Scene 合同失效相关历史，另一实例和源模型仍按各自数据渲染

#### Scenario: Inspect compatibility IOR and reject an invalid override
- **WHEN** 一个源材质使用 IOR=0，随后通过实例编辑尝试改变其 IOR，或者在普通实例上提交显式零 IOR
- **THEN** 兼容材质显示源模式，越过规则的请求交付定位到字段的原因，现有实例覆盖和有效渲染版本保有原来的资格

#### Scenario: Apply and clear clearcoat on one instance with an existing base profile
- **WHEN** 两个实例共享固定的无清漆源，用户在一个实例上设置清漆因子和贴图，清除覆盖，随后 Undo 和 Redo
- **THEN** 该实例的颜色和路由跟随清漆或继承的基底，各次有效内容变更使相关 View 的不兼容历史失效，另一个实例和共享源按自己的材质渲染

#### Scenario: Retain the current clearcoat edit when the target cannot use its normal slot
- **WHEN** 用户为现有 lit 实例提交使用不可用 UV 或无法取得切线框架的清漆法线覆盖
- **THEN** 面板定位被拒绝的槽和原因，原来接受的稀疏覆盖、场景修订、历史及可读帧保有资格

#### Scenario: Edit and restore sheen for only one instance

- **WHEN** 两实例固定同一源模型，用户为一实例编辑 sheen 颜色与粗糙度槽、提交显式零、清除单项覆盖并撤销或重做
- **THEN** 后续正常受光帧和面板对应目标实例的当前有效数据及色角色，相关内容变化使该 View 的不兼容历史失效。另一实例继承其源数据，诊断或准备中的预览保有上一份已接受受光身份

#### Scenario: Keep the last compatible state after an invalid sheen edit

- **WHEN** 用户提交与目标 sheen 槽用途不兼容的图像或该 Primitive 缺席的有效 UV
- **THEN** 诊断定位至该槽和原因，以前接受的实例覆盖、场景版本、历史和可读结果保有资格

### Requirement: Edit punctual light parameters with explicit units

Inspector SHALL 依据现有灯的类型展示并编辑线性颜色和强度，Directional 的强度标记为 lux，Point 和 Spot 标记为 candela。局部灯 SHALL 支持显式有限正 range 或继承无截断语义，Spot SHALL 展示内外锥角并明确角度单位。灯的创作值 SHALL 由 Scene 校验并通过现有事务更新。有效灯 SHALL 通过后续帧的灯列表影响像素，照明求值继续遵循该灯类型的物理单位和曝光合同。

#### Scenario: Edit and replay a localized light
- **WHEN** 用户为一个已有 Spot 设置有效强度、有限范围和锥角，随后清除 range 并重放该变更
- **THEN** 各次有效提交进入同一历史并更新 Scene，后续正常帧使用实际接受的 candela、无截断或对应的有限范围和内外锥角

#### Scenario: Reject invalid photometric parameters
- **WHEN** 提交非有限强度或非法 Spot 锥角关系
- **THEN** Inspector 报告字段及约束，保留原灯、版本、历史和现有有效照明

### Requirement: Edit the current scene environment from the Inspector

Inspector SHALL 提供 Scene 环境的设置和移除入口。用户 SHALL 能选择 `Assets/Environments` 下的受支持 HDR 源，编辑单位旋转、有限非负倍率，以及独立的照明和天空用途。配置存在时至少 SHALL 启用一个用途，无环境由移除表达。路径有效且产生当前请求所需的 CPU 版本后，编辑 SHALL 通过同一 Scene 边界提交，并与数值或用途编辑使用同一撤销历史。面板 SHALL 显示源的相对或绝对标定及实际元数据，旋转、倍率与 View 曝光按各自合同消费。

#### Scenario: Apply and undo an environment settings edit
- **WHEN** 已接受环境 A，用户保留源并改变环境朝向、倍率或用途，再 Undo 与 Redo
- **THEN** Scene 通过相应字段事务恢复和重放请求；匹配的正常帧显示实际接受的环境状态，查询空间修改复用匹配的同源预计算资源

#### Scenario: Remove a configured environment
- **WHEN** 用户明确移除环境并再次获得正常 Physical Lighting 帧
- **THEN** Scene 和随后保存的 v1 文档均无环境请求，当前帧沿已有无环境合同显示核心直接光及自发光，Undo 可恢复原环境请求

### Requirement: Report source support and distinguish requested resources from accepted rendering

编辑器 SHALL 显示当前选中实例固定模型版本的导入支持报告，覆盖被忽略的 optional 扩展及动画初始状态等实际降级。失败的导入和资源编辑 SHALL 提供可定位的资源或字段原因。纹理或环境源候选等待时 SHALL 显示 Pending；只有仍匹配当前文档、原目标及请求代的成功候选 SHALL 提交创作值。拒绝、取消、过期或被替代的候选 SHALL 保留此前接受的字段与历史。

CPU 资产可用、Scene 或 View 已接受请求、正常帧最后接受的实际输出 SHALL 在界面可辨认。环境新请求尚未完成 GPU 准备时 SHALL 沿既有环境和视口合同保留可用旧结果并显示原因。打开场景时的文档冻结 SHALL 涵盖候选编辑和历史重放；成功切换文档后，旧候选和草稿 SHALL 失去发布资格。

#### Scenario: Show support for the pinned model after opening a scene
- **WHEN** 用户打开已有场景并选择一个模型实例，该版本保留核心回退并带有导入降级说明
- **THEN** Inspector 显示该选中版本的实际说明，已有预览仍按当前渲染 profile 报告自身的限制

#### Scenario: Leave accepted state intact after a failed or stale load
- **WHEN** 用户申请一个项目资源替换，解码失败，或另一次替换在切换文档、目标退役或被新请求替代后才完成
- **THEN** 可用时先显示对应 Pending 或失败原因，迟到结果不更新活动文档、未保存状态或历史，先前已接受的资源保持有效

#### Scenario: Distinguish a pending environment request from the last drawn environment
- **WHEN** CPU 已发布的新环境成为 Scene 请求，主 View 仍在准备该环境的实际图资源
- **THEN** 面板分别显示新请求、准备状态及最后已经正常绘制的环境，只有匹配的正常帧才能确认新环境为当前输出

### Requirement: Edit and inspect the main View opaque strategy

View Settings SHALL 提供 Forward、Forward+ 和 Deferred 三种主 View 路径请求。选择、Undo 和 Redo SHALL 通过同一当前文档、Scene 和 View 作用域的字段事务生效。每次接受有净变化的请求 SHALL 形成一条可重放编辑，并更新主 View 的持久化 revision。重复同值、目标失效、文档冻结或字段非法 SHALL 保持原请求和历史。渲染计划的确认或拒绝 SHALL 独立于该字段编辑。

View Settings SHALL 同时呈现主 View 的当前请求、有效路径及准备原因。有效路径 SHALL 来自当前 Scene 已接受的受光计划，首次确认前显示尚未确认。等待或拒绝时 SHALL 保留实际仍有效的路径及当前候选原因，按实际情况显示准备等待、角色失败或不支持的路径配置。合法请求和实际显示配置 SHALL 可分别读取。若某种预览或诊断只渲染与路径无关的输出，界面 SHALL 表明路径请求应用于受光输出。

#### Scenario: Apply and replay an opaque path edit

- **WHEN** 用户从 View Settings 选择另一条不透明路径，然后 Undo 和 Redo
- **THEN** 各次操作通过当前文档的 View 请求生效，能读取实际请求与其最新准备状态。有效路径随匹配的受光帧确认，选择、重放和渲染确认使用各自的真实结果

#### Scenario: Keep the independently confirmed path visible during preparation

- **WHEN** 启用了普通受光预览，Forward+ 请求正在等待或被编译拒绝，Forward 仍能执行
- **THEN** 控件显示 Forward+ 请求和原因，标明实际有效路径仍为 Forward。候选被正常接受后，面板显示对应的 Forward+ 生效状态

#### Scenario: Respect frozen or replaced documents

- **WHEN** 用户尝试在 Open 冻结期间编辑或重放路径字段，或旧文档提交的意图在替换后才到达
- **THEN** 原意图无法改变当前请求和编辑历史，旧计划回执无法为另一 Scene 确认路径

#### Scenario: Keep a deferred request visible without confirming an unsupported configuration

- **WHEN** 主 View 编辑为 Deferred，其当前附件配置被拒绝，随后查看诊断输出并返回可正常受光的布局
- **THEN** 面板保留 Deferred 请求并显示适用范围和对应原因。已确认路径仍来自最近的正常受光提交。恢复后面板只按实际接受的 Deferred 帧更新有效路径和画质

### Requirement: Author and inspect anisotropy on a compatible static instance

Inspector SHALL 为兼容 lit OPAQUE/MASK Primitive 提供 [0,1] 强度、弧度旋转和线性 RGB 方向纹理槽的继承值与稀疏编辑。纹理槽 SHALL 复用项目图像、sampler、有效 UV 和 UV 变换编辑。显式零强度 SHALL 可区别于继承，清除单项 SHALL 恢复对应源值。正常提交、清除、撤销及重做 SHALL 通过同一有效材质边界校验，包括基底 normal 有效 UV 或框架来源发生改变的情况。超出支持的材质组合或无法交付所需切线空间的请求 SHALL 明确拒绝，原实例状态、历史和可用帧保有资格。

Inspector SHALL 区分固定源的支持或降级说明、当前请求的有效因子、颜色等待或拒绝，以及该 View 对应实例和 Primitive 最后一次实际接受的各向异性受光。仅有表面或诊断输出时 SHALL 保留最后有效颜色身份。有效内容改变 SHALL 沿 Scene 版本合同失效不兼容的 View 历史。

#### Scenario: Set explicit zero, restore inheritance and replay the instance edit

- **WHEN** 两个实例固定同一模型，用户为一个目标改变旋转和方向槽，提交显式零强度，再清除强度覆盖、撤销及重做
- **THEN** 目标实例的继承/覆盖和后来接受的颜色与当前有效材质相符，相关有效内容变化失效不兼容历史。另一实例和源记录继续按各自的固定值使用

#### Scenario: Reject incompatible frame edits and report actual color delivery

- **WHEN** 已正常交付各向异性颜色的实例申请不可用的基底 normal UV 或混层编辑，随后 View 交付表面诊断或等待颜色资源
- **THEN** 非法编辑显示目标及原因并保有有效 Scene 状态。诊断或等待状态与最后正常接受的实际受光身份均可辨认
