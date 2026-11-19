# Spec Delta

## MODIFIED Requirements

### Requirement: Reuse scene records only within a safely acquired slot

SceneFrame 及其共享资产 SHALL 由当前安全取得的 FrameSlot 支持，借用以该次槽使用为界。持久的对象、Primitive、灯记录及索引 SHALL 带 Scene 身份、结构代和已提取版本，并跨临时 arena 重置保留。键和版本吻合时 SHALL 复用记录。Scene 身份和结构代不变时，SHALL 按合并变更只更新受影响的记录及相关派生量。结构代或 Scene 身份变化时 SHALL 重建对应槽的记录与索引。包含可见有界几何的聚合范围 SHALL 随实际变更更新，支持无显式远面的 View 取得当前稳定裁剪范围。

每个槽 SHALL 保留自己尚未消费的有界变更，通过对应的安全取得追到最新已接受 Scene 状态。同一轮合法 View 请求 SHALL 共享一次只读 SceneFrame 准备结果，图重试 SHALL 继续消费该轮冻结结果。一个槽中的准备、取消和重置 SHALL 保持其他未回收槽的记录和固定资产有效。提取失败 SHALL 返回有界原因，准备值只交付完整帧，失败版本 SHALL 可安全重试。

#### Scenario: Rotate slots across camera motion and scene replacement

- **WHEN** 两个槽已提取同一 Scene，纯相机移动后其中一个槽先安全复用，随后内容改变或换成 revision 数值相同的新 Scene
- **THEN** 纯相机移动复用安全槽的静态记录但按当前相机准备可见索引，局部内容变化只更新相关记录，不同 Scene 身份触发重建，另一槽的已有记录和资产引用仍有效

#### Scenario: Catch up sparse changes in different slot orders

- **WHEN** 三个槽已完成首次提取，槽轮转之间反复修改同一对象的世界变换、一个源槽的材质覆盖和一个灯，还重置一个已取得槽的临时 arena
- **THEN** 各槽在真实可用后才按最终状态更新对应记录，只读快照及索引在各自使用期内有效，合并容量随受影响记录而非编辑次数增长，暖机后冻结输入的重建与改写计数均为零

#### Scenario: Rebuild structural indices and track shrinking drawable bounds

- **WHEN** 同一槽先观察到最远边界对象内移或隐藏，随后发生有界几何增删并进入空场景
- **THEN** 局部变更更新聚合范围及无限远透视的有限裁剪依据，结构变化后索引只引用当前记录，空场景可交付完整的空记录集合并进入现有无场景处理
