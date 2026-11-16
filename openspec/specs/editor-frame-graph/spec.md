# editor-frame-graph Specification

## Purpose

规定编辑器产品帧如何将上传、诊断视口、UI 采样及窗口颜色纳入同一张渲染图，使消费者能够确定本帧读取的资源版本、实际执行的工作和提交后的结果，并在裁剪、尺寸变化及帧槽复用时保持身份和资源寿命正确。

## Requirements

### Requirement: Compose the product frame through one compiled graph

每个可渲染的产品帧 SHALL 从封存的 UI、当前 View 请求和宿主取得的窗口目标形成一张图。图 SHALL 声明实际纹理及缓冲上传、匹配的视口颜色生产、UI 采样、窗口颜色初始化与合成，并导出 UI 写入后的原窗口目标。Forward 视口 SHALL 在本图声明实际几何、深度、工作颜色和显示色的生产与消费。各读取 SHALL 指向对应生产版本及范围。构图和编译 SHALL 只形成 CPU 声明与计划，存活 Pass 的资源物化和命令由图执行阶段完成。

产品帧 SHALL 使用编译计划驱动 Graphics 录制。图尾交付的呈现输出 SHALL 对应本次取得的纹理和 `ShaderResource` 出口。Runtime 持有关闭列表、窗口末端复制、提交和交换责任，并沿现有宿主协议保护末端访问。

#### Scenario: Present an editor frame with graph-produced diagnostic color

- **WHEN** 主视口可见、诊断颜色和字体需要上传，并取得合法窗口目标
- **THEN** 计划中上传先于对应采样，窗口底色先于 UI 合成，合成后导出原目标。执行并获有效提交后，窗口显示标识明确的诊断图和正确 UI 像素

#### Scenario: Export an initialized target from an empty UI draw set

- **WHEN** 已封存的有效帧没有普通 UI draw，且没有其它图内消费者
- **THEN** 图仍初始化并导出本次窗口颜色，无消费者的几何和纹理上传缺席，窗口可呈现约定底色

#### Scenario: Compose forward color into the acquired window target

- **WHEN** 同一帧需要初次场景上传，并有会实际绘制的 Viewport Image 和基础 UI 纹理
- **THEN** 场景的真实写入、索引绘制、预览转换和 UI 样本读位于同一图的生产/消费链，原窗口目标经有效回执后可按已有合同呈现

### Requirement: Resolve UI tokens to exact frame-local texture versions

UI SHALL 保持会话和代可校验的稳定纹理 token。长期 View 条目只关联 View 身份和输出角色，驻留条目持有外部纹理及导入事实。当前帧绑定 SHALL 在本图内把实际 draw 使用的 token 映射到确切纹理版本及采样范围。GraphId 的寿命 SHALL 限于所属图和帧槽使用期。

同一 token 的重复引用 SHALL 共享本帧解析结果。同一外部对象的多个 token SHALL 共享规范导入身份，并消费本帧更新产生的正确版本。UI 的实际纹理、视图和绑定 SHALL 由当前 Pass 的已声明权限解析。非基础图像的无效 token 或缺失 View 输出 SHALL 沿已有诊断占位合同处理，占位本身的上传及采样也进入图。实际 draw 必需的基础字体无法准备时 SHALL 按既有合同返回失败。

#### Scenario: Sample an updated texture through repeated or aliased tokens

- **WHEN** 多条普通 draw 使用同一 token，或两个有效 token 指向同一驻留纹理，且该纹理本帧有一次更新
- **THEN** 图中只有一份规范资源身份，所有相关 draw 采样同一更新后版本，各自的裁剪和几何顺序保持，更新只执行一次

#### Scenario: Resolve a newly produced view color during execution

- **WHEN** UI 在构图时引用本帧新声明且尚未物化的测试图或 Forward 显示色
- **THEN** 封存的绑定保存本图版本，构图无需实际颜色纹理或其采样 BindingSet，UI 执行阶段从已授权资源取得实际颜色并采样

#### Scenario: Diagnose a stale token and reject an old frame binding

- **WHEN** 当前非基础图像 draw 含过期会话或代的 token，或调用者尝试在后继图中消费先前帧的绑定
- **THEN** 过期 token 解析为带原因的占位，先前帧绑定被拒绝且不会访问后继图中碰巧同索引的资源

### Requirement: Cull unused editor producers from actual UI reads

UI 的资源读取集合 SHALL 由会实际绘制的普通命令确定，空裁剪和 reset-state 命令不产生纹理采样。隐藏或全裁剪且无其它消费者的视口生产，以及没有存活消费者的普通上传 SHALL 被裁剪或省去声明。未消费分支的图物理资源不得物化，其绘制和上传不得执行。保留为 View 或驻留纹理的输出 SHALL 只在本帧实际消费需要该版本时成为外部导出。

同一图版本只要仍有合法 Image 消费者，相关生产者 SHALL 保持存活。隐藏和恢复视口 SHALL 沿用逻辑身份，恢复帧以当前布局与 View 请求解析输出。

#### Scenario: Hide or fully clip the only diagnostic consumer

- **WHEN** 诊断视口隐藏，或其全部 Image 命令的像素裁剪为空，且其它 UI 正常可见
- **THEN** 其它面板继续呈现，诊断分支没有原生分配和上传，View 有效设置保持最后接受值

#### Scenario: Keep a producer for a remaining Image reference

- **WHEN** 同一候选视口颜色被多个 Image 引用，其中一条被裁剪而另一条实际绘制
- **THEN** 生产者保留且只执行一次，存活 Image 采样本帧版本，重复引用不形成多份资源或上传

#### Scenario: Cull forward work when its last viewport consumer disappears

- **WHEN** 静态场景是唯一候选视口的来源，该 Image 被隐藏或完全裁剪且没有其它图内消费者
- **THEN** 场景分支未物化图附件、未发 Draw 或上传，场景版本和 View 请求均未确认，其他 UI 可继续呈现

### Requirement: Admit the uploads that will actually execute

帧准备 SHALL 根据存活计划中实际上传的大小、次序及列表参数核对现有上传容量，并独立持有执行需要的字节。上传准入与实际命令 SHALL 使用一致的工作集合，涵盖保留的场景和 UI 上传。被裁剪的上传不得占用本次 GPU 上传额度或变为已确认内容。

场景或诊断颜色暂时无法纳入共享上传容量时，准备 SHALL 沿既有 UI 优先行为选择带原因的占位并重新确定完整正常帧的依赖与预算。确定超出本 profile 硬限制的场景候选 SHALL 报告具体拒绝原因。基础 UI 仍等待容量时 SHALL 在开放列表前归还本次呈现取得，保持待处理请求，继续输入与退休推进。

#### Scenario: Exclude a culled texture request from upload admission

- **WHEN** 合法纹理上传请求已规划，但它的结果在最终图中没有存活消费者
- **THEN** 本次准入和命令均不包含该上传，请求保持待处理，其后真正被 UI 消费时才进入新的准入和执行

#### Scenario: Preserve UI priority under upload backpressure

- **WHEN** 加入场景或诊断上传将超出当前共享上传额度，而使用占位的基础 UI 可以纳入
- **THEN** 当前帧呈现带等待原因的 UI，预算与最终执行计划一致，View 保留最后有效设置。若基础 UI 也等待容量，本轮没有新 Graphics 录制

### Requirement: Publish only accepted graph outputs

图执行成功 SHALL 交付所请求的版本和实际对象。场景与 UI 上传确认及 View 有效比例、尺寸和颜色代发布 SHALL 同时满足对应工作实际执行、当前图完整成功，以及本次宿主回执接受完整有效颜色。提交票据、帧槽、录制和准备值 SHALL 属于同一次工作。测试图与安全 UI 的 `NoSceneOutputs` 语义保持实际场景渲染记录。匹配的 Forward 预览获接受时 SHALL 确认本次场景、相机和布局，供该次输出的图像映射消费。

构图、编译、执行、关闭或提交失败 SHALL 保留原始诊断, 结束该候选的 CPU 请求借用, 并阻止该候选成功输出发布。尚未发出命令时按安全 fallback 或归还取得的合同处理。已经发出的工作 SHALL 沿前缀提交或受控退出责任保活, 失败目标不得进入有效颜色复制和交换。交换结果继续独立于已经接受的输出发布。

独立安全 UI 图成功 SHALL 只确认自身实际执行且获匹配有效回执的 UI 上传。失败主图中的上传、候选 View 及读回 SHALL 不借用 fallback 的成功票据发布。安全图重复显示旧输出时 SHALL 保持原 View 有效比例、尺寸和颜色代, 不轮转场景历史。未执行、被裁剪或失败的 UI 或场景请求 SHALL 保留可重试数据和未确认状态，本轮 UI 元数据借用 SHALL 在下一次 UI 帧前结束。

#### Scenario: Accept a sampled view version with its matching submission

- **WHEN** 当前请求对应的视口版本实际被 UI 采样，图及关闭成功，宿主接受完整有效颜色
- **THEN** View 发布对应比例、像素范围及颜色代，相关已执行上传可确认，Forward 预览确认该次实际渲染记录，诊断图沿 `NoSceneOutputs` 合同处理，后续交换状态单独报告

#### Scenario: Preserve pending state when graph work fails

- **WHEN** 某图候选在命令发出前失败, 或已发上传后发生执行错误并只获得前缀票据
- **THEN** 该候选不发布有效颜色、上传完成或 View 有效设置, 首错可定位; 无命令候选结束后才可独立 fallback 或取消取得, 已发前缀及引用按真实完成或受控退出收拢

#### Scenario: Confirm retried UI work only from the successful fallback

- **WHEN** 主图取消后一个 UI 更新在安全图中重新准备并实际执行, 另一个请求未被安全图消费, 且安全图完整获接受
- **THEN** 只有安全图实际完成的更新被确认, 未消费请求仍待处理, 主图候选 View 和读回没有成功结果, 旧 View 输出和有效设置保持原值

### Requirement: Retain frame bindings and resource generations until safe reuse

帧槽 SHALL 有界持有当前图的绑定、封存几何和上传数据。Pass 延迟使用的值及 CPU 载荷 SHALL 在消费期间有效，纹理请求元数据借用 SHALL 在下一次 UI 帧推进前通过确认或取消结束。GPU 使用的纹理、绑定及目标代 SHALL 至少保留至覆盖相应访问的真实完成。

逻辑 View 和 UI token SHALL 跨 resize 保持稳定。新帧可选择新颜色代，仍在途的工作继续使用原代。帧槽重新准备 SHALL 先取得真实安全复用证明，再销毁或清理旧图绑定并开始新图代，正常帧沿现有有界背压运行。

#### Scenario: Resize while a previous binding remains in flight

- **WHEN** 先前提交仍持有视口颜色和 UI 绑定，当前布局或窗口尺寸要求新的目标代
- **THEN** 新帧的稳定 token 解析到正确的新输出，原提交继续引用旧对象，视口和窗口各按自身尺寸换代，真实完成后旧引用可退休

#### Scenario: Reuse a slot only after its submission completes

- **WHEN** 全部槽仍在途，随后某槽获得真实完成证明
- **THEN** 待完成期间输入和回收继续，槽存储及绑定保持有效，安全取得后旧帧数据被清理并建立新图代，旧准备值不能消费新帧内容

### Requirement: Classify frame failures with owned diagnostics

产品帧 SHALL 区分准备、主图声明/编译、执行、关闭、呈现交接和提交失败, 并区分可恢复取消、上传等待与必须停止渲染的结果。每次失败 SHALL 保留拥有型有界诊断, 包含首个失败阶段和已有的 Pass、资源或 RHI 操作定位。图或帧内存储销毁后诊断仍可读。后续 fallback 或清理错误 SHALL 作为附加原因记录, 不覆盖主错。

可恢复帧错误 SHALL 继续事件处理、有限等待与非阻塞退休。已启动的编辑器在下一次可以成功合成的 UI 中 SHALL 显示最近失败及恢复状态; 当前 fallback 可以显示通用旧输出/缺失输出标识, 详细错误最迟进入下一次可绘制的面板。状态存储 SHALL 有界, 无需保留每帧错误对象。准备失败的原候选 SHALL 始终保持失败, 后续尝试的诊断起点不得把原候选改记成功。

#### Scenario: Reject ordinary UI preparation without losing the acquisition

- **WHEN** 已完成基础 UI 初始化的会话在本轮 UI 准备中发生普通错误, 且尚未开放即时列表
- **THEN** 本轮准备被取消, 呈现取得被正确归还, 没有新录制、复制或交换; 请求保留待重试, 事件和后续安全帧继续, 错误阶段可观察

#### Scenario: Preserve the primary graph error after fallback also fails

- **WHEN** 主图编译失败后安全 UI 图准备或编译也失败, 随后两张候选图和帧内数据被释放
- **THEN** 首个图错误仍有完整的有界定位, fallback 失败作为附加原因, 下一次可绘制 UI 能解释本次未呈现的原因

#### Scenario: Keep a failed candidate failed across diagnostic scopes

- **WHEN** 主候选已记录失败, 随后独立 fallback 的准备和执行未产生新错误
- **THEN** 主候选仍不可发布输出, fallback 按自己的完整准备/执行结果决定资格, 两者的结果不因重新取样诊断而混同

### Requirement: Compile an independent safe UI fallback before recording

主图声明、封存或编译失败且尚未开放即时列表, 已验证的基础 UI 程序仍可用、必需字体能够满足本次 UI 消费且没有 Fatal 时, 产品帧 SHALL 至多尝试一张独立的安全 UI 图。失败图及其帧内资源绑定 SHALL 结束后再建立新图身份与准备值。安全图 SHALL 使用同次合法呈现取得, 初始化并合成其窗口颜色, 沿相同的上传准入、图编译、执行与宿主呈现合同运行。

安全图 SHALL 仅包含窗口初始化、必需 UI 上传和合成、合法驻留 UI 读取及同一 View 上次接受输出的显式读取; 不包含失败的诊断/场景生产分支或其读回与历史导出。旧输出 SHALL 具有有效内容、明确状态与独立寿命, 且未被失败候选覆写。无有效旧输出时 SHALL 显示带原因的占位。安全图的结果 SHALL 具有 `NoSceneOutputs` 语义。

安全图准备、编译或硬容量校验失败 SHALL 取消本次取得, 不递归 fallback。普通上传等待 SHALL 归还取得并等待后续安全尝试。只要一次图执行已经开始, 即使在首次资源/recipe 创建处失败且尚无 Draw, 本轮也 SHALL NOT 拼接安全图, 而应按执行失败协议处理已开放录制。

#### Scenario: Replace a rejected main graph with a safe UI graph

- **WHEN** 主图被编译器拒绝, 同一 View 存在上次有效输出, 当前尚未开放列表
- **THEN** 失败主图没有执行上传或绘制命令, 新图只导入旧输出并合成 UI, 使用独立图代和新帧内绑定, 主图失败分支及其读回不执行

#### Scenario: Recover before any view output has been accepted

- **WHEN** 首次正常图失败, 没有可导入的已接受 View 输出, 基础 UI 可完成准备和上传
- **THEN** 安全图呈现明确缺失状态和编辑器界面, 不访问空或未初始化的旧纹理, 不伪造场景历史或有效 View 设置

#### Scenario: Cancel a failed fallback without another retry in the same acquisition

- **WHEN** 独立安全图准备、编译失败或其所需上传超过硬上限
- **THEN** 本次取得结束且无录制或交换, 不再生成另一张恢复图, 请求保持未确认, 后续事件和合法新帧仍可继续

#### Scenario: Refuse fallback after execution has started

- **WHEN** 正常图或安全图在首次物化、附件/绑定 recipe 或后续 Pass 执行时失败
- **THEN** 后续 Pass 和图尾成功发布停止, 本轮不执行另一张图, 窗口目标不获得复制或交换资格, 录制及可能已发前缀按真实提交或受控退出收拢

### Requirement: Latch fatal graphics diagnostics before further frame work

宿主已有消息入口 SHALL 在收到 Fatal 时锁存停止新渲染, 不依赖日志写入、字符串匹配或短期诊断作用域。Runtime SHALL 在每个可能产生图形诊断的准备阶段返回后检查该事实, 包括 UI、图消费者准备及 fallback 准备, 并在开放列表和发布结果前再次守住对应边界。Fatal SHALL 禁止继续准备、fallback、执行、成功发布和交换, 只允许符合原生设备健康状态的收拢。

Fatal 与 context/device 是否仍可用 SHALL 分别记录。原生环境仍可用时, 已发工作沿有效 fence 或仅限退出边界的收拢协议保活; 已确认失效时 SHALL 不再发原生命令或等待失败票据。普通 Error SHALL 使对应尝试失败, 不自动设置 Fatal。

#### Scenario: Stop after a late preparation producer reports Fatal

- **WHEN** UI 或图消费者的准备在先前准备成功后报告 Fatal
- **THEN** 不再编译或执行该候选, 不进入 fallback 或开放列表, 当前取得和已创建引用有明确退出 owner, 之前在途工作仍按适用关闭协议收拢

#### Scenario: Stop when fallback preparation reports Fatal

- **WHEN** 主图尚未执行, 安全图准备期间报告 Fatal
- **THEN** 不继续安全图执行或下一帧准备, 保留主图首错及 Fatal 原因, 不发布颜色、上传完成或历史

#### Scenario: Distinguish a fatal latch from native context loss

- **WHEN** Fatal 到达时已有先前在途工作, 后端分别处于仍可用和已确认失效的状态
- **THEN** 前者完成合法的退出收拢后释放依赖, 后者只走失效退出且不等待失败编号; 两者均不伪造完成或清除停止事实
