# Design

## Context

动机与工作包范围见 [proposal](proposal.md)。规划基准为 `3a3d5a6afdbaea193664f393243ea6e3da3818b0` (`D05-T04实现`), 建立本 change 前工作区干净。D05-T01 至 T04 已验收归档; 本设计只补齐 [D05-T05](../../../../Docs/Developments/D03-D05-FramePipeline.md#d05-t05) 的失败闭环。

已核对的实际边界:

| 现有实现 | 本次需要衔接的行为 |
|---|---|
| [Runtime::RenderFrame](../../../../Runtime/Main.cpp) 只返回 Rendered、UploadPending 或文本错误, 任一文本错误使主循环返回 1 | 增加可恢复取消与必须退出的明确结果; fallback 不再被主图错误直接中止 |
| [View](../../../../Engine/Renderer/View.cpp) 只在完整接受后保存诊断颜色, 新候选不覆写已接受纹理 | 提供旧输出快照, 不通过再次 Prepare 伪装成当前请求候选 |
| [EditorSession::PrepareUiFrame](../../../../Editor/Session/EditorSession.cpp) 要求输出尺寸及请求代匹配封存布局 | 为安全 UI 显式选择旧输出, 保留正常帧的匹配检查 |
| [PreparedUiFrame](../../../../Editor/Session/UiFrame.hpp) 一次构图, Cancel 撤回临时 token; [UiRenderer](../../../../Editor/ImGui/UiRenderer.cpp) 已能给封存几何追加状态文字 | 重建准备值而非二次 BuildGraph; 沿同一状态显示链加入失败/旧输出标签 |
| [RhiMessageAdapter](../../../../Platform/RhiDeviceHost.cpp) 累计 Error/Fatal, 但未单独锁存 Fatal; [RhiErrorScope](../../../../Platform/DeviceHost.hpp) 只比较计数起点 | 保存独立 Fatal 停止事实和每次尝试的诊断结果, 避免新 scope 掩盖早先失败 |
| [Presentation](../../../../Platform/Presentation.cpp) 已按 Event 复用槽, 失败前缀因窗口目标末态不确定而全局 stopped | 成功接受且有可靠观察的普通前缀只隔离该目标代, 完成后更换; 无可靠票据仍停止 |
| [图执行器](../../../../Engine/Renderer/RenderGraph/GraphExecutor.cpp) 已返回定位错误并恢复允许的执行模式; [GL 提交](../../../../RHI/OpenGL/Submission.cpp) 拒绝 invalid packet/tracker | 继续依赖现有有效列表与 orphan/退出保活协议, 不把 invalid 包改成可提交 |

架构依据: [主循环](../../../../Docs/Architecture.md#frame-flow)、[图编译失败](../../../../Docs/Architecture.md#graph-compile)、[图资源寿命](../../../../Docs/Architecture.md#graph-resources)、[历史提交](../../../../Docs/Architecture.md#history-commit)、[取消/关闭](../../../../Docs/Architecture.md#shutdown)。[NVRHI 提交参考](../../../../Docs/Reference/NVRHI/ExecutionAndLifetime.md#n9) 说明提交身份、状态发布与完成退休的区别; 本项目 GL 即时语义以现有 GL 代码及架构为准, 不照搬 Vulkan 的录制/重放模型。

## Goals / Non-Goals

**Goals:**

- 让一次窗口取得只有一个明确终点: 完整正常帧、完整安全 UI 帧、已归还的无命令取消、已绑定真实完成的失败前缀, 或交给受控关闭的活动工作。
- 以同一 UI/Graph/RHI 链提供恢复, 使发布资格、旧输出读取资格与 GPU 退休条件各自可证明。
- 主循环保留编排职责, View、Editor 和 Platform 各自拥有输出、UI 请求和呈现状态; 增量状态有固定容量和清理点。

**Non-Goals:**

- 不建立通用事务管理器、另一套 UI Renderer、公共 RHI 错误框架、可注入的产品测试服务或跨帧资源池。
- 不恢复 invalid 原生列表, 不在执行中途切图, 不为 Fatal 重建 Device。普通错误是否能继续取决于真实关闭/接受/完成事实。
- 当前仅有静态诊断 View。Scene、颜色/深度时域历史及产品截图/拾取尚未接入, 不创建空壳历史/读回 owner 来声称这些能力完成。

## Decisions

### 1. 用明确的结果与阶段决定后续动作

沿 Runtime 已有 FrameStatus 扩展当前工作所需的结果, 使“帧取消但可继续”和“必须退出”不再共用文本错误分支。诊断正文继续使用 Core 有界错误值, 分类来自返回值/阶段/宿主事实, 不解析正文。UI 所需基础字体的首次初始化失败保留原启动失败语义; Editor 向调用者提供明确的初始化资格或错误分类, 不通过错误文本识别。

| 已知事实 | 本次动作 | 后续结果 |
|---|---|---|
| 诊断上传额度不足, 既有正常占位可以准入 | 保留 D05-T04 的一次正常候选重建 | 属于正常背压, 不是错误 fallback |
| 基础 UI 上传暂时等待 | 结束准备并归还未录制取得 | 继续输入/退休, 下一轮重试 |
| 已就绪 UI 的普通准备错误 | 结束准备并归还取得, 记录原因 | 可恢复取消; 不用同一无效 UI 反复尝试 |
| 主图声明/Seal/Compile 失败, 尚未 Open, UI 可用且无 Fatal | 销毁候选, 构建一次安全 UI 图 | 采用安全图完整结果或取消 |
| fallback 准备/编译/硬容量失败 | 结束准备并归还取得 | 可恢复取消, 不递归 fallback |
| 已 Open 后图首次物化、recipe 或回调失败 | 停止图; 在原列表可用时 Close, 以无效颜色交接 | 获可靠前缀票据则取消并继续, 否则退出 |
| Close、提交或完成观察失败, 原生末端复制失败 | 停止新帧, 保留原 owner | 受控退出, 返回运行失败 |
| 任意阶段 Fatal | 立刻停止普通帧推进 | 根据后端健康事实退出, 不推定 device lost |

既有正常上传压力重建最多一次, 错误 fallback 在同一次取得中也最多一次。安全图不生成诊断纹理上传, 不再进入正常候选降级循环。两个有限分支显式组合, 不用无界 retry 或仅凭“尚无 Draw”判断可以重建。

### 2. 错误有拥有型快照, Fatal 贯穿宿主寿命

Platform 在已有 `RhiMessageAdapter` 中增加独立 Fatal 锁存, 在转发日志前更新。保留单调错误计数及后端原生健康状态的现有职责。Runtime 使用宿主窄查询判断停止新工作; 不扩充 NVRHI 公共 Device 错误接口。

每次候选记录准备到执行/关闭的失败事实。正常候选一旦失败便被冻结; 安全图只能在结束该候选后开启自己的准备 scope, 其执行 scope 必须包含其准备结果。首个 GraphError 的阶段/Pass/资源与有界描述在图销毁前复制, RHI 失败关联实际操作诊断, 不留 arena/string_view 借用。保留主错和一个附加失败摘要及计数即可, 不保存逐帧历史队列。

UI、图消费者准备以及 fallback 准备返回后立即检查 Fatal; Open、Close/提交和发布边界依据实际生命周期再检查。Fatal 后只执行允许的收拢, 由后端健康事实控制是否可以调用原生 API。原环境仍可用的 Fatal 不能直接释放旧提交或跳过关闭。普通 Error 不锁存 Fatal, 错误计数饱和或状态无法证明时按不可继续处理。

EditorSession 保存一份不含 GraphId/原生指针的最近错误快照, Workspace 显示阶段、原始原因与当前恢复状态。fallback 当帧在既有 Viewport 状态层标注旧输出/缺失输出; 完整详细错误最迟在下一次成功可绘制 UI 中出现, 不在同一输入轮次再次 NewFrame/Render 或重放编辑事务。重复失败替换同一有界状态, 恢复由后续正常成功说明。相较于清空全局错误或新建诊断事件总线, 这种安排保持每个候选的真实结果和现有 owner。

`Frame status` 的正文按当前视口和字体尺寸限制换行宽度, 包含恢复状态、首错及附加原因。宽度预留菜单边距及纵向滚动条空间, 超出可见高度时沿既有菜单滚动完整阅读。已有有界诊断不因显示宽度再次截短, 不增设另一份错误存储或新诊断窗口。

### 3. 正常输入与已接受输出快照分开

View 新增只读的拥有型已接受诊断输出快照入口, 仅复制 `ViewOutput` 的身份、角色、尺寸、颜色代及强引用等现有事实, 不递增代、不产生像素上传, 不改变 requested/effective。旧输出在当前 profile 中从不原地更新, 因而主图新候选失败不破坏其内容。只凭某个非空句柄或最近一次准备不能取得此资格。

Editor 的准备输入明确区分正常候选与安全 UI 的旧输出。正常模式继续核对封存请求代、View 和尺寸; 安全模式验证相同 View/角色、有效内容与采样合同, 按当前面板矩形只读显示旧像素并标注上次有效状态。安全模式不改造布局尺寸、不清除用户的新请求, 不把旧尺寸当作当前 effective 的新确认。隐藏/全部裁剪时不额外生产或上传 View 资源。

旧纹理以其已建立的 `ShaderResource` 进出状态及有效内容显式 import 到新图, 稳定 UI token 只解析为本图 TextureId。跨帧 snapshot/registry 不保存 GraphId。没有旧输出时沿既有可诊断占位上传与采样链工作。D05-T04 的驻留 token 快照及 alias 最新版本规则继续适用。

不直接调用 `View::Prepare` 来获取 fallback 输出, 因为它可能生成新候选且要求当前请求匹配; 不全局放宽 Editor 的匹配规则, 因为正常帧仍必须拒绝过期候选。对应变化见 [View delta](specs/editor-viewport-output/spec.md)。

### 4. fallback 使用独立图身份与一次性 UI 准备值

复用现有 `FrameSlot::arena`、`graphArena` 和 `FrameSlotUiFrameBindings`。主候选未 Open 才允许结束计划与图, 取消 PreparedUiFrame, 撤回未发布临时 token, 清空图绑定, 最后重置相应存储。图回调持有的准备值/回执地址必须在图销毁后才失效; 两个 arena 继续独立, 图 Rewind 不覆盖 UI 载荷。

在同一次封存 ImGui 数据上重新 Prepare, 使用明确的安全输入、旧输出强引用与原 PresentationFrame; 形成新的图代。图只声明窗口 clear、实际 UI 上传、旧输出/驻留采样、UI Raster 和原窗口颜色 export, 不重新调用失败的诊断生产或附带读回声明。复用现有编译器和上传预算计算, 队列容量仍按最终存活计划确认, 字体和其它 UI 更新仍保留真实生产依赖。

主候选取消时临时 TexID 的回退必须结束旧准备关系, fallback 可以从仍有效的 ImGui 请求重新复制拥有型载荷。UI 图声明不是上传完成, 即使两次使用同一个请求也只由成功的第二次回执确认。不能复用已 BuildGraph 的 PreparedUiFrame 或从失败的绑定表搬运 GraphId。

Runtime 可把现有正常/安全路径共享的声明、准入、执行/交接拆成有直接消费者的局部流程, 渲染声明仍归 Renderer/Editor。不开第二套生产录制路径, 不把窗口/Platform 依赖引入 RenderGraph 编译器。

### 5. 已开始执行只处理前缀, 不切换图

一旦 Open, 即使在第一个存活资源创建处失败也进入录制清理。执行器继续按已有合同停止后续 Pass、拒绝成功出口并恢复允许的 Automatic/图形状态。RHI 的 invalid 包或有 tracker 错误的包仍不可提交; 不通过清错或补一张空列表给原前缀伪造票据。

没有 Fatal、Close 未新增错误且原列表仍合法时, 用原命令列表调用 `PrepareSubmission(..., false)` 和 `SubmitPresentation`。即使已发上传成功, 也不读取/复制失败 PresentColor, 不确认本次 UI 请求。成功取得 ticket 且 Event 可靠绑定时, Present 只消耗取得而不交换, FrameSlot 记录真实票据。准备或执行的原错保持可见, 不阻止独立检查 Close/Submit 自身是否成功。

Platform 为该呈现槽记录目标需要重建, 代的状态未知不再转为全局 stopped。原 Texture/FBO、提交和 Event 在完成前保持, 其它已安全槽可继续工作。该槽下一次取得先 PollEvent, 只有 Complete 才销毁旧目标引用、按最新窗口范围成组创建新代, 新代入口为 Common。不能通过 clear 去假定一个状态未知的旧目标可以复用, 也不为恢复创建第四个槽。

对于实际 invalid 列表、Close 失败、fence/flush 拒绝、Event 失败、末端复制失败或 Fatal, 仍由现有 RHI 包/宿主关闭保活, 停止新渲染。非零 ticket 后 Event 失败时先保留 ticket 再报告失败。可恢复前缀路径仅接受现有 RHI 合法 Close/Execute 已证明的状态协议; 若其它持久导入也失去可靠状态, 不继续复用, 转受控退出。无需新增通用状态回滚机制。 [呈现 delta](specs/opengl-window-presentation/spec.md) · [现有帧槽合同](../../../specs/frame-slot-lifecycle/spec.md)

### 6. 发布资格跟随实际成功的候选

Runtime 明确当前执行的是正常候选还是安全 UI 候选。只有完整执行、Close、提交和宿主有效颜色回执匹配的正常候选能调用 View 发布; fallback 永不调用该发布入口, 即使旧输出尺寸/请求代与新请求碰巧一致。

安全图可以确认它自己实际执行的 UI 上传, 使用现有 `PreparedUiFrame::Finish` 的同图 plan、执行输出、列表及 ticket。未消费或被裁剪的请求保持 pending, 不以整批成功代替逐请求确认。主图取消或失败执行不推进上传内容版本; 下一次安全重试重新使用独立拥有的像素。持续跨 GPU 的资源由 graph/RHI 包保活, 与下一次 ImGui NewFrame 前结束请求元数据借用分开。

当前 `NoSceneOutputs` 本来不包含场景历史; fallback 继续显式保持这一事实, 不能把 UI ticket 当作“场景有效”。后续 Scene/历史/产品读回加入时需沿相同失败结果拒绝发布, 本 change 不制造不存在的历史字段或读回状态。窗口交换仍晚于有效输出接受; 交换时发生尺寸变化不撤销已接受的正常输出。 [发布 delta](specs/editor-frame-graph/spec.md#requirement-publish-only-accepted-graph-outputs)

### 7. 在当前消费者上验证阶段出口

优先复用本机仍存在的 `build/d05-t04/verify/Verify.cpp` 和 `build/d05-t04/cr-001/Cr001.cpp`, 规划时仅确认其存在并阅读, 不把旧 exe 当作新行为证据。针对改变的分支在 `build/d05-t05/` 建立有限的临时消费者/故障注入, 链接正式目标, 使用原生 GL 像素、调用次序及真实 fence 证明关键结果。测试不进入正式源码、默认 CMake 或 Runtime 参数。

验证按 tasks 中的 R1-R7 风险组合安排, 一组可以覆盖多个 spec 场景。图错误定位/状态分类可先用 CPU 证据, 旧输出采样、取消后下帧、prefix ticket、资源退休和退出使用实际后端。错误注入分别选择实际准备、资源/recipe 创建、已有命令后的回调或操作、提交边界; 不遍历所有分配点。

D05-T05 计划中的“读回准备失败”在当前阶段没有产品读回服务。用 build 下显式 Readback side effect 的临时图消费者在同一构图/准备诊断边界制造普通失败/Fatal, 验证无成功结果或后续发令, 不新增正式截图/拾取接口。若无法让该消费者经过实际公共编排边界, 该项保持待验证并说明, 不能用脱离产品门控的独立模拟冒充通过。真实 Scene 历史数值仍属后续阶段, 本次以 NoSceneOutputs、View 代及发布调用证明不误发布, 在最终证据写明界限。

## Risks / Trade-offs

- [RHI Error 与图回调错误具有不同后果] → 合法前缀可提交, invalid 包按既有协议退出; 不为恢复放宽 RHI/Validation 拒绝条件。
- [安全图沿用失败候选的 scope 或 GraphId] → 旧尝试结果冻结, 图/准备值先销毁再 reset; 新图建立自己的全程 scope 和绑定。
- [旧输出显示看似确认了新比例] → 具名安全模式及 UI 状态标识, View 发布入口由正常候选资格控制。
- [失败目标的实际末态不明确] → 只隔离受影响槽, 完成后换代; 保守放弃一次目标复用, 不声明性能收益。
- [重试临时 TexID 与像素元数据被下次 NewFrame 修改] → Cancel/Finish 先结束借用; 每次执行用拥有数据, 已发引用按 ticket 退休。
- [持续普通错误导致 UI 暂时无法绘制] → 有界错误快照和 stderr/现有日志持续可定位, 输入/关闭及有限等待仍可用; 不靠无限同帧 fallback 消耗资源。
- [Fatal 之后误调用失效 context] → 准备和执行边界读取永久停止事实, 后端健康决定退出操作; 完成失败不退化成普通 Pending。

## Migration Plan

先落实诊断结果/Fatal 门控与旧输出输入, 再接入独立安全图、取消前缀目标退休和主循环继续结果, 最后验证产品路径与文档。保留既有正常路径像素、背压和驻留快照行为; 替换后清理不再准确的“所有图错误均退出”注释, 不保留并行产品实现。

实施按 [tasks](tasks.md) 记录输入、命令、结果和限制; 维护者调度独立 B 审查/完整文件注释, A 整改后由 B 复核。B 签收最终版本后才执行最终原生 verify、同步四份主 specs、归档和当前问题单重置。D05-T05 与 D05 出口均通过后才把当前阶段移到 D06-T01。

没有持久文件格式或第三方升级。若实现中发现现有 RHI 不能支持某类已计划的普通恢复, 先用具体失败事实调整本 change 的对应合同/验收并按项目流程交接; 不改历史归档、不弱化原生校验, 不自动回滚维护者工作区。
