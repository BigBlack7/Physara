# Spec Delta

## MODIFIED Requirements

### Requirement: Execute baseline graphics through the existing immediate recording

受支持的 clear、图形状态、直接 Draw、固定数量的 indexed indirect Draw 与 copy SHALL 在现有 owning/current Graphics 即时列表的合法 recording 中执行, 与上传共享唯一打开列表和已有提交协议. 原生绘制 SHALL 在录制调用时发出, Execute SHALL 不重放已经发出的命令. 新 recording SHALL 无上一轮逻辑图形状态. ClearState SHALL 清除逻辑绘制状态并失效相关原生缓存, 同时保留实际资源依赖与已发工作的 owner.

#### Scenario: Mix uploads and graphics without replay
- **WHEN** 合法列表依次执行上传、clear、图形设置、直接 Draw 和 copy, 随后 Close/Execute
- **THEN** 命令在录制区间按序发出, 实际结果只包含一次绘制, Execute 仅为已发工作建立已有合同要求的真实提交与完成跟踪

#### Scenario: Require a fresh valid graphics recording
- **WHEN** 调用者在 Open 前、Close 后, 或在新 Open/ClearState 后尚未建立有效图形状态时 Draw
- **THEN** Debug 和 Release 均拒绝相应工作, 不使用旧 Graphics 状态绘制, 不影响另一列表的 claim 或丢弃已经发出的前缀

### Requirement: Compose executable graphics state from compatible owned objects

图形设置 SHALL 使用同一设备的真实 Pipeline、Framebuffer、绑定集合与 VB/IB. 目标兼容 SHALL 比较完整 FramebufferInfo, 不将附件身份或尺寸作为 Pipeline 身份. 活动绑定 SHALL 符合本次布局/资源类型、所需 block extent 和已知结构化记录粒度. 顶点绑定 SHALL 满足 InputLayout 的格式、slot、stride 和实例频率, 索引绑定 SHALL 满足用途、类型与对齐. 如果提供固定数量绘制的 `indirectParams`, 该参数 Buffer SHALL 属于同一设备且带 `isDrawIndirectArgs` 用途. 非法候选 SHALL 在该候选的原生应用前拒绝, 不发布部分可绘制状态.

#### Scenario: Draw into another compatible framebuffer
- **WHEN** 同一 Pipeline 分别与两个对象/尺寸不同但格式、sampleCount 和 sampleQuality 相同的 framebuffer 组合
- **THEN** 两次绘制分别使用各自附件和动态区域, 不要求因对象或尺寸不同重建 Pipeline

#### Scenario: Reject a foreign or incompatible executable state
- **WHEN** 候选状态包含另一设备/实现的对象、目标信息不匹配、活动绑定缺失或 range/recordStride 不满足接口
- **THEN** 设置明确失败且不应用该候选的原生状态, 诊断定位相关操作和输入, 不把单独成功创建的对象当作完整可绘制组合

#### Scenario: Preserve vertex input interpretation
- **WHEN** 合法状态使用非零 VB offset、多个合法 slot 或 per-instance 属性, 或仅用 builtin 而不需要顶点属性
- **THEN** 活动输入按公开描述的 location、格式、字节偏移、stride 和实例频率取数, 无属性路径同样可实际绘制

### Requirement: Deliver a rebuildable limited graphics capability with matching evidence

正式公共 RHI、Shader 包格式、canonical 接口生成约定和依赖方向 SHALL 保持既有合同. 单样本直接图形执行及固定数量 indexed indirect/MDI SHALL 按各自实际验证的范围启用, 未交付 profile SHALL 明确拒绝. 开发检查代码、Shader/schema、脚本与临时工程 SHALL 只在按工作包归类的 build 目录, 不成为正式源码、默认目标或普通构建前提.

验收 SHALL 使用匹配当前输入的真实 backend/context/device 的像素/已知数据及必要拒绝观察, 明确区分实际运行、复用和未覆盖范围. 基础直接绘制的证据 SHALL 与 D03-T04 的完整在途验收、D03-T05 稳定图像门槛、呈现或 D03 阶段出口分别记录. indexed indirect/MDI SHALL 具有该执行路径自己的原生证据.

#### Scenario: Build without development fixtures
- **WHEN** 在没有本地测试材料的新 binary tree 配置/构建正式目标和受影响消费者
- **THEN** 工程可构建且不下载或恢复 fixture, 通用 RHI 不引入原生/业务依赖, 正式默认目标无测试入口

#### Scenario: Report only the observed graphics boundary
- **WHEN** 当前有限图形检查及独立审查已完成并准备最终验收
- **THEN** 记录实际输入、构建/可执行文件、设备/驱动和结果/限制, 不将 Shader link、CPU 校验、过期运行或 T03 的有限结果当作后续阶段的完成证据

## ADDED Requirements

### Requirement: Execute fixed-count indexed indirect commands with their recorded dependencies

OpenGL 的固定数量 indexed indirect profile SHALL 建立在已校验的原生单命令和多命令入口上. `DrawIndexedIndirect(offsetBytes, drawCount)` SHALL 消费当前同设备 `GraphicsState.indirectParams` 中紧排的 20 字节 `DrawIndexedIndirectArguments`. 参数偏移按字节计, 索引起点按元素计, 索引类型 SHALL 使用当前 R16_UINT 或 R32_UINT 绑定. 本间接 profile 的索引绑定字节偏移 SHALL 为零. `baseVertexLocation` 为有符号值, `startInstanceLocation` 为首实例. 原生取数及 canonical VertexIndex/InstanceIndex SHALL 各应用相应基值一次. 合法单记录 SHALL 发出一次 indexed indirect, 合法多记录 SHALL 按记录顺序发出一次 MDI. 既有直接 indexed 仍按其公开绑定偏移取数.

Raw 和 Validation SHALL 核对有效 owning recording、图形状态及参数/索引 Buffer 的用途与可用性. offset 的 4 字节对齐、记录跨度、乘加、原生表示和容量 SHALL 在发出工作前校验. 合法零 drawCount SHALL 不发出原生 Draw 或增加原生调用数. 参数生产者 SHALL 保证记录中的计数、基值及实际索引、顶点和实例读取位于合法容量内. 后端 SHALL 直接消费 GPU Buffer 参数并核对命令边界可知的范围.

每次实际调用 SHALL 满足当前图形绑定、动态常量及 `IndirectArgument` 访问. Automatic 推导用途, Explicit 使用已经提交的状态, GraphManaged 同时核对当前 Pass 授权的参数范围. 绑定缓存命中 SHALL 继续消费本轮有效数据及已满足的依赖. 参数 Buffer 和所用图形依赖 SHALL 保活到真实完成或合法退出. 后续失败 SHALL 保留已发前缀并按原机制报告结果.

#### Scenario: Draw one and multiple records from the requested byte offset
- **WHEN** 真实列表由普通上传写入非对称的 indexed 参数及数据, 在非零参数偏移用一次单记录和一次多记录绘制, 记录包含非零 firstIndex、baseVertex 和 firstInstance
- **THEN** 原生读回匹配同序直接绘制或已知数据标记, 两次调用各只增加一个真实原生 Draw, 设施使用各自索引宽度及正确实例数据, 提交不再重放

#### Scenario: Refuse invalid indirect calls and leave a zero draw harmless
- **WHEN** Raw 或 Validation 收到本次调用缺少必要的有效状态、参数或索引角色, 范围未对齐或越界, drawCount 不可表示, 或索引绑定字节偏移非零的请求, 并运行合法零数量对照
- **THEN** 非法请求有相应诊断且无原生 Draw, 合法零数量不改目标或原生 Draw 计数, 有效状态的直接 indexed 偏移合同仍可使用

#### Scenario: Consume a changed command through a cached binding
- **WHEN** 同一参数 Buffer 由有序上传或复制更新, 调用者未再次改变图形状态就按 Automatic 或已声明及提交的用途绘制
- **THEN** 实际绘制使用更新记录, 正确使用本次常量范围, 缺失的显式用途或 GraphManaged 参数权限在发出相应命令前报告

#### Scenario: Preserve an issued indirect batch until real retirement
- **WHEN** 录制已经发出间接批次, 随后调用者释放原始参数句柄并正常提交, 或录制因后继错误只保留已发前缀
- **THEN** 参数及其它实际 Draw 依赖在真实访问结束前持续有效, 受影响的失败候选不会发布正常完整结果
