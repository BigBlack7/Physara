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
