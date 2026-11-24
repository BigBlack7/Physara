# render-graph-execution Spec Delta

## MODIFIED Requirements

### Requirement: Complete declared resource handoffs at the end of execution

实际参与计划的导入 SHALL 从 owner 已建立的 entry 状态接入，在图尾对允许范围提交约定 final，并遵守列表关闭的初态恢复合同。图尾直接导出的导入同样适用。未参与计划的导入不触发状态请求。归还导入对象的 final 以其显式导入合同为准，输出的版本、范围和用途交给外部 owner 继续消费。

瞬态导出 SHALL 保活至图尾，并按导出范围准备可兑现的着色读或 CopySource 状态。若同一追踪单元有不兼容的瞬态出口请求，执行 SHALL 在发出工作前拒绝并指出冲突。所有 Pass、末端状态准备和恢复正常模式均成功后，执行 SHALL 交付请求的版本、范围、用途及可保活的实际对象。跨录制消费 SHALL 遵守实际 Close 终态和录制接受结果。呈现导出交还原 `PresentColor` 及 `ShaderResource` 状态，由窗口 owner 之后负责复制与交换。

普通采样与 ShaderRead 导出 SHALL 使用同一同格式视图能力判定。首发的 packed depth/stencil 同格式采样视图提供深度分量，声明须明确选择 Depth。具备显式只读层组交接能力的二维数组 SHALL 可将同一已定义范围作为不可变 ShaderRead 输出交接，其中每个层均须通过单层同格式采样能力核对。后继图的实际采样 SHALL 继续满足单个视图允许的层数和本 Pass 的范围权限。

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

#### Scenario: Retain an immutable group of individually sampled array layers

- **WHEN** 设备能力仅支持单层采样视图且登记了数组的只读层组交接能力，一张图导出多个已完整定义的层，后续图只读导入该范围
- **THEN** 各已定义层可按自己的授权独立采样。一次采样超出单视图能力、导出包含未定义层或请求层组 owner 写权限时，图拒绝相应声明或计划
