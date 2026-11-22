# Spec Delta

## MODIFIED Requirements

### Requirement: Materialize authorized resources at first use

执行 SHALL 在计划槽的首个实际使用处取得兼容的 Texture 或 Buffer，并用导入资源的原对象建立导入映射。相容且寿命分离的逻辑资源按计划复用同一物理槽，相交寿命各用各的槽。复用 SHALL 保留真实的物理状态和访问历史，新逻辑版本的有效内容由其实写、Clear 或声明允许的保留关系建立。

执行 SHALL 先取得资源，再完成当前 Pass 所需的附件及绑定准备。准备和回调仅能解析当前声明覆盖的确切版本、用途及范围。合法请求得到已准备好的对象，解析对象不可用或 recipe 创建失败时该 Pass 不进入执行回调。Pass 内借用止于回调返回，已用对象由录制和真实完成路径保活。

使用设备级池的执行 SHALL 消费为同一设备和计划取得的准入容量。匹配的空闲对象按其实际前态进入录制，需要创建的对象在首次使用才物化。裁剪的槽和未走到的首用 SHALL 不创建对象，执行完成或放弃时 SHALL 结清未用预留。

#### Scenario: Reuse an allocated slot only after its preceding last use

- **WHEN** 两项同描述的瞬态资源在计划中共用一个不重叠寿命的槽，另一项存活资源的寿命重叠，图中还含有裁剪分支
- **THEN** 每个存活物理槽从其首个使用才分配，共槽资源观察到同一对象且按其真实末态准备下一用途，重叠槽使用另一个对象，裁剪分支不发生分配

#### Scenario: Use an admitted pool slot at first use

- **WHEN** 池已为编译计划保留一个兼容对象和一个待创建槽，执行在后者的首用之前失败
- **THEN** 前一槽沿原状态按计划借用，后一槽从未创建且其预留退回。已实际使用的存储依录制和退休合同继续保护

### Requirement: Complete declared resource handoffs at the end of execution

实际参与计划的导入 SHALL 从 owner 已建立的 entry 状态接入，在图尾对允许范围提交约定 final，并遵守列表关闭的初态恢复合同。图尾直接导出的导入同样适用。未参与计划的导入不触发状态请求。归还导入对象的 final 以其显式导入合同为准，输出的版本、范围和用途交给外部 owner 继续消费。

瞬态导出 SHALL 保活至图尾，并按导出范围准备可兑现的着色读或 CopySource 状态。若同一追踪单元有不兼容的瞬态出口请求，执行 SHALL 在发出工作前拒绝并指出冲突。所有 Pass、末端状态准备和恢复正常模式均成功后，执行 SHALL 交付请求的版本、范围、用途及可保活的实际对象。跨录制消费 SHALL 遵守实际 Close 终态和录制接受结果。呈现导出交还原 `PresentColor` 及 `ShaderResource` 状态，由窗口 owner 之后负责复制与交换。

普通采样与 ShaderRead 导出 SHALL 使用同一同格式视图能力判定。首发的 packed depth/stencil 同格式采样视图提供深度分量，声明须明确选择 Depth。

使用受保留存储的交接 SHALL 随输出提供原物理对象、精确范围、有定义的内容、用途和可归属本次录制的状态事实。图尾状态 SHALL 标为该阶段的事实。跨录制交接 SHALL 在对应工作 Close 且提交可信后取得实际终态，计入 Close 恢复和图退出后的合法访问。纹理的状态回传 SHALL 保留 mip/layer 差异，Buffer SHALL 回传整对象状态。状态未知的范围 SHALL 不发布为可重新导入。

View 显示输出、历史和读回 SHALL 通过对应 owner 持有实际输出存储。后继图 SHALL 为匹配的输出代建立新的图内导入。已接受的 View/历史保留版本 SHALL 可在同队列依次读取，且在仍有合法持有和在途读取时保持其声明的有效内容。历史调用方要求成对交付时，颜色和深度 SHALL 来自同一 View、兼容的尺寸和格式代及同一次接受工作，缺项不推进该对。Readback 结果仍 SHALL 等待实际复制所属的接受提交及其真实完成。无可靠接受、执行失败或取消 SHALL 不发布新输出，已有前缀的安全保活继续成立。

#### Scenario: Hand off used imports and retained output ranges

- **WHEN** 同一计划包括已使用但未导出的导入、延寿至图尾的瞬态输出和供宿主消费的呈现导出
- **THEN** 使用中的导入按约定范围达到 final，瞬态输出仍指向所需版本和可保活对象，呈现交还原对象，外部 owner 承担后续提交与完成职责

#### Scenario: Hand off an imported version without a pass

- **WHEN** 一份没有 Pass 的封存图直接导出有效导入版本，entry 与 final 不同
- **THEN** 执行直接完成约定范围的状态交接并返回该导入句柄、原版本和用途，回调次数为零

#### Scenario: Reject conflicting uses of one transient output

- **WHEN** 同一瞬态导出对象的同一追踪单元同时要求设备不能合并的出口用途
- **THEN** 执行在分配和命令发出前报告资源、范围及冲突用途，调用方无法取得仅兑现部分用途的结果

#### Scenario: Reimport an output using the verified end of its recording

- **WHEN** 图导出有效范围，随后合法外部用途或 `keepInitialState` 的 Close 恢复改变其实际跨列表入口，生产列表在同一队列获接受
- **THEN** 下一图只导入该代允许的已定义范围，采用经核实的真实终态。不能以图尾状态替代终态，也不能把其它子资源登记成已定义

#### Scenario: Retain the last accepted pair when a newer generation fails

- **WHEN** 此前接受了同一 View 的一对历史资源，新候选 resize 或改变格式后仅有部分工作获得前缀票据，同时旧版本仍有按队列顺序声明的读取
- **THEN** 旧对仍可按原身份取得，失败候选无法替换它。独立持有者和前缀各自保护其对象，真正解除持有和完成前池不能把仍存活的存储分给另一版本

### Requirement: Stop on execution failure and preserve issued work

首次物化、附件/绑定准备、计划用途提交、typed clear、资源权限查询、Pass 回调或图尾准备失败时，执行 SHALL 保留首个可定位错误，停止后续 Pass 并拒绝发布成功出口。RHI 的 void 操作经调用方提供的本次诊断作用域纳入结果。已发前缀及使用的资源和上传区段 SHALL 交还录制 owner，按已接受提交的真实完成或既有受控退出协议退休。

GraphManaged 退出 SHALL 恢复 Automatic 并清理本次图形状态。仍可用的录制按命令列表合同恢复。已无效的录制仅收拢允许的 CPU 状态并维持原失败结论。设备或 context 不可用时禁止为恢复模式或状态发出原生命令。失败结果保留被拒绝的阶段以及可定位的 Pass、资源或 RHI 操作。

失败图的容量与状态 SHALL 按实际发生的访问处理。能证明未写入或未发出访问的预留 SHALL 归还。具有可靠 Close 状态和真实退休票据的对象 SHALL 在完成且没有剩余持有后才可回到空闲。状态无法验证的对象 SHALL 保持不可复用，只有满足原有保活及删除条件时才能释放。

#### Scenario: Stop before invoking a pass with a failed allocation or recipe

- **WHEN** 某存活 Pass 首用的物理对象创建失败，或其附件/绑定准备失败
- **THEN** 首错定位受影响 Pass 或资源，该 Pass 及后续回调没有执行，调用方未取得图尾成功结果，仍有效的录制恢复 Automatic

#### Scenario: Preserve an issued prefix after a later command fails

- **WHEN** 一次原生 Draw 或上传已发出，后续 Pass 中的 RHI 操作报告 Error/Fatal
- **THEN** 执行以首错失败且停止图尾成功交接，先前资源继续由有效的录制或退出 owner 保活，既有完成条件满足后才退休

#### Scenario: Exit with native context unavailable

- **WHEN** 执行进入退出路径时设备或其 context 已确认不可用
- **THEN** 返回原执行失败，只进行合法 CPU 清理，恢复阶段的原生命令次数为零，调用方持有既有失效退出职责
