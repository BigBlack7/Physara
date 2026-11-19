# editor-domain-edits Delta

## ADDED Requirements

### Requirement: Edit surface fields on the selected model instance

Inspector SHALL 按选中实体的固定源版本列出可编辑的源 Primitive 槽，并区分继承值与实例覆盖。编辑 SHALL 支持线性 base color RGBA、metallic、roughness、normal scale、occlusion strength、线性 emissive 和 emissive strength，以及当前表面 profile 的 alpha 和 double-sided 字段。每个被支持的纹理槽 SHALL 可选用 `Assets/Textures` 下的项目图像并编辑适用的 sampler、已有 UV 集和 UV 变换，色彩和通道解释 SHALL 由槽用途决定。Inspector SHALL 允许清除单个材质字段或纹理槽的覆盖并显示继承后的有效值。已导入材质的预览限制 SHALL 与可创作字段分开显示。

离散提交、清除、Undo 和 Redo SHALL 通过同一 Scene 编辑边界更新目标实例的稀疏覆盖。请求 SHALL 绑定文档、实体代、固定模型版本及源 Primitive 和字段或纹理槽。其它字段、其它实例与共享 glTF/GLB SHALL 保持原值。字段或采样组合无效时 SHALL 报告被拒绝的目标及原因。

#### Scenario: Edit and reset one inherited surface field
- **WHEN** 同一模型被实例化两次，用户在其中一个 Primitive 上写入 roughness 覆盖、清除该字段，然后撤销清除
- **THEN** 该实例后续被接受的帧先使用覆盖，再使用源 roughness，最后恢复覆盖；该 Primitive 的其它覆盖、另一实例及共享源保持原值

#### Scenario: Bind a valid texture to its slot and reject an incompatible edit
- **WHEN** 用户将项目纹理用于支持该采样组合的 base color 槽，然后为该槽提交目标 Primitive 缺失的 UV 集
- **THEN** 成功项以 sRGB 颜色语义采样，拒绝项指出有效 UV 的限制，已接受纹理、Scene 版本和历史保持有效；线性数据槽沿自身的用途解释图像

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

## MODIFIED Requirements

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
