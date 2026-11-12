# Frame-slot lifecycle delta

## Purpose

规定 Runtime 如何以真实提交完成复用有限的 CPU 帧存储和窗口目标，并在暂停、取消、失败和退出时保持已发工作及其依赖的寿命。

## ADDED Requirements

### Requirement: Prepare bounded frame-owned CPU data

每个 FrameSlot SHALL 为实际 UI 几何和拥有型上传数据提供可复用的 FrameArena。帧内分配 SHALL 在返回借用前检查大小、对齐、算术溢出和容量边界。失败 SHALL 返回有界原因并保持已成功分配的内容及范围有效。借用 SHALL 限于本次槽使用及其有效准备阶段，跨 ImGui 下一次 NewFrame 使用的数据 SHALL 具有独立所有权。

#### Scenario: Allocate aligned ranges inside the frame budget
- **WHEN** 真实准备工作需要连续的不同对齐的几何及上传区段，且包含对齐填充的范围恰好落在可用容量内
- **THEN** 返回满足对齐、互不重叠的区段，独立持有的数据不随原输入的修改而变化

#### Scenario: Reject an invalid or oversized range
- **WHEN** 分配的大小计算或对齐无法表示，或计入填充后越过可用容量
- **THEN** 拒绝该分配并保留之前的分配和游标，失败准备不能被当作可录制批次

#### Scenario: Reject a prepared borrow after its lifetime ends
- **WHEN** 旧准备值已经结束或取消，槽经安全回收后被用于下一帧
- **THEN** 旧准备值不能录制、确认或引用下一次使用的数据，即使底层存储地址被复用

### Requirement: Reuse a fixed number of slots after real completion

Runtime SHALL 使用固定上限的 FrameSlot，初始上限为 3。每个获接受的有效提交或已发前缀 SHALL 记录该槽的非零设备票据，帧内引用 SHALL 保留至真实完成或受控收拢。只有从未发出工作、已确认无工作取消或已证明上次票据完成的槽才能重新分配帧内存储。槽均在途时 SHALL 以有限事件等待维持有界背压，并可独立观察槽忙与窗口交换等待。完成观察或绑定失败 SHALL 阻止复用并保留未证明安全的工作。

#### Scenario: Apply backpressure until a slot actually completes
- **WHEN** 上限内所有槽的完成观察仍为 Pending，即使 CPU 帧号已经前进或交换已经返回
- **THEN** 本轮没有新 GPU 帧、替代槽或在途槽重置，事件继续推进，只有获得真实完成证明的槽才能再次用于录制

#### Scenario: Retain an accepted prefix without publishing success
- **WHEN** 失败或取消的帧已有合法 GPU 工作且该前缀获得有效票据
- **THEN** 该槽及其依赖仍按此票据退休，失败帧的颜色、上传确认和 View 历史不被发布为成功

#### Scenario: Stop reuse when completion cannot be proved
- **WHEN** 已发工作缺少可靠票据或对应的完成观察，或该观察返回失败
- **THEN** 停止新 GPU 帧，相关槽的帧内存储不能 reset 或改记为可用，已发工作沿设备受控关闭处理，首个原因仍可观察

### Requirement: Keep input responsive during suspension and retire on exit

窗口最小化或 framebuffer 为零时 Runtime SHALL 先处理事件及输入清理，跳过正常帧槽取得、UI 帧构建与新提交。正常运行中的回收 SHALL 在所属线程和有效 context 上推进。收到关闭或达到 poll limit 时 SHALL 停止发出新帧，按设备的完成与关闭协议处理最后的已接受或未获可靠票据的访问。

#### Scenario: Resume from a suspended window with current metrics
- **WHEN** 窗口在有未退休工作的情况下最小化，并以新的有效像素范围恢复
- **THEN** 暂停期间不为新帧占槽或分配替代目标，事件和非阻塞回收继续，恢复后的正常帧使用当前尺寸及可安全取得的槽

#### Scenario: Close with the last frame still outstanding
- **WHEN** 用户关闭窗口或达到 poll limit 时仍有在途使用
- **THEN** 结束待用的帧准备并停止提交，保活涵盖最后的实际访问，正常关闭等待真实设备收拢，失败则按既有退出协议保留主因
