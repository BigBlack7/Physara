# opengl-immediate-compute Spec Delta

## Purpose

定义 OpenGL 4.6 的有限直接 Compute 执行，以及 Shader 存储访问对后续图形、计算和传输命令的同步与在途保活行为。通过同一 Graphics 即时列表提供可预测的写入可见性和执行次序。

## ADDED Requirements

### Requirement: Record direct compute dispatch on the owning graphics list

OpenGL SHALL 在拥有当前 context 的已打开 Graphics 即时列表上执行已准备的直接 Dispatch。有效 `SetComputeState` SHALL 使用同设备 Compute Pipeline 与实际绑定，切换后旧图形状态失效。切回图形、ClearState 或开始新录制后 SHALL 重新建立 Compute 状态。Dispatch SHALL 核对三个工作组数及设备上限，更新实际 volatile/push 常量范围并消费本次活动绑定。合法零工作量 SHALL 不发出原生 Dispatch、不写资源且不新增同步依赖。非法参数、缺失状态或不支持的调度 SHALL 有诊断并阻止相应工作。成功调用 SHALL 发出一次原生 Dispatch，Execute SHALL 只按既有协议提交并跟踪完成。

#### Scenario: Mix dispatch and draw without replay

- **WHEN** 在同一有效 Graphics 列表内依次 Dispatch、Draw、重新建立 Compute 状态后再 Dispatch，最后 Close/Execute
- **THEN** 每条合法命令在录制时执行一次，顺序和资源值符合输入。真实提交提供票据，Execute 不再发出三条命令

#### Scenario: Validate group bounds and harmless empty work

- **WHEN** 配置合法 Compute 状态，分别请求设备允许的最后一个 X 工作组数、紧邻超限值和含零的有效参数，并在切换 Graphics 或 ClearState 后继续请求 Dispatch
- **THEN** 仅合法的非空调用发出一次 Dispatch。超限或缺失状态有诊断，空调用不改变已定义内容、原生命令数或后继依赖

### Requirement: Make shader writes visible to every actual consumer domain

每次实际 Shader 写 SHALL 使其覆盖范围上的新写入对相关后继消费域重新需要同步。实际 Draw、Dispatch、clear、上传及 copy SHALL 在消费前满足适用于其重叠范围的可见性。Texture fetch、image、SSBO、indirect、vertex/index、uniform 和实际原生传输/附件访问 SHALL 按真实使用各自建立依赖。一个消费域已可见 SHALL 不被当作其它消费域已可见。原生绑定命中、ResourceStates 值不变或切换录制 SHALL 不遗漏尚未满足的有效访问。Automatic、已提交的 Explicit 及 GraphManaged SHALL 共用同一原生发射权威，缺失的 Explicit 或 GraphManaged 用途 SHALL 明确拒绝。有效新写或已证明覆盖的普通写只改变它实际覆盖的范围。

#### Scenario: Consume one shader write in multiple domains

- **WHEN** Compute 写入有已知数据的 image 和参数 Buffer。一条路径将同版 image 先作为 texture 采样、再走既有传输读取，另一路将同版 Buffer 先用于 SSBO 读取、再用于 indexed indirect 和 Buffer copy
- **THEN** 每个消费者都取得正确像素、索引参数或字节。每种尚未同步的消费域在首次重叠访问前得到相应依赖，先前域的成功读取不会掩盖新的需要

#### Scenario: Update and then read a partially overwritten result

- **WHEN** 同一结构化 Buffer 区间先由 Compute 写入，随后普通 Buffer update 覆盖其中一段，继续复制并消费更新段与未更新段
- **THEN** 结果同时保留最新更新的字节和剩余 Shader 结果。更新前落实必需依赖，未更新重叠区间保留其有效版本事实

#### Scenario: Carry dependencies across recordings and binding reuse

- **WHEN** 前一份即时列表的 Shader 写已进入原生命令流，后继列表在无全设备等待的情况下重新使用相同 Program 和 Buffer range
- **THEN** 新消费者看见所需数据，后继录制和绑定缓存命中都不会丢失进行中的依赖

### Requirement: Order prior overlapping accesses before subsequent shader writes

对重叠范围，后继 image 或 SSBO 写 SHALL 满足先前读写的执行次序，与 Shader 写的可见性独立处理。同态重复 `UnorderedAccess`、同一实际绑定及读后写 SHALL 按本次实际命令处理。只读到只读和可证明不重叠的访问 SHALL 不增加依赖。调用方无法用 UAV 选项移除已知冲突所需的同步。已交付路径 SHALL 使用必要的访问域同步，并保持附件与采样等非法反馈的拒绝。正常路径 SHALL 无需 `GL_ALL_BARRIER_BITS`、全设备等待或用 CPU 读回接续 GPU 数据。

#### Scenario: Preserve old reads before overlapping shader stores

- **WHEN** 两条相邻 Compute 命令使用同一份记录和同一 image 视图，一条读取旧值并留存见证，后续命令原位覆写。另安排一次相同 UAV 状态的重叠写入
- **THEN** 见证保留旧值，后继读取匹配最终写入。针对重叠的后继 Shader 写保留执行依赖，即使该资源已经对其它消费域可见

#### Scenario: Keep independent uses free of unnecessary dependencies

- **WHEN** 以同一 storage 的不相交字节或 mip/layer 执行两次访问，另以两个相同域的单纯读取作对照
- **THEN** 在这些独立用途中不产生由同对象身份或重复 Bind 触发的额外 barrier。把控制用例改为重叠读写后出现所需的域依赖

### Requirement: Keep actual compute dependencies until completion

已经发出的 Compute 命令 SHALL 沿用既有命令包和提交协议。在各自公开保活合同下，实际使用的 Program、BindingSet、存储、自有 view 和动态范围 SHALL 维持到真实完成或受控退出。关闭列表、清除逻辑状态、后续命令拒绝或无正常输出的图 SHALL 不使已发访问丢失。只有已接受提交 SHALL 发布对应状态元数据，完成确认或既有受控退出 SHALL 决定原生释放。无效、空操作和被裁剪的使用 SHALL 不成为写入依据。已发前缀发生失败时 SHALL 保留其实际访问事实与引用，后继消费者在允许继续录制时仍须满足对应依赖。

#### Scenario: Preserve issued compute on later failure

- **WHEN** 有效 Dispatch 已对 storage image 和 Buffer 发出写入，随后一个 Compute 或 graphics 候选失效，调用者释放原始引用
- **THEN** 错误沿录制或图结果返回，已发前缀在合法完成/退出前仍保有依赖，失败候选没有正常输出或虚假完成，成功退休后再释放原生对象
