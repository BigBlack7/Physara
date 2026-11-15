# Spec Delta

## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: Publish only accepted graph outputs

图执行成功 SHALL 交付所请求的版本和实际对象。UI 上传确认及 View 有效比例、尺寸和颜色代发布 SHALL 同时满足对应工作实际执行、当前图完整成功, 以及本次宿主回执接受完整有效颜色。提交票据、帧槽、录制和准备值 SHALL 属于同一次工作。诊断输出继续使用 `NoSceneOutputs` 语义, 场景历史保持原状态。

构图、编译、执行、关闭或提交失败 SHALL 保留原始诊断, 结束该候选的 CPU 请求借用, 并阻止该候选成功输出发布。尚未发出命令时按安全 fallback 或归还取得的合同处理。已经发出的工作 SHALL 沿前缀提交或受控退出责任保活, 失败目标不得进入有效颜色复制和交换。交换结果继续独立于已经接受的输出发布。

独立安全 UI 图成功 SHALL 只确认自身实际执行且获匹配有效回执的 UI 上传。失败主图中的上传、候选 View 及读回 SHALL 不借用 fallback 的成功票据发布。安全图重复显示旧输出时 SHALL 保持原 View 有效比例、尺寸和颜色代, 不轮转场景历史。未执行、被裁剪或失败的 UI 请求 SHALL 保留可重试数据和未确认状态, 本轮元数据借用在下一次 UI 帧前结束。

#### Scenario: Accept a sampled view version with its matching submission

- **WHEN** 当前请求对应的诊断版本实际被 UI 采样, 图及关闭成功, 宿主接受完整有效颜色
- **THEN** View 发布对应比例、像素范围及颜色代, 相关已执行上传可确认, 诊断图不推进场景历史, 后续交换状态单独报告

#### Scenario: Preserve pending state when graph work fails

- **WHEN** 某图候选在命令发出前失败, 或已发上传后发生执行错误并只获得前缀票据
- **THEN** 该候选不发布有效颜色、上传完成或 View 有效设置, 首错可定位; 无命令候选结束后才可独立 fallback 或取消取得, 已发前缀及引用按真实完成或受控退出收拢

#### Scenario: Confirm retried UI work only from the successful fallback

- **WHEN** 主图取消后一个 UI 更新在安全图中重新准备并实际执行, 另一个请求未被安全图消费, 且安全图完整获接受
- **THEN** 只有安全图实际完成的更新被确认, 未消费请求仍待处理, 主图候选 View 和读回没有成功结果, 旧 View 输出和有效设置保持原值
