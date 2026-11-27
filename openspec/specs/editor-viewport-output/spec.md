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

Editor SHALL 从面板内容矩形和当前实际 framebuffer 对逻辑窗口的各轴比例取得未取整的物理像素范围。正常场景的显示画布 SHALL 对该范围向下取整，渲染栅格 SHALL 对原范围乘当前请求比例后各轴向下取整，并检查有效范围和算术。无场景的诊断测试图仍 SHALL 使用按渲染比例换算的像素范围。窗口 PresentColor SHALL 按实际 framebuffer 像素独立准备。布局及相关资源请求 SHALL 在录制前确定，录制期间到达的变化只供后续帧消费。零面积或隐藏视口 SHALL 不发起新的离屏输出分配。

合成 SHALL 使用该输出携带的布局和有效图像矩形，正常场景使用显示画布的矩形，无场景诊断图使用自身的矩形。图像输入 SHALL 以当前封存布局从逻辑窗口坐标转换到已接受输出的画布，再从该有效矩形取得左上 UV。该映射直接使用已换算的画布尺寸，留边或失效布局返回无有效图像位置。导航命中 SHALL 使用与当前 Scene、相机、预览和 AA 请求以及布局相符的正常已接受图像，安全回退、未获接受的候选、占位和被剪掉的区域 SHALL 没有输入命中资格。画布的可见性和帧边界像素请求保持独立于导航捕获。

#### Scenario: Resize the window and viewport independently
- **WHEN** 连续改变窗口像素尺寸、视口停靠布局或 framebuffer 比例
- **THEN** 新帧的窗口目标匹配最新有效窗口像素范围，正常场景的输出画布与渲染栅格分别对应当帧两个像素请求，诊断测试图使用其按比例换算的范围，内容缩放只计算一次，单独改变布局不重建窗口目标

#### Scenario: Hide and reopen a viewport
- **WHEN** 面板隐藏或内容面积变为零，随后恢复可绘制面积
- **THEN** 空闲期间其余编辑器界面仍可合成，没有为该视口分配替代纹理，恢复时按当前内容矩形解析同一逻辑 View 和 token

#### Scenario: Scale diagnostic pixels independently of window pixels
- **WHEN** 本帧内容矩形有效，View 的渲染比例在准备前改变
- **THEN** 离屏请求使用新比例和各轴实际 framebuffer 比例，窗口目标仍由窗口像素决定，逻辑输入经当帧布局和有效图像矩形映射

#### Scenario: Map a point inside a letterboxed view exactly once
- **WHEN** 当前有效图像有固定比例留边，实际 framebuffer/client 比例和内容缩放不同，render scale 小于 1
- **THEN** 同一可见逻辑点按当前画布和半开的有效图像矩形得到预期 UV，留边、被裁剪区域和右下边界返回无命中

#### Scenario: Wait for a current image after layout or camera changes
- **WHEN** 用户改变大小、切换场景相机或隐藏后重开，安全 UI 或等待状态可能保留旧有效颜色
- **THEN** 旧色可按自身回退合同显示，不能启动新捕获或提供本次有效图像映射；正常图实际采样并接受匹配结果后，同一 View 和 token 可以继续使用

#### Scenario: Keep display pixels at panel resolution when render scale changes

- **WHEN** 正常视口的面板内容折算为 321.8×241.25 物理像素且窗口布局固定，渲染比例从 1 改为 0.75
- **THEN** 输出画布仍为 321×241，渲染栅格变为 241×180；图像命中使用当前已接受的显示矩形，窗口目标继续由实际 framebuffer 决定

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

View SHALL 分别公开最新合法请求比例、最后接受的有效比例及其输出像素范围。首次确认前有效值 SHALL 显示为尚未确认。待确认的请求缺少可用布局、槽或暂时可用的上传容量时 SHALL 报告 Pending 及原因。请求尺寸超出设备能力、有效像素范围或单目标预算时 SHALL 报告具体拒绝原因。两种结果均保留最后接受的有效状态。只有匹配当前请求的测试图或 正常场景预览实际用于完整正常图合成且该提交被接受后，View 才可发布其有效比例和尺寸。窗口交换结果 SHALL 与该确认独立。正常帧无匹配颜色时 SHALL 沿现有诊断占位合同呈现。

正常候选 SHALL 匹配其所需的输出像素范围和有效图像矩形。正常场景预览还 SHALL 匹配当前 Scene 内容、相机、预览模式、AA 请求以及渲染和显示布局，以该次实际渲染的快照和显示矩形提供图像映射。就绪的请求策略或仍有效的原策略 SHALL 以其实际正常输出确认这些值。原策略正常交付期间，最新路径请求 SHALL 继续显示自己的准备状态。View Settings SHALL 分别显示合法的 AA 请求和最后一次被接受场景实际使用的 AA、显示 profile、渲染尺寸和输出尺寸；首次场景确认前这些有效值 SHALL 标为尚未确认。Physical Lighting SHALL 消费有效的 None 或 FXAA；诊断模式 SHALL 确认实际使用的 None，说明该模式下 FXAA 不生效，保留合法请求。诊断测试图只确认自身比例和有效布局。相机或请求变化时 SHALL 等待匹配候选被采纳，准备失败或等待期间保有最后接受的输出及其矩形。

安全 UI 图读取旧输出或显示占位 SHALL 不确认当前请求, 即使旧输出的尺寸碰巧相同。最近主图失败及使用旧输出的状态 SHALL 可见, 原有效比例、尺寸和颜色代保持不变; 后续正常图成功才能清除该请求的失败/等待状态并发布匹配结果。所选显示 profile、FXAA 或缩放准备或执行失败时，当前状态 SHALL 提供可定位原因，旧结果的有效配置 SHALL 保持与旧图匹配。没有当前有效图像时图像输入映射 SHALL 不可用。

#### Scenario: Keep the last effective setting while a request is pending

- **WHEN** 新请求已经应用, 但视口隐藏、当前没有可用槽或匹配输出仍等待共享上传容量
- **THEN** 状态报告新请求及等待原因, 保留最后接受的比例和尺寸, 未获得匹配颜色的可见正常视口按现有规则显示占位

#### Scenario: Publish the setting with the matching accepted frame

- **WHEN** 新比例对应的当前颜色在完整正常图合成中被采样，且该次提交已接受
- **THEN** View 发布该比例与实际输出尺寸，测试图在取整后尺寸与有效图像矩形均相同时可沿用已经接受的匹配颜色代，正常场景预览以本次场景、相机和模式的匹配输出为准

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

#### Scenario: Confirm the actual display profile and AA only with a matching frame

- **WHEN** 已有一个接受的 Physical Lighting 输出，用户在相同尺寸请求 FXAA；其首个候选失败或过期，后一匹配图实际采样新输出并成功提交
- **THEN** 在安全 UI 期间可辨认当前请求、原因及旧图实际的 profile 和 AA；有效配置及图像命中由后续完整正常帧一并确认，窗口交换状态独立可见

#### Scenario: Keep the FXAA request while inspecting diagnostic colors

- **WHEN** 保有 FXAA 请求的 View 从已接受的 Physical Lighting 切到一个诊断预览，再切回 Physical Lighting
- **THEN** 已接受的诊断帧报告诊断显示和实际 None，请求仍为 FXAA；匹配的下一物理帧实际执行并确认 FXAA

#### Scenario: Match display and image mapping when a deferred switch overlaps a new viewport size

- **WHEN** 主 View 请求 Deferred 和新的视口尺寸，原有效策略先以当前 FXAA 请求正常交付，随后 Deferred 的匹配颜色被正常图采样并接受
- **THEN** 有效比例、布局、实际 AA 和图像映射各自对应真实接受的颜色。原策略在新尺寸的帧保持自身生产身份，Deferred 确认时显示与该次提交配对的路径及配置

### Requirement: Record the pre-exposure of the last accepted scene render

View SHALL 分别提供当前合法请求的曝光计算结果和最后成功渲染的预曝光。首次场景渲染确认前，后者 SHALL 显示为尚未确认。确认时 SHALL 使用本次实际渲染相机快照中的曝光，且场景输出与当前 View 请求、有效布局、场景和预览模式匹配，在完整正常图中被实际采样并获得可靠的 Graphics 提交接受。曝光及相机状态 SHALL 随同一结果确认。

不可准备、待提交、陈旧候选、安全 UI 旧图或测试图 SHALL 保留最后成功渲染的记录。继续显示旧结果时 SHALL 标明它所属的旧有效输出或场景状态。单纯改变曝光 SHALL 与几何投影的变化独立表达。

#### Scenario: Keep the last rendered exposure while a new request is pending

- **WHEN** 场景以上次预曝光成功提交，用户仅调整曝光，先遇到等待或安全 UI，后一正常帧实际采样新场景结果并获接受
- **THEN** 新曝光请求先可见，等待或回退时最后渲染值仍对应旧帧，新的可靠接受才同时确认相机和本次预曝光

#### Scenario: Reject a late scene result from an older camera request

- **WHEN** 一个场景候选已准备，之后 View 接受另一物理相机请求，随后旧候选尝试确认
- **THEN** 旧候选失去当前场景结果资格，仍然由后续匹配且成功接受的结果发布最后渲染值

### Requirement: Confirm the effective environment with the accepted scene render

请求的环境 SHALL 与最近一次成功渲染实际使用的环境分开呈现。状态 SHALL 可区分场景未配置环境、当前未消费所声明的用途、等待准备或提交、拒绝以及已接受的有效用途。已接受结果 SHALL 对应当前帧实际使用的资源代、标定、用途和查询参数，并与该帧的场景、View、模式及曝光一并确认。缺少可用候选时 SHALL 沿现有安全 UI 选择并标明上次有效图像或带理由占位。安全 UI、过期请求及匹配失败的候选 SHALL 不推进已接受环境记录。

#### Scenario: Show which environment produced the last accepted image
- **WHEN** 环境 A 的 Physical Lighting 已接受，随后打开带 B 的场景，B 先等待预计算或上传，后由正常图实际使用并获得可靠提交
- **THEN** 等待期间界面显示 B 的请求及原因，旧图像若被使用则仍标明 A，B 完整确认后界面和图像表示同一新代

### Requirement: Inspect typed opaque surface previews for the current view

编辑者 SHALL 能在主 View 的普通受光结果旁选择一种表面预览。三种策略在拥有可采样的配对主 D32 时 SHALL 提供标明近端为 1 的 Reverse-Z 深度。Deferred 的最终表面可交付时 SHALL 提供 G0 线性基色与 AO、G1 世界着色法线与过滤后的感知粗糙度及核心金属度、G2 预曝光自发光和 G3 路由。前向策略仅凭其普通深度提供深度预览，依赖 GBuffer 的选项显示当前不可用原因。

预览 SHALL 在同图以类型化读取将当前资源转换为普通可采样颜色，再通过显式图读取交给 UI。深度、法线、核心参数和路由 SHALL 使用同一生产 View 的实际渲染栅格、有效矩形及各自的最终版本。预览 SHALL 明确标示当前通道与颜色含义。深度灰阶用 `[0,1]` 设备深度值，值为 0 时保留其数值语义。G3 路由以有区别的颜色区分核心、Unlit、不透明前向扩展与无覆盖。G0、G1 金属度、G2 只在最终核心分类有效时显示核心材质值。辅助覆盖处 SHALL 仍能检查最终法线、粗糙度及其路由。留边 SHALL 保持已定义的预览背景。

#### Scenario: Inspect the final visible material through a masked auxiliary surface

- **WHEN** Deferred 的前景 Unlit MASK 跨过有不同基色、AO、金属度与自发光的核心接收面，另一块辅助几何被核心遮挡，并选看同一渲染矩形的多个通道
- **THEN** 孔洞和遮挡处显示实际胜出的核心数据，前景覆盖处保留 Unlit 标记、最终 G1 法线和粗糙度，核心专属通道以无有效核心值表示。可识别整数路由类别的输出通过普通颜色采样进入 UI。

#### Scenario: Inspect reverse depth on each strategy and classify a covered far plane

- **WHEN** 有有效主 D32 的相同单样本 View 轮换三策略，固定比例相机的有效图像中包含 D32 为 0 的有限远面与空白
- **THEN** 三种策略的深度预览各自按当前渲染像素显示近深度与零深度。在 Deferred 中，路由和法线预览从分类区分有效远面和空白。图像外留边保持背景。前向的 GBuffer 选项报告缺少当前表面。

### Requirement: Bind surface previews only to a live matching frame

主 View 的表面预览 SHALL 使用与普通显示色独立的会话输出角色和逻辑纹理身份。只有当前普通视口和预览各有实际 UI 读取、该帧所选策略交付匹配输入、预览程序与资源准入均就绪时，才 SHALL 交付本帧的预览颜色。绑定及原生引用 SHALL 分别沿本图和实际提交完成保护。尺寸、场景、相机、实际路径、当前模式或所选通道变更后，预览 SHALL 取匹配本轮的输出或带原因的占位。

关闭、隐藏或全部裁剪预览时，新增的图生产和原生资源工作 SHALL 随消费消失。当前无匹配普通颜色、正在显示安全 UI、预览输入或可用程序缺失，以及预览专属准入失败时，预览 SHALL 使用可诊断占位。若选中的预览超出 UI 几何预算，或原因标签超出正常图剩余的上传额度，SHALL 保留空画布并按正常图的提交票据记录当前原因。诊断记录 SHALL 保留完整票据和可辨认的原因，较长原因允许截断。普通主 View、独立离屏 View 及普通显示色的发布 SHALL 继续遵循其实际准备和提交合同。预览采样自身 SHALL 只有诊断含义，受光路径、曝光、AA、历史、文档编辑和主视口导航命中仍仅由各自已有入口与接受条件决定。

#### Scenario: Keep the current preview version through a change of layout and path

- **WHEN** 正在预览的 Deferred View 重设尺寸、路径请求尚待准备，随后普通帧接受 Forward 并再切回 Deferred，且仍有早先使用预览的提交在途
- **THEN** 每张正常图只绑定与其实际生产相符的预览版本。接受 Forward 后所选的 GBuffer 通道显示占位，恢复 Deferred 后使用该帧的新布局和数据。旧引用由原提交安全退休。

#### Scenario: Keep normal color when a diagnostic consumer disappears or is unavailable

- **WHEN** 一帧存在预览需求但其专属程序或资源无法准入，随后条件恢复、预览可读，下一轮预览关闭或完全被裁剪
- **THEN** 普通颜色在自身合法时继续被实际采样并正常接受。预览先显示当前原因，额度不足以附带图像或原因标签时按同一完整提交票据记录相应原因。恢复后才采样当前预览，撤去消费后只有普通帧所需的生产和资源工作，也没有新的预算拒绝记录。
