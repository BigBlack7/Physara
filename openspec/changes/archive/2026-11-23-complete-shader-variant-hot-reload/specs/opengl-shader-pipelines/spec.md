# Spec Delta

## ADDED Requirements

### Requirement: Reuse native preparation by exact device and pipeline compatibility

受预算管理的 OpenGL 原生 Program 和 Pipeline 复用 SHALL 核对当前设备或 context 归属、vendor、renderer、驱动及 lowering 环境身份。Program 身份 SHALL 包含验证后的阶段与源内容、变体、ABI 和实际原生绑定映射。Pipeline 身份还 SHALL 包含完整输入布局、不可变状态、逻辑布局与公开目标兼容信息。等价目标的尺寸及附件对象身份 SHALL 不参与兼容键。缓存 SHALL 以全部有效键字段确认命中，设备、驱动或兼容规则变化时 SHALL 重新核对并准备相应结果。

原生缓存 SHALL 维持可观测的条目限额和压力结果。仍由有效候选、Active 计划、冻结帧或原生包使用的对象 SHALL 保持其寿命和兼容身份，具备淘汰条件的条目才可归还缓存容量。成功的新增原生结果 SHALL 来自有 owner context 的真实 lowering、编译、link 和 Pipeline 验证，准备失败 SHALL 交回可定位原因并收拢临时对象。

#### Scenario: Reuse a pipeline for another compatible framebuffer

- **WHEN** 两个视图的 framebuffer 尺寸和存储对象不同，当前设备、程序及完整 Pipeline 兼容键相同
- **THEN** 第二次请求命中已准备的对象，省去再次 link 和 Pipeline 创建，两视图仍使用各自的 framebuffer

#### Scenario: Miss an incompatible native preparation key

- **WHEN** 阶段内容、活动资源映射、输入布局或目标兼容事实改变，或者缓存查找的设备驱动身份与写入时不符
- **THEN** 原生准备拒绝旧键命中，交付匹配的新结果或报告 Pending 或失败原因。编译包身份仍等价时可复用原 CPU 包

#### Scenario: Preserve live entries at the native cache limit

- **WHEN** 缓存额度已被 Active、候选或尚未完成的工作占用，额外请求无法准入
- **THEN** 准备保持有界并报告压力，已有效的对象继续可用。真实完成和相关持有解除后，后继请求可重新取得容量
