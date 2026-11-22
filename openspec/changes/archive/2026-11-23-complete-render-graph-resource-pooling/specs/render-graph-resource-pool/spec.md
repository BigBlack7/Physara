# Spec Delta

## Purpose

规定单设备 Graphics 路径中，渲染图如何在不同帧之间复用实际物化的 Texture 和 Buffer，以及容量不足、跨帧持有和 GPU 未完成时如何维持有界存储。图内仍以当前编译所得的资源寿命和精确用途安排访问。

## ADDED Requirements

### Requirement: Admit only live physical graph demand within bounded capacity

设备级图池 SHALL 按成功编译后的不同物理槽准入，分开限制 Texture 与 Buffer 合计的对象数和完整存储描述的逻辑字节。每份物理存储在同一账本只计一次，同时覆盖已承诺的未物化槽、在录制、在途、外部继续持有和空闲存储。命中已保留的槽只转移占用，裁剪的声明和外部原有导入 SHALL 不占池容量。资源逻辑字节沿用编译的完整存储计量口径。

准入 SHALL 在本次列表开放前为计划保留所需容量，且 SHALL 不提前物化槽。考虑可立即淘汰的空闲存储后，若该计划自身超出硬限制，SHALL 明确拒绝。若容量被尚未完成的使用或有效的外部持有阻挡，SHALL 报告当前压力及其来源，交帧 owner 按现有有界协议改用可纳入的计划、等待或取消。失败准入 SHALL 不留下本次预留、半成品或对存活 Pass 的调用。

#### Scenario: Reclaim idle storage before refusing a compatible plan

- **WHEN** 旧尺寸的空闲对象占据容量且新计划可以通过释放这些对象纳入，随后提交一个在空池中也超限的计划
- **THEN** 先回收可淘汰对象并接纳前一计划，首用前无该计划的新分配。后一计划在录制前拒绝并给出请求量及硬限制

#### Scenario: Keep capacity charged while work or a published owner holds it

- **WHEN** 容量由尚未完成的提交和已经完成但仍被 View、UI 或读回调用方持有的对象占据
- **THEN** 诊断能辨认在途与外部持有，不能借此超配。只有实际解除阻挡的容量才可用于新的计划

### Requirement: Reuse compatible objects only with proven lifetime and state

首次使用 SHALL 获取完整存储描述及用途相容的 Texture 或 Buffer，兼容项不存在时在已准入容量内创建。格式、尺寸、层数、mip、样本、角色、视图或状态协议不相容的对象 SHALL 保持不同。相同对象可服务同图内不相交的计划寿命。跨图重新分配给另一逻辑资源 SHALL 要求上次实际使用的 Graphics 工作已完成且当前录制、外部导出和缓存均已放弃该对象的相关持有。

在同一设备队列中显式导入先前提交的同一受保留版本读取，SHALL 服从该版本的约定范围、真实状态和队列顺序。池 SHALL 把后继访问纳入该对象的退休条件。提交接受、呈现返回、帧号推进和计时结果可用 SHALL 不证明完成。复用 SHALL 从可核实的物理状态和访问历史接续，新建的图逻辑资源内容仍由自身的完整写入或 Clear 等声明建立。已发行前缀缺少可靠票据或完成观察失败时 SHALL 保持不可复用，按设备的受控退出处理。

#### Scenario: Read an accepted output while its physical slot is still protected

- **WHEN** 生产图在同一 Graphics 队列已获接受且尚未完成，后继图显式只读导入其有效输出，另一个图请求同描述的新瞬态槽
- **THEN** 后继读取可按提交顺序消费，瞬态槽不能取走该对象。已释放外部持有且最后实际使用完成后，兼容槽才可复用该对象

#### Scenario: Preserve physical state across logical reassignment

- **WHEN** 完成并释放的对象带有实际跟踪到的末态，下一图在同描述槽内完整写入，同时另一分支请求把池内旧内容作为新逻辑资源读出
- **THEN** 合法写入以真实前态准备并可命中该对象，未初始化的逻辑读仍被拒绝

### Requirement: Evict only eligible storage and bound retained idle capacity

池 SHALL 设定空闲对象数和字节的保留上限，在容量推进和持续尺寸、格式换代时淘汰已完成且没有外部持有的项。待确认完成、仍被绑定或附件使用、状态未知或退出待收拢的存储 SHALL 不作为可借出的空闲项。无可证明安全的空闲候选时 SHALL 维持容量拒绝或背压。释放池或 recipe 的强引用 SHALL 沿已有的录制保活和 context 删除协议完成，已无效的 context SHALL 不接收新的原生回收工作。

#### Scenario: Bound repeated size and format changes

- **WHEN** 连续换代导致一批不相容对象完成，旧输出仍由先前消费者持有，池的空闲额度随后达到上限
- **THEN** 可安全淘汰的旧容量受到限制，外部仍持有或仍在途的对象保持有效。回到仍缓存的兼容描述时可复用，未缓存的描述仍受硬容量约束
