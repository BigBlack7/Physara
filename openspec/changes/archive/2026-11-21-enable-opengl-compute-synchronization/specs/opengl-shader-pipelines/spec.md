# opengl-shader-pipelines Spec Delta

## MODIFIED Requirements

### Requirement: Consume canonical modules through the existing Shader and RHI boundaries

包消费入口 SHALL 在创建 RHI Shader 前核对包完整性、所需 ABI 和已声明 profile，并以全有或全无的结果交付对应阶段。OpenGL Shader 创建 SHALL 接收自持的 canonical Vulkan SPIR-V，保留原字节码、入口和阶段，拒绝非法字节范围、入口或执行模型不匹配及受限 profile 之外的输入。产品包继续采用 `GraphicsBaselineV1` 的 Vertex/Fragment、main 入口、常量、只读定长结构化 buffer、独立采样图像和 Sampler 及 stage IO；浮点采样图像支持 `texture2D` 与 `textureCube`。公共 `CreateShader` 图形路径 SHALL 另接收符合当前布局合同、可按 UBO 保留 ABI 的 canonical push constant 块。公共 RHI SHALL 无需了解包格式或 Engine 类型。

包消费入口 SHALL 接受 `ComputeBaselineV1` 的单个已验证 Compute module，并核对入口、profile 与 ABI。直接 `CreateShader` 的 Compute 路径 SHALL 接受对应的 canonical 存储声明、固定工作组尺寸和可保持公共 ABI 的有限 push constant 块。

#### Scenario: Load a matching package and release its CPU storage
- **WHEN** 消费者按预期 ABI 加载合法的基线图形包, 成功创建 Shader 后释放包的原始 CPU 存储
- **THEN** 返回的阶段集合完整, 各 Shader 仍可提供与输入一致的字节码并用于后续 Pipeline 创建

#### Scenario: Reject a mismatching package before native preparation
- **WHEN** 包损坏、ABI 与消费者期望不同或 profile 不受支持
- **THEN** 包消费明确失败且不交付部分 Shader 集合, 不把保留的旧包或默认 Shader 当作本次成功

#### Scenario: Reject invalid direct Shader input
- **WHEN** 直接创建请求具有非法字节长度/头、不可解析模块、与入口不符的阶段，或使用该阶段 profile 之外的能力、资源类别或逻辑 set
- **THEN** Debug 和 Release 均拒绝创建 Shader，诊断标明操作、请求的阶段、适用 profile、入口、源身份和原因

#### Scenario: Prepare a matching compute stage from a canonical package

- **WHEN** CPU 消费者使用匹配的 `ComputeBaselineV1` 包创建 CS，并提交错误执行模型作为直接输入对照
- **THEN** 匹配字节码、入口和阶段被保留，执行模型不匹配在原生编译前拒绝

### Requirement: Preserve logical layout identities without reserving native holes

常规绑定布局 SHALL 保留资源类别、slot、数量、阶段可见性和公开 register-space 策略. descriptor-set 模式 SHALL 以 registerSpace 解释 canonical set; 顺序模式 SHALL 以 Pipeline 布局位置解释 set, 且不接受与该策略冲突的非零 registerSpace 或混合模式. 支持的布局 SHALL 使用普通 ConstantBuffer、只读 StructuredBuffer_SRV、Texture_SRV 和 Sampler, 每项数量为一. 图形与 Compute 的受支持布局 SHALL 支持单项 VolatileConstantBuffer 和符合公共大小、数量及阶段约定的 PushConstants. Compute 可见布局 SHALL 额外支持单项 StructuredBuffer_UAV 和 Texture_UAV，并保留逻辑 `u` 类别。超出已交付类型、阶段或数量的声明 SHALL 明确拒绝. 布局创建 SHALL 不按逻辑集合或 slot 预先占用 GL 单元.

#### Scenario: Reuse equal slots in distinct logical namespaces
- **WHEN** 两个集合分别声明 b0、t0 与 s0, 且类别与可见阶段合法
- **THEN** 布局保留全部独立身份, 不把它们合并为同一资源, 也不按集合预留固定原生区段

#### Scenario: Interpret the selected set strategy
- **WHEN** Pipeline 使用合法的顺序布局, 或使用顺序不同但 registerSpace 唯一的 descriptor-set 布局
- **THEN** 每项活动资源按所选策略解析到同一 canonical set/binding, 混合策略、重复 set 或不可表示的映射在成功准备前被拒绝

#### Scenario: Refuse a resource outside the current profile
- **WHEN** 请求 descriptor 数组、图形阶段 UAV、未交付的 image/UAV、typed/raw buffer、超出公共约定的 volatile/push、bindless 或不支持的可见阶段
- **THEN** 创建以 Unsupported 或对应非法输入诊断失败, 保留公共声明, 不替换资源类型或静默启用后续 profile

### Requirement: Allocate a deterministic compact plan from active cross-stage resources

Pipeline 准备 SHALL 合并各阶段最终 IR 的活动资源, 核对布局中的类别、canonical set/binding、可见性和跨阶段 block/IO 兼容性, 并生成确定且不可变的逻辑到原生映射. UBO、SSBO、image 和组合采样单元 SHALL 在各自 GL 命名空间紧凑分配; 相同逻辑资源跨阶段共享相应物理位置, 不活动声明 SHALL 不占单元. 所有活动数量、block 大小及适用的每-stage、combined 和 binding-index 限制 SHALL 基于真实设备值核对. 诊断或后端准备记录 SHALL 可追溯逻辑集合/类别/slot、阶段与原生位置; 超限 SHALL 报告所需值、实际限制及受影响类别.

活动 volatile 常量 SHALL 作为按布局选出的 UBO 参与同一计划。一个图形 Pipeline SHALL 至多拥有一处逻辑 push 声明，其活动块在相容阶段间共享一个独立原生 UBO 位置。该位置 SHALL 避免普通和 volatile 绑定冲突，并纳入实际阶段及 combined 限制。Lowering SHALL 保留公共 push 大小、成员 offset 与数组/矩阵 stride 的兼容关系。不能准确表达或跨阶段不匹配的布局 SHALL 在成功发布前拒绝。

Compute SHALL 从活动 IR 提取实际访问方向和资源类别。逻辑 `t` 类只读 SSBO 与逻辑 `u` 类的 storage block SHALL 遵守公开槽位身份并共同分配 SSBO binding。Storage image 单元的阶段、combined 及相关输出资源限制 SHALL 独立核对。Compute 活动常量和有限 push 声明 SHALL 走同一 UBO 计划并计入实际限制。

#### Scenario: Merge shared active resources
- **WHEN** VS/FS 共同使用同一逻辑 buffer, 并使用其它集合的同号 slot
- **THEN** 共享资源保留一致布局和原生位置, 不同集合保持独立, 映射与输入容器偶然遍历顺序无关

#### Scenario: Ignore inactive legal declarations
- **WHEN** 接口/布局有大量合法但本程序未活动的声明, 活动集合仍在实际限制内
- **THEN** Pipeline 不因全项目预留数量或 slot 空洞而误报设备不足

#### Scenario: Reject incompatible active interfaces
- **WHEN** 活动资源缺少布局、类型/可见性/布局不匹配, 或跨阶段资源与 IO 发生冲突
- **THEN** Pipeline 准备明确失败并定位冲突, 不把单独可创建的 Shader 当作兼容的完整程序

#### Scenario: Enforce stage and combined limits
- **WHEN** 原生 binding 数量、阶段资源数、combined 使用量或 block 大小超过任一适用设备限制
- **THEN** Pipeline 在成功 link/发布前失败; 共享物理位置不掩盖多个阶段对 combined 限额的实际贡献

#### Scenario: Reserve the active graphics push block beside other constants
- **WHEN** 真实图形程序使用普通 UBO、volatile UBO 和跨阶段相容的 push 块，另一个准备请求具有无法保持 ABI 的 push 布局
- **THEN** 有效程序 link 后三种活动用途使用互不冲突的原生 UBO 位置，push 字段的偏移与阶段可见性保持匹配；不兼容请求给出原因且无可用 Pipeline

#### Scenario: Disambiguate active compute binding spaces

- **WHEN** 单个 Compute stage 从多个逻辑 set 的同号槽使用 UBO、SSBO 和 image，其中一个合法声明不活动
- **THEN** 活动资源按逻辑集合、类别和槽位取得确定的实际位置。SSBO 的读写槽彼此兼容，image 与采样各用自己的命名空间，不活动声明不占单元

## ADDED Requirements

### Requirement: Prepare a compute pipeline with exact storage bindings

同设备 Compute Pipeline SHALL 由单个有效 CS、兼容布局和通过设备限制检查的本地工作组准备真实 Program，不依赖 Framebuffer。实际结构化 Buffer 绑定 SHALL 核对合法范围、SSBO offset 对齐、记录粒度、活动块 extent、大小上限及所需角色。Storage image SHALL 按实际存储格式、单 mip、有效层范围、维度、访问方向与真实支持绑定。`image2D` SHALL 选择一个二维图层，`image2DArray` SHALL 选择声明的连续层范围。资源身份与范围 SHALL 按原 storage 解释。Program、布局、BindingSet 的准备及失败退回 SHALL 沿用既有保活、诊断和清理合同。

#### Scenario: Bind precisely the selected storage subresources

- **WHEN** Compute 使用非零字节起点的合法结构化记录范围、非零 mip 的单层 image 和受支持的连续数组层 image
- **THEN** 实际访问仅命中选中的范围与层，读取的值符合显式格式和访问方向，邻近字节或 mip/layer 的哨兵值保持

#### Scenario: Reject invalid execution or storage limits at the owning boundary

- **WHEN** Pipeline 的固定工作组超出实际尺寸或 invocation 限制，或绑定出现错误格式、维度、用途、extent、alignment、layer 或访问权限
- **THEN** 准备或消费边界给出定位限制或绑定的诊断，不发布失效的 Pipeline 或执行非法 Dispatch

#### Scenario: Diagnose conflicting compute push layouts before native preparation

- **WHEN** 同一设备的有效 CS 与两份各含 push 声明的合法布局一起提交给 `CreateComputePipeline`
- **THEN** Pipeline 准备在编译 GLSL 前拒绝重复布局，诊断标明 Compute 操作、阶段、profile、入口、源身份和布局冲突原因
