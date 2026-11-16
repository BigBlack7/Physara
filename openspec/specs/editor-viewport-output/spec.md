# editor-viewport-output Specification

## Purpose

规定编辑器主视口如何在会话中保持逻辑身份、按本帧布局选择离屏颜色版本，以及场景输出未就绪时如何呈现可诊断的编辑器画面。

## Requirements

### Requirement: Resolve a stable viewport token to the current output version

编辑器的主视口 SHALL 在其生命周期内保持稳定的 `ViewId` 和逻辑 `UiTextureId`。视口纹理注册 SHALL 关联 View 身份及显示色角色，当前帧的采样绑定 SHALL 解析到该 View 的有效输出代。测试图与 Forward 预览 SHALL 通过同一显示色角色和稳定 token 提供，输出代携带本帧内容种类和预览模式。视口布局变化及重新显示 SHALL 沿用逻辑身份。已发工作使用的旧纹理和绑定 SHALL 保留至相应完成或受控收拢。

正常帧 SHALL 使用匹配当前请求的输出。独立安全 UI 图 SHALL 显式选择同一 View/角色上次成功接受、内容未被覆写且状态可验证的输出; 该只读旧输出可以按当前面板矩形显示, 即使尺寸或请求代不同, 但 SHALL 标识为上次有效输出。该选择不允许过期 token、其它 View 或未接受候选冒充旧输出。安全图 SHALL 为其重新建立本图采样绑定, 长期状态不保存旧 GraphId。无符合条件的旧输出时 SHALL 使用带原因的占位。

#### Scenario: Resolve the same viewport token after a resize

- **WHEN** 视口由一个有效尺寸变为另一个有效尺寸, 旧代仍被先前的提交采样
- **THEN** 已准备的新正常帧用同一 token 解析新尺寸对应的输出代, 旧提交继续持有原纹理和绑定, 真实完成后原代可退休

#### Scenario: Resolve an expired or missing viewport binding

- **WHEN** UI 引用已失效的会话或 View 身份, 或正常帧没有与请求匹配的可用输出且未选择独立安全 UI 图
- **THEN** 视口呈现可辨认且带原因的占位, 过期的 token 和输出代不能解析到其它 View 或被复用的纹理

#### Scenario: Show a retained output after a resized candidate fails

- **WHEN** 新尺寸或比例的主图失败, 安全图选择同一 View 已接受的旧尺寸输出, 且旧输出仍被此前提交引用
- **THEN** 当前面板只读显示旧像素并明确标注恢复状态, 两次提交均保有各自依赖, 新候选没有覆写旧输出, 正常帧恢复后才可按当前请求发布新代

#### Scenario: Use the same viewport token for the first accepted scene preview

- **WHEN** 空文档的测试图已获接受，随后当前文档为同一主 View 提供可绘静态场景
- **THEN** 首次接受的 Forward 预览通过原 token 显示，标识与本次像素的实际模式一致，旧测试图保持自身输出代和接受记录

### Requirement: Apply viewport extent at a frame boundary

Editor SHALL 从面板内容矩形、本次有效的 framebuffer 对逻辑窗口比例和主 View 当前请求的渲染比例计算 View 像素请求。各轴 SHALL 在比例相乘后向下取整并检查范围和算术。窗口 PresentColor 和离屏 View 输出 SHALL 按各自尺寸准备。布局及相关资源请求 SHALL 在录制前确定，录制期间到达的变化只供后续帧消费。零面积或隐藏视口 SHALL 不发起新的离屏输出分配。

编辑器 SHALL 以同批逻辑布局和 View 准备的有效图像矩形建立内容映射。图像及其输入坐标 SHALL 采用该有效区域，留边或失效布局返回无有效图像位置。

#### Scenario: Resize the window and viewport independently
- **WHEN** 连续改变窗口像素尺寸、视口停靠布局或 framebuffer 比例
- **THEN** 新帧的窗口目标匹配最新有效窗口像素范围，离屏输出对应当帧内容区域的像素请求，内容缩放不被重复计入，单独改变布局不重建窗口目标

#### Scenario: Hide and reopen a viewport
- **WHEN** 面板隐藏或内容面积变为零，随后恢复可绘制面积
- **THEN** 空闲期间其余编辑器界面仍可合成，没有为该视口分配替代纹理，恢复时按当前内容矩形解析同一逻辑 View 和 token

#### Scenario: Scale diagnostic pixels independently of window pixels
- **WHEN** 本帧内容矩形有效，View 的渲染比例在准备前改变
- **THEN** 离屏请求使用新比例和各轴实际 framebuffer 比例，窗口目标仍由窗口像素决定，逻辑输入经当帧布局和有效图像矩形映射

### Requirement: Present a diagnostic editor without advancing scene history

当渲染结果为 `NoSceneOutputs` 时，Runtime SHALL 仍能通过清屏和基础 UI 合成呈现窗口。视口 SHALL 明确显示无场景或场景准备状态，诊断测试图 SHALL 有独立的可见标识。测试图不可用时 SHALL 显示带原因的占位。该路径 SHALL 保持 View 的已渲染相机、曝光和颜色/深度历史状态。

正常帧的诊断测试图 SHALL 在匹配的有效图像矩形内呈现内容，并令留边显示为背景。相机或图像区域准备被拒绝时 SHALL 通过可见的 View 状态或占位提供原因，其他可用 UI 继续运行。

#### Scenario: Present a labeled test viewport with no scene
- **WHEN** 没有可渲染场景且基础 UI 与本帧测试输出都可用
- **THEN** 窗口显示 Docking 界面与标识明确的测试图，成功接受和呈现此帧也不会提交场景 View 历史

#### Scenario: Keep the editor visible while viewport output is pending
- **WHEN** 场景尚无有效输出，本帧也未能准备匹配的诊断纹理，且基础 UI 可正常录制
- **THEN** 有效的窗口帧仍包含清除的底色、可用的面板和说明状态的视口占位，View 历史保持原值

#### Scenario: Show diagnostic content only within a fixed camera image
- **WHEN** 固定比例的 Camera 已准备且当前面板比例不同，本帧合成采样该 View 的诊断纹理
- **THEN** 图案仅覆盖有效图像区域，留边为背景，诊断标识可见，场景相机和颜色／深度历史保持原值

### Requirement: Confirm effective viewport settings from accepted output

View SHALL 分别公开最新合法请求比例、最后接受的有效比例及其输出像素范围。首次确认前有效值 SHALL 显示为尚未确认。待确认的请求缺少可用布局、槽或暂时可用的上传容量时 SHALL 报告 Pending 及原因。请求尺寸超出设备能力、有效像素范围或单目标预算时 SHALL 报告具体拒绝原因。两种结果均保留最后接受的有效状态。只有匹配当前请求的测试图或 Forward 预览实际用于完整正常图合成且该提交被接受后，View 才可发布其有效比例和尺寸。窗口交换结果 SHALL 与该确认独立。正常帧无匹配颜色时 SHALL 沿现有诊断占位合同呈现。

正常候选 SHALL 匹配输出像素范围和有效图像矩形。Forward 预览还 SHALL 匹配当前 Scene 内容、渲染相机与所选预览模式，以该次实际渲染的相机和矩形提供图像映射。相机变化导致矩形变化时 SHALL 等待匹配候选被采纳，准备失败或等待期间保有最后接受的输出及其矩形。

安全 UI 图读取旧输出或显示占位 SHALL 不确认当前请求, 即使旧输出的尺寸碰巧相同。最近主图失败及使用旧输出的状态 SHALL 可见, 原有效比例、尺寸和颜色代保持不变; 后续正常图成功才能清除该请求的失败/等待状态并发布匹配结果。没有当前有效图像时图像输入映射 SHALL 不可用。

#### Scenario: Keep the last effective setting while a request is pending

- **WHEN** 新请求已经应用, 但视口隐藏、当前没有可用槽或匹配输出仍等待共享上传容量
- **THEN** 状态报告新请求及等待原因, 保留最后接受的比例和尺寸, 未获得匹配颜色的可见正常视口按现有规则显示占位

#### Scenario: Publish the setting with the matching accepted frame

- **WHEN** 新比例对应的当前颜色在完整正常图合成中被采样，且该次提交已接受
- **THEN** View 发布该比例与实际输出尺寸，测试图在取整后尺寸与有效图像矩形均相同时可沿用已经接受的匹配颜色代，Forward 预览以本次场景、相机和模式的匹配输出为准

#### Scenario: Recover from an unsupported extent

- **WHEN** 当前请求和布局产生的尺寸超出可支持范围, 随后编辑或布局变化产生可准备的范围
- **THEN** 拒绝阶段报告具体范围或预算原因并保留最后有效状态, 恢复后只发布成功接受的当前请求和输出

#### Scenario: Keep requested and effective values distinct during fallback

- **WHEN** 新请求的主图失败, 旧输出通过安全 UI 图成功呈现, 随后一次正常图按新请求成功
- **THEN** 安全图期间请求及失败原因可见而有效值不变, 只有后续正常图的匹配回执推进有效比例、尺寸和输出代

#### Scenario: Replace the matching image layout without resizing the canvas

- **WHEN** 同一输出像素范围改用不同固定 aspect 的相机，有效图像矩形随之变化
- **THEN** 先前内容只具备旧输出资格，当前正常合成采用匹配矩形的候选，接受后才确认该布局和颜色代

#### Scenario: Keep scene preview identity consistent at the same extent

- **WHEN** 当前画布及图像矩形相同，场景内容、相机或预览模式改变，随后新候选等待上传或发生图失败
- **THEN** 先前预览只可从安全 UI 以旧输出标明，当前有效模式和图像映射等待匹配的新候选；该候选实际采样并获接受后，颜色与渲染相机记录一并推进
