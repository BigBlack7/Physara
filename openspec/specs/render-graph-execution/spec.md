# render-graph-execution Specification

## Purpose

定义已封存的渲染图如何在现有即时 Graphics 录制中从编译计划产生原生工作。规定首次使用物化、Pass 访问与同步、外部结果交接和执行失败时的保活责任。

## Requirements

### Requirement: Execute only a matched compiled graph plan

执行入口 SHALL 接受同图代、已封存且当前空闲的原图，以及由编译入口为它发布的计划。调用方提供该设备上由 owner 线程持有、context 有效、处于 Automatic 的打开 Graphics 录制。不同图代、错误录制阶段或不具备执行条件的请求 SHALL 在分配、模式切换和回调前被拒绝。

执行 SHALL 按计划顺序逐个调用存活 Pass，保留原图的 Pass 数据及全部版本、用途和范围权限。调用方持有录制的打开、关闭、提交及完成观察职责。执行成功表示本次计划的命令与图尾准备成功，提交接受和 GPU 完成由各自票据确认。

#### Scenario: Consume the compiled order without reviving culled work

- **WHEN** 一份匹配计划含有按依赖重排的存活 Pass 和已裁剪分支
- **THEN** 每个存活回调按编译顺序调用一次，裁剪回调和仅由其使用的资源均未进入执行序列，提交由调用方随后负责

#### Scenario: Reject a mismatched graph or recording

- **WHEN** 合法计划与另一图代的封存图配对，或在没有有效打开录制时被执行
- **THEN** 执行返回可定位的入口诊断，本次请求的分配、模式切换、回调和原生命令次数均为零

### Requirement: Materialize authorized resources at first use

执行 SHALL 在计划槽的首个实际使用处分配兼容的 Texture 或 Buffer，并用导入资源的原对象建立导入映射。相容且寿命分离的逻辑资源按计划复用同一物理槽，相交寿命各用各的槽。复用 SHALL 保留真实的物理状态和访问历史，新逻辑版本的有效内容由其实写、Clear 或声明允许的保留关系建立。

执行 SHALL 先取得资源，再完成当前 Pass 所需的附件及绑定准备。准备和回调仅能解析当前声明覆盖的确切版本、用途及范围。合法请求得到已准备好的对象，解析对象不可用或 recipe 创建失败时该 Pass 不进入执行回调。Pass 内借用止于回调返回，已用对象由录制和真实完成路径保活。

#### Scenario: Reuse an allocated slot only after its preceding last use

- **WHEN** 两项同描述的瞬态资源在计划中共用一个不重叠寿命的槽，另一项存活资源的寿命重叠，图中还含有裁剪分支
- **THEN** 每个存活物理槽从其首个使用才分配，共槽资源观察到同一对象且按其真实末态准备下一用途，重叠槽使用另一个对象，裁剪分支不发生分配

### Requirement: Apply typed clears before ordinary pass uses

执行 SHALL 按计划先提交清除所需的传输用途，再以声明的类型、值和精确 mip、layer、aspect 发出清除。随后按实际 RHI 状态粒度合并并提交普通用途，才允许该 Pass 发出 Draw、上传或复制。清除以外的范围 SHALL 保持其内容。附件 Load 继续消费前版内容，StoreDontCare 只交付声明允许的内容有效性。

#### Scenario: Draw after a scoped clear and load the surviving contents

- **WHEN** 一项附件写要求清除选定范围，随后在其中绘制，下一 Pass 以 Load 保留未覆盖部分
- **THEN** 命令顺序体现清除前传输状态及 Draw 前附件状态，原生结果区分清除底色和绘制内容，Load 与未选范围保留预期值

### Requirement: Use one authority for graph and automatic synchronization

GraphManaged SHALL 由计划提供访问用途，在需要消费前提交至公共 RHI tracker 和其现有后端发射路径。Automatic SHALL 从直接 RHI 操作推导用途并经同一权威处理。GraphManaged 中 Draw、Dispatch、clear、copy 和上传 SHALL 核对已经准备的用途，缺失或冲突的用途返回错误。纹理 mip/layer 和整 Buffer 上的兼容状态合并后交付，权限、实际访问和传输范围仍保持精确。

GraphManaged 的状态登记、状态请求和提交 SHALL 由执行器持有的录制接管对象发起。Pass 借用的列表 SHALL 拒绝改写或替换当前授权。传输、活动绑定以及 Draw 可由参数确定的字节范围 SHALL 按该阶段的具体用途核对。普通附件 SHALL 只挂载声明的分量，Pipeline 对未挂载分量的测试在发出 Draw 前拒绝。

已交付图形、计算和传输 profile 的重叠 RAW、WAR、WAW SHALL 满足真实原生依赖。后端依据访问域、范围和设备规则决定所需同步。相同状态或原生绑定缓存命中仍要满足本次实际用途。只读深度附件联合采样 SHALL 仅在实际能力支持、同一追踪范围具备 `DepthRead | ShaderResource` 且 Draw 禁止深度/模板写入时成立。非法的重叠采样和可写附件 SHALL 被拒绝。

GraphManaged 的 Compute SHALL 按计划授权 Pipeline 的实际活动 storage 方向和范围。设备发射前 SHALL 检查绑定的物理存储身份和范围。绑定命中不免除本次用途。正常或失败退出 SHALL 撤销图授权、清除图形与计算状态并按既有合同恢复 Automatic，已发访问事实及其保活继续有效。

#### Scenario: Observe ordered upload sampling composition and overwrite

- **WHEN** 图依次使用上传版本进行采样、将附件产出作为后续 UI 式合成的采样输入，并在读取结束后覆写原采样范围，且两个重叠写入均有存活消费者
- **THEN** 原生读回区分旧值、合成值和最终覆写值，顺序与 RAW、WAR、WAW 计划一致，GraphManaged 期间的用途来自计划，同一依赖只有一个发射权威

#### Scenario: Sample a depth image while its attachment is read-only

- **WHEN** 给定设备支持把先前定义的深度图像同时作为只读深度附件和采样输入，并以禁用深度/模板写入的 Draw 消费它
- **THEN** 合并用途在 Draw 前有效，深度测试及采样得到预期原生值，附件内容保持。启用重叠写入的对照被拒绝

#### Scenario: Resume direct automatic uses after graph execution

- **WHEN** GraphManaged 正常返回，调用方在仍有效的录制中重新建立普通 RHI 绘制或传输状态
- **THEN** 直接消费者凭 Automatic 推导完成工作，先前图形状态不会被误用，图和直接消费者共享同一访问历史和发射权威

#### Scenario: Run compute graph uses through the existing recording authority

- **WHEN** 一张合法的图在 Compute 后消费 storage 产物并继续普通 Draw/传输，另外一次执行尝试在后续 Compute Pass 使用错误方向的有效对象
- **THEN** 合法图消费数据并按后继域满足依赖，图录制与 Automatic 共用既有发射权威。非法 Dispatch 不发出，已发前缀保活且没有失败图输出。后续允许录制时，调用方可重新建立普通状态

### Requirement: Complete declared resource handoffs at the end of execution

实际参与计划的导入 SHALL 从 owner 已建立的 entry 状态接入，在图尾对允许范围提交约定 final，并遵守列表关闭的初态恢复合同。图尾直接导出的导入同样适用。未参与计划的导入不触发状态请求。归还导入对象的 final 以其显式导入合同为准，输出的版本、范围和用途交给外部 owner 继续消费。

瞬态导出 SHALL 保活至图尾，并按导出范围准备可兑现的着色读或 CopySource 状态。若同一追踪单元有不兼容的瞬态出口请求，执行 SHALL 在发出工作前拒绝并指出冲突。所有 Pass、末端状态准备和恢复正常模式均成功后，执行 SHALL 交付请求的版本、范围、用途及可保活的实际对象。跨录制消费 SHALL 遵守实际 Close 终态和录制接受结果。呈现导出交还原 `PresentColor` 及 `ShaderResource` 状态，由窗口 owner 之后负责复制与交换。

普通采样与 ShaderRead 导出 SHALL 使用同一同格式视图能力判定。首发的 packed depth/stencil 同格式采样视图提供深度分量，声明须明确选择 Depth。

#### Scenario: Hand off used imports and retained output ranges

- **WHEN** 同一计划包括已使用但未导出的导入、延寿至图尾的瞬态输出和供宿主消费的呈现导出
- **THEN** 使用中的导入按约定范围达到 final，瞬态输出仍指向所需版本和可保活对象，呈现交还原对象，外部 owner 承担后续提交与完成职责

#### Scenario: Hand off an imported version without a pass

- **WHEN** 一份没有 Pass 的封存图直接导出有效导入版本，entry 与 final 不同
- **THEN** 执行直接完成约定范围的状态交接并返回该导入句柄、原版本和用途，回调次数为零

#### Scenario: Reject conflicting uses of one transient output

- **WHEN** 同一瞬态导出对象的同一追踪单元同时要求设备不能合并的出口用途
- **THEN** 执行在分配和命令发出前报告资源、范围及冲突用途，调用方无法取得仅兑现部分用途的结果

### Requirement: Stop on execution failure and preserve issued work

首次物化、附件/绑定准备、计划用途提交、typed clear、资源权限查询、Pass 回调或图尾准备失败时，执行 SHALL 保留首个可定位错误，停止后续 Pass 并拒绝发布成功出口。RHI 的 void 操作经调用方提供的本次诊断作用域纳入结果。已发前缀及使用的资源和上传区段 SHALL 交还录制 owner，按已接受提交的真实完成或既有受控退出协议退休。

GraphManaged 退出 SHALL 恢复 Automatic 并清理本次图形状态。仍可用的录制按命令列表合同恢复。已无效的录制仅收拢允许的 CPU 状态并维持原失败结论。设备或 context 不可用时禁止为恢复模式或状态发出原生命令。失败结果保留被拒绝的阶段以及可定位的 Pass、资源或 RHI 操作。

#### Scenario: Stop before invoking a pass with a failed allocation or recipe

- **WHEN** 某存活 Pass 首用的物理对象创建失败，或其附件/绑定准备失败
- **THEN** 首错定位受影响 Pass 或资源，该 Pass 及后续回调没有执行，调用方未取得图尾成功结果，仍有效的录制恢复 Automatic

#### Scenario: Preserve an issued prefix after a later command fails

- **WHEN** 一次原生 Draw 或上传已发出，后续 Pass 中的 RHI 操作报告 Error/Fatal
- **THEN** 执行以首错失败且停止图尾成功交接，先前资源继续由有效的录制或退出 owner 保活，既有完成条件满足后才退休

#### Scenario: Exit with native context unavailable

- **WHEN** 执行进入退出路径时设备或其 context 已确认不可用
- **THEN** 返回原执行失败，只进行合法 CPU 清理，恢复阶段的原生命令次数为零，调用方持有既有失效退出职责
