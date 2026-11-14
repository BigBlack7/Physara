# Spec Delta

## Purpose

定义如何把已封存的帧内资源声明编译为确定且可检查的执行计划。计划为执行器交付存活的工作、内容和存储顺序、资源寿命、受预算约束的物理分配，以及执行所需的用途和外部边界。

## ADDED Requirements

### Requirement: Resolve dependencies from the content of exact resource versions

编译 SHALL 按访问请求的逻辑版本和精确范围确定内容来源。一个请求可同时依赖多个生产者。只继承未被较新写入覆盖的部分，以 mip、layer、aspect 和缓冲字节区间的实际交集建立 RAW 内容依赖。`Load` 和保留式更新 SHALL 读取其所选范围的前一版本。`Clear`、`Discard` 或完整覆盖按声明的效果解释，只有声明了读取旧内容的操作才建立相应的内容边。

编译 SHALL 拒绝传输操作无法兑现的 aspect 范围。首发配置的普通纹理复制和读回须覆盖 packed depth/stencil 的两个 aspect。支持按 aspect 选择的 typed clear 仍可分别定义深度和模板。

导入版本 0 的初始内容来自明确的有效性声明。图新建的资源初始无内容。已丢弃、未初始化或仅部分定义的范围仍不能完整读取。声明拒绝的访问 SHALL 遵守图声明合同，未发布的访问不能进入计划。编译发现已发布访问所需内容未定义时 SHALL 返回含资源、版本和原因的错误。

#### Scenario: Trace a read to multiple surviving writes

- **WHEN** 不同 Pass 分别初始化同一纹理的两个 mip 或同一缓冲的两个相邻字节区间，另一个 Pass 只写入读取范围以外的部分，随后消费者读取指定版本的两个已写部分
- **THEN** 两个实际生产者均排在消费者之前，范围以外的写入没有其它消费者时被裁剪，计划中的读取仍指向所声明的版本和范围

#### Scenario: Refuse content that was discarded or never defined

- **WHEN** 输入尝试读取未初始化的字节，或在仅补写部分范围后 Load 或导出仍含有 `StoreDontCare` 内容的范围
- **THEN** 所需内容被报告为未定义，该访问无法进入执行计划，明确写满缺失范围的合法输入可编译

### Requirement: Preserve overlapping storage order and reject illegal feedback

同一资源的多个逻辑版本 SHALL 共享该资源的存储身份。编译 SHALL 为同一资源的重叠写入保留版本先后关系（WAW），并把对旧内容的读取排在会覆盖所读范围的较新写入之前（WAR）。两种约束都必须保留在仍会执行的 Pass 之间。只读访问之间和不相交的访问本身不形成存储约束。

同一 Pass 的独立读与写或两次独立写，在访问同一重叠范围且需要 Pass 内先后顺序时 SHALL 被拒绝。附件 `Load` 作为该次写入的旧内容读取。只读深度附件与兼容采样可同时声明，前提是所选范围无重叠写入且给定能力支持联合用途。编译 SHALL 在裁剪前检查已声明的 Pass 依赖。依赖成环或需要在覆盖后读取同一存储的旧内容时 SHALL 拒绝并指出涉及的 Pass 和原因。失败只交付诊断。

在同一普通阶段共享一个 RHI 状态单元的用途 SHALL 能同时满足。首发配置按纹理 mip/layer 和整 Buffer 检查状态组合，并按各自的精确范围追溯内容。清除步骤先独立准备状态。同一 packed mip/layer 上的采样与可写深度/模板附件 SHALL 被拒绝。

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

### Requirement: Cull work from explicit output and side effect roots

活性 SHALL 从图尾的明确导出和具名类型的图内副作用开始，仅沿内容依赖保留所需生产者。首发图内副作用为读回，并关联该 Pass 已声明的 `CopySource` 读取。无对应读取的标记 SHALL 被拒绝。普通上传和写入只有在其结果被根或存活 Pass 消费时才保留。WAR 或 WAW 约束本身不能让一个 Pass 存活。

成功计划 SHALL 对存活 Pass 按依赖做稳定拓扑排序。多个 Pass 同时可运行时按声明索引升序选取。相同的封存输入、能力与预算 SHALL 产生相同的顺序和物理计划。没有根且有效的图允许返回空执行序列。

#### Scenario: Keep a typed readback and cull a superseded upload

- **WHEN** 一个上传被存活的完整写入覆盖且无其它消费者，另一个读回 Pass 声明了明确的读回副作用和 `CopySource`，但没有图内输出
- **THEN** 被覆盖的上传被裁剪，读回 Pass 与其内容生产者保留，裁剪的 Pass 不占用计划资源

#### Scenario: Use declaration order for equally ready live passes

- **WHEN** 多条互不依赖的有效分支均通向输出，多个存活 Pass 同时可运行
- **THEN** 编译按声明索引打破并列，重复编译的执行顺序与资源安排一致

### Requirement: Keep exported ranges available at the end of the plan

导出 SHALL 保留所声明的版本、范围和外部用途，并把该范围的图内存储保留到图尾交接。若仍会执行的较新写入覆盖导出所需的任何部分，编译 SHALL 报告导出版本和冲突写入并拒绝计划。无活性的写入和不相交的写入不能使该导出失效。有效的导入版本可直接导出，计划 SHALL 保留其外部身份、允许范围以及进出状态。

纹理 `ShaderRead` 导出 SHALL 具备所选范围的同格式采样视图能力。`Presentation` 导出 SHALL 满足宿主要求的完整结束状态。首发 `PresentColor` 的结束状态为 `ShaderResource`。

#### Scenario: Export an old version only while its content survives

- **WHEN** 输出指定旧版本，较新写入在一个图中被裁剪，在另一个图中因独立输出而存活并覆盖该范围
- **THEN** 前者交接所请求的旧版本，后者报告冲突并拒绝计划

### Requirement: Plan compatible physical storage within a transient budget

编译 SHALL 依据存活 Pass 的有序访问和图尾导出计算每项使用中的资源的首末使用。图内复用 SHALL 仅发生于存储描述和用法兼容、使用区间不重叠的瞬态资源之间。导入使用原对象，导入的外部存储不进入瞬态复用计划。复用后的逻辑资源仍须自行建立有效内容，物理对象的实际状态沿执行交接跟踪。

瞬态预算 SHALL 按计划中的不同物理槽累计逻辑存储字节。缓冲按字节容量，纹理按全存储的格式块、mip、层或深度与样本计量。不能表示的估算和超出调用方预算的计划 SHALL 返回错误。该预算约束计划用量，原生分配与执行的实际结果由执行器报告。

#### Scenario: Reuse a compatible slot only after the last use

- **WHEN** 两个同描述的瞬态资源在有序计划中先后使用，另有一个同描述资源已在后者首用时处于使用中，或在此之前已写入且仍须保留到图尾
- **THEN** 先后使用的资源可共享物理槽，同时使用的资源分开分配，预算按各槽计一次

#### Scenario: Reject a plan that cannot fit the transient budget

- **WHEN** 存活资源需要的物理槽总规划字节超过提供的预算，或所需字节不可表示
- **THEN** 编译返回计划容量错误，不交付分配计划。字节数可表示且其它条件满足时，同一封存图以恰好足够的预算可重新编译

### Requirement: Compile a complete CPU-only execution plan

编译 SHALL 接受已封存的图与明确的能力、预算输入，拒绝尚未封存的图。编译 SHALL 在裁剪前核对声明用途所需的描述、格式、样本和附件视图兼容性，能力不足或不合法的组合须指出 Pass 或资源及失败原因。失败只返回诊断。成功 SHALL 交付同图代的完整计划，包括存活顺序、资源映射与首末使用、附件及资源视图信息、范围明确的清除与后续访问，以及实际使用的导入进出状态和外部输出。清除所需用途 SHALL 排在其后实际消费前。

能定位到声明的编译失败 SHALL 在有界诊断中保留原因和声明索引。资源诊断含其类型，调试名称可用于补充说明。

Compile SHALL 在 CPU 上完成，成功或失败均不得调用 Pass 回调、物化设备资源、录制命令、提交或操作窗口。

#### Scenario: Compile the editor frame declarations into ordered uses

- **WHEN** 已封存的基线图声明诊断图及字体上传、UI 缓冲写入、`PresentColor` 清除、合成和呈现导出
- **THEN** 计划保留上传到采样及缓冲读取的关系、清除用途与合成 Load 的顺序，并在图尾标出合成版本及导入的结束状态，编译不产生设备工作

#### Scenario: Reject unsupported formats or mismatched attachments

- **WHEN** 给定能力缺少声明用途所需的格式或样本支持，或同一 Pass 的附件视图在所选 mip 上尺寸或样本不兼容
- **THEN** 编译指出不受支持的用途或不匹配的附件并返回错误
