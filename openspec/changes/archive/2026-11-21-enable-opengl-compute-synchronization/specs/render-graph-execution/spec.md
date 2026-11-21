# render-graph-execution Spec Delta

## MODIFIED Requirements

### Requirement: Use one authority for graph and automatic synchronization

GraphManaged SHALL 由计划提供访问用途，在需要消费前提交至公共 RHI tracker 和其现有后端发射路径。Automatic SHALL 从直接 RHI 操作推导用途并经同一权威处理。GraphManaged 中 Draw、Dispatch、clear、copy 和上传 SHALL 核对已经准备的用途，缺失或冲突的用途返回错误。纹理 mip/layer 和整 Buffer 上的兼容状态合并后交付，权限、实际访问和传输范围仍保持精确。

GraphManaged 的状态登记、状态请求和提交 SHALL 由执行器持有的录制接管对象发起。Pass 借用的列表 SHALL 拒绝改写或替换当前授权。传输、活动绑定以及 Draw 可由参数确定的字节范围 SHALL 按该阶段的具体用途核对。普通附件 SHALL 只挂载声明的分量，Pipeline 对未挂载分量的测试在发出 Draw 前拒绝。

已交付图形、计算和传输 profile 的重叠 RAW、WAR、WAW SHALL 满足真实原生依赖。后端依据访问域、范围和设备规则决定所需同步。相同状态或原生绑定缓存命中仍要满足本次实际用途。只读深度附件联合采样 SHALL 仅在实际能力支持、同一追踪范围具备 `DepthRead | ShaderResource` 且 Draw 禁止深度/模板写入时成立。非法的重叠采样和可写附件 SHALL 被拒绝。

GraphManaged 的 Compute SHALL 按计划授权 Pipeline 的实际活动 storage 方向和范围。设备发射前 SHALL 检查绑定的物理存储身份和范围。绑定命中不免除本次用途。正常或失败退出 SHALL 撤销图授权、清除图形与计算状态并按既有合同恢复 Automatic，已发访问事实及其保活继续有效。

#### Scenario: Observe ordered upload sampling composition and overwrite

- **WHEN** 图依次使用上传版本进行采样、将附件产出作为后续 UI 式合成的采样输入，并在读取结束后覆写原采样范围，且两个重叠写入均有存活消费者
- **THEN** 原生读回区分旧值、合成值和最终覆写值，顺序与 RAW、WAR、WAW 计划一致，GraphManaged 期间的用途来自计划，同一依赖只有一个发射权威

#### Scenario: Sample a depth image while its attachment is read-only

- **WHEN** 给定设备支持把先前定义的深度图像同时作为只读深度附件和采样输入，并以禁用深度/模板写入的 Draw 消费它
- **THEN** 合并用途在 Draw 前有效，深度测试及采样得到预期原生值，附件内容保持。启用重叠写入的对照被拒绝

#### Scenario: Resume direct automatic uses after graph execution

- **WHEN** GraphManaged 正常返回，调用方在仍有效的录制中重新建立普通 RHI 绘制或传输状态
- **THEN** 直接消费者凭 Automatic 推导完成工作，先前图形状态不会被误用，图和直接消费者共享同一访问历史和发射权威

#### Scenario: Run compute graph uses through the existing recording authority

- **WHEN** 一张合法的图在 Compute 后消费 storage 产物并继续普通 Draw/传输，另外一次执行尝试在后续 Compute Pass 使用错误方向的有效对象
- **THEN** 合法图消费数据并按后继域满足依赖，图录制与 Automatic 共用既有发射权威。非法 Dispatch 不发出，已发前缀保活且没有失败图输出。后续允许录制时，调用方可重新建立普通状态
