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
