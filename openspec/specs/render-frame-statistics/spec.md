# render-frame-statistics Specification

## Purpose

让编辑器通过实际帧提交获得可定位的 CPU 阶段时间、渲染和原生操作计数、资源驻留数量及已就绪的 GPU 时长，供固定输入的验证和后续同口径测量使用。

## Requirements

### Requirement: Publish an immutable snapshot of accepted scene frame facts

Runtime SHALL 对实际接受的正常场景帧发布有界只读快照，标识对应的 Scene/View、图像范围、实际渲染配置和提交身份。快照 SHALL 记录 UI、场景与 View 准备、图编译和执行的 CPU 阶段时间，以及 Renderer 的绘制项、灯数、存活 Pass 数，RHI 实际发出的 Draw、绑定、上传（含字节）及 barrier 计数，和有明确归属及采样口径的驻留资源数量。等待及 PresentWait 如有显示 SHALL 与 CPU 工作和 GPU 时长分别标记。编辑器 SHALL 直接读取快照，隐藏统计面板时 SHALL 跳过面板构建。失败、取消和安全 UI 帧 SHALL 保留已接受场景统计的身份与值。

#### Scenario: Inspect a cold reference frame and its steady successor
- **WHEN** 参考场景首次使用需上传的资源并成功出图，随后同一场景使用已驻留资源再次成功出图
- **THEN** 统计的实际 Draw 与绑定来自对应提交，首次上传与已复用资源有可解释的差异，驻留数量与其口径一致

#### Scenario: Retain the latest accepted scene statistics during recovery
- **WHEN** 已有正常场景统计，后续只有取消、无新场景输出的列表或安全 UI 被接受
- **THEN** 当前状态可辨认，最后正常场景统计仍带原身份，未把其它工作记作新场景提交

### Requirement: Resolve GPU timing only from an available matching submission

统计 SHALL 提供一个标明范围的正常场景图 GPU 计时指标。支持的原生设备 SHALL 通过同次录制的时间戳对取得区间，后续在合法设备上下文中轮询，只有匹配的已接受区间可用后 SHALL 发布时长和其原提交身份。等待结果、计时能力不可用或当轮因容量跳过采样 SHALL 可辨认且不提供替代数值。结果读取 SHALL 非阻塞，查询所有权和待处理数量 SHALL 有界，退出沿已有设备退休合同收拢。

#### Scenario: Publish an asynchronous GPU interval for its own frame
- **WHEN** 同一正常场景图的提交已接受，GPU 结果先不可用而后可用，期间提交另一帧
- **THEN** Pending 时显示等待，Ready 时标明所测区间及其原提交，后续帧不借用旧区间时长

#### Scenario: Keep scene output usable when a timing sample cannot be reserved
- **WHEN** 原生计时能力不可用，或者有界计时池全部仍在用
- **THEN** 所对应的统计项标为不可用或跳过取样，正常场景继续，现有查询仍按各自提交生命周期退休

### Requirement: Report incremental preparation and arena use

正常接受帧的只读统计 SHALL 带 Scene、结构及内容版本、槽、本次被接受的 View 身份和提交身份。统计 SHALL 分别报告一次共享场景准备的耗时、复用或重建事实、对象、Primitive 与灯的实际更新数量、静态编码次数及各 View 的裁剪耗时与候选规模。统计 SHALL 区分 CPU 编码、实际静态 GPU 写入、View 动态写入和总原生上传。每槽的临时载荷与图 arena SHALL 提供容量、高水位及实际扩容次数，重置游标不计扩容。

事实 SHALL 由操作发生的 owner 记录。共享工作 SHALL 按帧计一次，裁剪按 View 计，失败、取消或安全 UI SHALL 保留最后有效的正常场景快照。展示 SHALL 消费已有只读统计通道，开启统计面板不改变事实计数口径或查询等待行为。

#### Scenario: Distinguish a warm frame from a local edit across slot rotation

- **WHEN** 同一场景经过三个槽暖机，两路 View 继续使用冻结输入，随后提交一次仅影响一个源槽的材质编辑
- **THEN** 快照可从实际更新与编码计数辨认零改写的暖机后帧和局部变化，共享提取只有一项，View 裁剪各自可见，arena 的高水位和扩容事实与各槽实际使用匹配

### Requirement: Report bounded GPU residency and upload reuse

已接受场景的有界只读统计 SHALL 报告几何页的分配容量、已用、空闲和待退休字节，以及能辨别页内碎片的连续空闲范围信息。材质行和绑定 SHALL 报告条目、请求、命中与资源代，纹理/环境 SHALL 报告物理驻留容量和适用条目。RHI SHALL 从实际录制及退休记录发布映射区段容量、占用与高水位、复用或原生分配，以及真实上传次数和字节。脏范围合并后的原生写入 SHALL 按实际命令计数，Renderer 的准备需求 SHALL 保持单独口径。共享物理存储的字节 SHALL 在同一口径只计一次。

暂时容量不足、确定超限及资源准备失败 SHALL 通过当前诊断报告相应操作和可用预算。失败、取消或只接受 UI 的帧 SHALL 保留最后正常场景统计身份。Editor SHALL 通过既有只读通道展示这些计数，展示开关 SHALL 不改变资源回收、上传或 GPU 等待行为。

#### Scenario: Observe reused storage and distinguish budget pressure

- **WHEN** 场景在正常帧中首次分配页及上传区段，之后复用已完成范围，并在一轮申请中先后遇到暂时占用和确定超限
- **THEN** 接受帧的快照可区分实际新分配、范围复用和原生上传，容量及高水位可解释，当前诊断区分两种拒绝原因并保留最近有效场景快照

### Requirement: Report prepared surfaces and executed draw batches with distinct units

Runtime SHALL 在正常接受场景的既有只读快照中区分所选 View 的可绘表面数、实际几何 Pass 使用的批次数及间接记录数. 表面数在该 View 工作集内计一次, 批次与记录数随实际执行的几何 Pass 计, 并可辨认 depth 和 color 的贡献. Renderer 的实例/参数上传 SHALL 按正常计划中实际执行的写入报告次数和字节, 与静态写入和每 View 常量写入区分.

整图原生 Draw、绑定和上传 SHALL 保留原采样范围, 包括该图实际执行的 UI 等消费. 一次 MDI SHALL 只计一次原生 Draw, 它所消费的记录按实际记录数在 Renderer 侧计. 绑定 SHALL 来自实际原生状态变更, 上传 SHALL 来自实际传输. Editor 面板 SHALL 标明计数单位和所属 View 或图. 失败、取消或安全 UI SHALL 保留原正常场景快照, 统计显隐 SHALL 保持现有只读行为.

#### Scenario: Reconcile one instanced command and two commands in a batch

- **WHEN** 一个接受的 View 有多个可绘实例, 一次存活颜色 Pass 以一批包含两个间接记录的 MDI 绘制, 其中一条记录消费多个实例
- **THEN** 该 View 的表面数、批次和记录数各自可核对, 该 MDI 仅增加一次原生 Draw, 绑定及实例/参数上传按各自实际变化和发生次数记录

#### Scenario: Keep batch statistics attached to the accepted work

- **WHEN** 两路候选中有分支被裁剪, 或后续只有失败候选的前缀或安全 UI 得到提交
- **THEN** 已接受的 View/图统计只包括各自实际完成的 Pass 和上传, 后续非正常场景提交保持上一正常场景的身份及统计值

### Requirement: Report graph admission and pool operation with distinct units

Runtime SHALL 通过现有只读统计提供最近编译的有界图摘要，以及图池当前的预算和占用。摘要 SHALL 标识图代、正常或安全 UI 以及可用的提交身份，并报告声明、存活、裁剪 Pass 数，实际使用的导入和出口数，以及计划的物理槽数和逻辑字节。只有接受后才标为有效接受的提交。失败及当前准入原因 SHALL 独立可辨认，最近有效的正常场景统计保持其原身份。

池 owner SHALL 报告逻辑字节及对象数的硬上限、空闲上限、当前用量和高水位，并分别提供在录制或预留、在途、外部保留及可回收的容量。总容量 SHALL 对同一物理对象计一次。若标记重叠，口径 SHALL 明确可辨。命中、实际新建、淘汰以及因压力未准入 SHALL 按事实发生计数。图内多个逻辑资源共槽和 RHI 实际 native 创建 SHALL 有独立口径。诊断 SHALL 标明逻辑预算的单位及其适用范围。

Editor SHALL 从该有界快照显示池容量、图摘要和最近原因，保持已有隐藏时跳过面板构建的行为。快照和显示 SHALL 不修改图、持有实际资源、查询阻塞结果或改变回收和命令次序。

#### Scenario: Explain reuse and pressure without replacing the last accepted scene

- **WHEN** 代表帧暖机后再次取得兼容槽，随后尺寸或格式切换使正常候选未准入并执行了独立的安全 UI 图
- **THEN** 诊断可核对存活需求、兼容命中、新建及淘汰的实际计数，显示最近候选的容量原因和安全图身份。上一有效场景的提交、图像身份与计时仍可独立识别

### Requirement: Report Shader preparation and generation status independently of scene acceptance

Shader 与原生准备的事实 owner SHALL 提供有界只读快照，包含当前 Active 代、当前候选请求及其 Pending 或 Failed 状态、程序与变体、可用源路径和行号或实际原生诊断位置。快照 SHALL 区分声明的变体规模、实际准备需求、CPU 包与原生缓存的条目和限额、命中、实际编译或创建次数以及 CPU 编译和 context 线程准备耗时。因额度顺延、压力、失败或丢弃过期结果 SHALL 有对应原因。同一时刻的准备状态及最近一次接受的场景统计 SHALL 保有各自来源。

Editor SHALL 通过现有状态或统计面板显示这些事实及原因。显示开关 SHALL 只影响面板构建，不能触发编译、原生命令、阻塞查询或改变提交及回收次序。

#### Scenario: Show a failed candidate beside a usable active generation

- **WHEN** 一次有效旧代仍在渲染，而新候选先耗尽本轮准备额度，随后出现编译或原生准备错误
- **THEN** 诊断先显示 Pending 的预算原因，失败后显示关联请求和源位置。Active 代及最近接受场景的票据独立可辨，实际执行的编译与 Pipeline 创建分别计量

#### Scenario: Observe reload without changing work through panel visibility

- **WHEN** 在相同输入和请求顺序下显示或隐藏统计面板
- **THEN** 编译、准备额度、代的发布与接受统计来源保持同一合同。面板读取快照仅产生自己的界面构建成本

### Requirement: Report current shadow planning with logical budget units

Renderer SHALL 为每个已处理 View 提供有界的只读 CPU 规划摘要，标识所属 Scene、View、当前请求和计划版本。摘要 SHALL 报告启用状态和明确原因、请求及有效质量、已分配和未准入的灯数、规划的区域数及层跨度、逻辑字节和上限、所需内容变化及缓存复用。禁用、灯删除、尺寸无效和预算不足的当前结果 SHALL 可辨认。逻辑计划的额度和用量 SHALL 以 CPU 规划单位显示。已接受的场景快照、GPU 时长及原生分配计数 SHALL 保持各自身份和采样口径。

Editor SHALL 从现有统计通道读取摘要，显隐 SHALL 仅改变面板构建。正常帧取消或只接受安全 UI 时，当前 CPU 规划摘要仍可按自身身份辨认，最后已接受的正常场景统计仍保有原值。

#### Scenario: Show planning pressure alongside the last accepted scene

- **WHEN** 一个 View 已有接受的正常场景，之后尺寸或预算使当前 CPU 规划降级，且后续帧没有新的正常场景接受
- **THEN** 面板从摘要显示本次 View、质量与额度原因，最近接受的场景仍带原票据，显示或隐藏面板时规划与提交行为一致

### Requirement: Report accepted shadow production and consumption per view

现有统计 SHALL 报告当前 View 的请求开关、过滤、请求和有效边长、有效预算、计划准入及降级原因。各路 View 的最近正常接受记录 SHALL 带自身 Scene 和提交票据，以及实际消费的分支、过滤、有效边长、使用的组及层数、实际生产及复用的层数、阴影绘制批次与配对深度存储按描述符计算的逻辑字节。当前 CPU 计划字节、已接受图使用的存储逻辑字节、池容量及原生计数 SHALL 各自标明来源和范围。失败、取消或安全 UI SHALL 保留受影响 View 上次正常接受的统计和提交身份。整图 CPU 与 GPU 计时 SHALL 各自仍对应单次提交。

Editor SHALL 沿用只读统计及非阻塞计时。显示与隐藏面板 SHALL 只改变面板构建。GPU 时间只在匹配已接受区间有结果时发布。

#### Scenario: Reconcile planned shadow pressure and accepted shadow work

- **WHEN** 受光 View 首次接受深度并以静态输入复用，随后预算不足使一组阴影被拒绝，且继任帧只有前缀或安全 UI
- **THEN** 接受帧的实际生产、消费和复用计数可与存活 Pass 及其图提交核对。当前规划显示新原因，较早已接受统计仍带原来源。轮询中的 GPU 时长保持原计时状态与身份。

#### Scenario: Keep prior shadow work for a rejected View alongside a newer graph

- **WHEN** 主 View 与离屏 View 同时接受，下一轮离屏 View 的尺寸无效且主 View 继续接受，随后离屏 View 恢复并接受无阴影颜色
- **THEN** 失败期间可同时读取主 View 的新票据、离屏 View 的旧票据及工作量，以及本轮离屏准备的拒绝原因。离屏 View 恢复后报告自己的新票据与零消费。
