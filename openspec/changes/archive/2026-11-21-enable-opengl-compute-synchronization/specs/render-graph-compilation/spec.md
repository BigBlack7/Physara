# render-graph-compilation Spec Delta

## MODIFIED Requirements

### Requirement: Preserve overlapping storage order and reject illegal feedback

同一资源的多个逻辑版本 SHALL 共享该资源的存储身份。编译 SHALL 为同一资源的重叠写入保留版本先后关系（WAW），并把对旧内容的读取排在会覆盖所读范围的较新写入之前（WAR）。两种约束都必须保留在仍会执行的 Pass 之间。只读访问之间和不相交的访问本身不形成存储约束。

同一 Pass 的独立读与写或两次独立写，在访问同一重叠范围且需要 Pass 内先后顺序时 SHALL 被拒绝。附件 `Load` 作为该次写入的旧内容读取。只读深度附件与兼容采样可同时声明，前提是所选范围无重叠写入且给定能力支持联合用途。编译 SHALL 在裁剪前检查已声明的 Pass 依赖。依赖成环或需要在覆盖后读取同一存储的旧内容时 SHALL 拒绝并指出涉及的 Pass 和原因。失败只交付诊断。

在同一普通阶段共享一个 RHI 状态单元的用途 SHALL 能同时满足。首发配置按纹理 mip/layer 和整 Buffer 检查状态组合，并按各自的精确范围追溯内容。清除步骤先独立准备状态。同一 packed mip/layer 上的采样与可写深度/模板附件 SHALL 被拒绝。

单条 storage 读改写 SHALL 作为同时消费输入和产出输出的明确内容操作，并参加已有 RAW/WAR/WAW 推导。完整 storage 覆写 SHALL 只由确实使用的资源内容和已有约束确定活性。一个 Dispatch 内声明的独立活动写或 sample/image 反馈在共享重叠存储且无法定义顺序时 SHALL 按同一冲突规则拒绝。

#### Scenario: Schedule a late-declared old reader before the overwrite

- **WHEN** 一个读取旧版的 Pass 声明在覆写它的 Pass 之后，二者均被外部输出保留，且读取没有依赖覆写者的其它结果
- **THEN** 旧内容的生产者先于读取，读取先于重叠覆写，读取仍指向原版本

#### Scenario: Preserve live write order across a culled overwrite

- **WHEN** 首次写入所在 Pass 由另一资源的输出保留，末次写入由该范围的最终导出保留，中间重叠写入没有消费者。首次写入所在 Pass 还须等待另一资源的旧版读取
- **THEN** 中间写入被裁剪，较晚的存活写入仍排在较早的存活写入之后

#### Scenario: Reject a cycle that would require old content after overwrite

- **WHEN** 旧版读取还依赖覆写 Pass 在另一资源上生产的内容，导致读旧内容与取得新结果所需的顺序相反
- **THEN** 编译返回可定位的环或覆盖诊断，不交付声明顺序或部分顺序

#### Scenario: Distinguish attachment load from sampling feedback

- **WHEN** 一个 Pass 既采样某纹理范围又把同一范围作为可写附件，另一个合法图用附件 Load 保留旧颜色或以只读深度附件和兼容采样读取同一深度范围
- **THEN** 编译拒绝重叠采样写反馈，保留合法 Load 的内容依赖和受支持的联合只读用途

#### Scenario: Order compute writes with live readers and cull unused work

- **WHEN** 一个 Compute 写的存活 image 或 Buffer 有多个不同用途的消费者，较晚 Compute 还要覆写重叠范围。另一独立写只被未导出的分支读取
- **THEN** 保留图中旧内容的读取排在覆写之前，存活消费者引用确定版本，未使用分支被裁剪。无法满足的同 Pass 重叠冲突或旧版本环被拒绝

### Requirement: Compile a complete CPU-only execution plan

编译 SHALL 接受已封存的图与明确的能力、预算输入，拒绝尚未封存的图。编译 SHALL 在裁剪前核对声明用途所需的描述、格式、样本和附件视图兼容性，能力不足或不合法的组合须指出 Pass 或资源及失败原因。失败只返回诊断。成功 SHALL 交付同图代的完整计划，包括存活顺序、资源映射与首末使用、附件及资源视图信息、范围明确的清除与后续访问，以及实际使用的导入进出状态和外部输出。清除所需用途 SHALL 排在其后实际消费前。

能定位到声明的编译失败 SHALL 在有界诊断中保留原因和声明索引。资源诊断含其类型，调试名称可用于补充说明。

Compile SHALL 在 CPU 上完成，成功或失败均不得调用 Pass 回调、物化设备资源、录制命令、提交或操作窗口。

Compute storage 声明 SHALL 使用传入的 Buffer 对齐、范围及 Texture 目标/格式/方向能力。Compute 用途 SHALL 在计划中保留访问范围和读写方向，按其公共资源状态交给执行，编译 SHALL 不创建 Program、BindingSet 或 native 同步命令。

#### Scenario: Compile the editor frame declarations into ordered uses

- **WHEN** 已封存的基线图声明诊断图及字体上传、UI 缓冲写入、`PresentColor` 清除、合成和呈现导出
- **THEN** 计划保留上传到采样及缓冲读取的关系、清除用途与合成 Load 的顺序，并在图尾标出合成版本及导入的结束状态，编译不产生设备工作

#### Scenario: Reject unsupported formats or mismatched attachments

- **WHEN** 给定能力缺少声明用途所需的格式或样本支持，或同一 Pass 的附件视图在所选 mip 上尺寸或样本不兼容
- **THEN** 编译指出不受支持的用途或不匹配的附件并返回错误

#### Scenario: Refuse a compute storage use outside supplied capability

- **WHEN** CPU 编译器收到合法的版本边，但设备能力快照缺少所选 mip/layer 和 image 写入方向对应的已交付能力，或者 Buffer 区间无法合法绑定
- **THEN** 编译给出资源及 Pass 所在位置的能力/范围诊断，不交付有缺失用途的计划或执行被拒绝的分支
