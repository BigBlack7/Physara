# Spec Delta

## MODIFIED Requirements

### Requirement: Consume canonical modules through the existing Shader and RHI boundaries

包消费入口 SHALL 在创建 RHI Shader 前核对包完整性、所需 ABI 和已声明 profile，并以全有或全无的结果交付对应阶段。OpenGL Shader 创建 SHALL 接收自持的 canonical Vulkan SPIR-V，保留原字节码、入口和阶段，拒绝非法字节范围、入口或执行模型不匹配及受限 profile 之外的输入。产品包继续采用 `GraphicsBaselineV1` 的 Vertex/Fragment、main 入口、常量、只读定长结构化 buffer、独立采样图像和 Sampler 及 stage IO；浮点采样图像支持 `texture2D` 与 `textureCube`。公共 `CreateShader` 图形路径 SHALL 另接收符合当前布局合同、可按 UBO 保留 ABI 的 canonical push constant 块。公共 RHI SHALL 无需了解包格式或 Engine 类型。

#### Scenario: Load a matching package and release its CPU storage
- **WHEN** 消费者按预期 ABI 加载合法的基线图形包, 成功创建 Shader 后释放包的原始 CPU 存储
- **THEN** 返回的阶段集合完整, 各 Shader 仍可提供与输入一致的字节码并用于后续 Pipeline 创建

#### Scenario: Reject a mismatching package before native preparation
- **WHEN** 包损坏、ABI 与消费者期望不同或 profile 不受支持
- **THEN** 包消费明确失败且不交付部分 Shader 集合, 不把保留的旧包或默认 Shader 当作本次成功

#### Scenario: Reject invalid direct Shader input
- **WHEN** 直接创建请求具有非法字节长度/头、不可解析模块、与入口不符的阶段或当前图形 profile 之外的能力
- **THEN** Debug 和 Release 均不交付可用 Shader, 并按既有错误边界标明操作、阶段/入口及原因

### Requirement: Preserve logical layout identities without reserving native holes

常规绑定布局 SHALL 保留资源类别、slot、数量、阶段可见性和公开 register-space 策略. descriptor-set 模式 SHALL 以 registerSpace 解释 canonical set; 顺序模式 SHALL 以 Pipeline 布局位置解释 set, 且不接受与该策略冲突的非零 registerSpace 或混合模式. 支持的布局 SHALL 使用普通 ConstantBuffer、只读 StructuredBuffer_SRV、Texture_SRV 和 Sampler, 每项数量为一. 图形布局 SHALL 支持单项 VolatileConstantBuffer 和符合公共大小、数量及阶段约定的 PushConstants; 其它类型/数组 SHALL 明确拒绝. 布局创建 SHALL 不按逻辑集合或 slot 预先占用 GL 单元.

#### Scenario: Reuse equal slots in distinct logical namespaces
- **WHEN** 两个集合分别声明 b0、t0 与 s0, 且类别与可见阶段合法
- **THEN** 布局保留全部独立身份, 不把它们合并为同一资源, 也不按集合预留固定原生区段

#### Scenario: Interpret the selected set strategy
- **WHEN** Pipeline 使用合法的顺序布局, 或使用顺序不同但 registerSpace 唯一的 descriptor-set 布局
- **THEN** 每项活动资源按所选策略解析到同一 canonical set/binding, 混合策略、重复 set 或不可表示的映射在成功准备前被拒绝

#### Scenario: Refuse a resource outside the current profile
- **WHEN** 请求 descriptor 数组、image/UAV、typed/raw buffer、超出公共约定的 volatile/push、bindless 或不支持的可见阶段
- **THEN** 创建以 Unsupported 或对应非法输入诊断失败, 保留公共声明, 不替换资源类型或静默启用后续 profile

### Requirement: Create immutable binding sets with exact resource ownership and views

绑定集 SHALL 对应同一设备的常规布局，按布局核对独立资源类型、slot 与范围，保存不可变描述及预解析视图和实际范围。Buffer range SHALL 满足资源用途、已知记录粒度、UBO/SSBO offset 对齐和实际设备限制。Texture SRV SHALL 保留所请求的兼容二维单层视图或单个 cube 视图，以及 format、mip/layer 范围和分量映射。cube 采样视图 SHALL 选择完整六面和合法 mip 区间，并支持启用时的跨面线性过滤。不可用或不兼容的资源和视图 SHALL 拒绝。图形状态 SHALL 在原生绑定和 Draw 前核对活动图像类型与实际视图维度。绑定集 SHALL 强持有布局与实际资源依赖。`trackLiveness` SHALL 保持其公开的命令列表保活含义及集合自身的依赖所有权。

Buffer 绑定的 `byteSize=0` 与全一长度哨兵 SHALL 按公共范围语义解释为从合法 byteOffset 到 Buffer 末尾，与该剩余长度的显式表示等价。解析后的范围 SHALL 非空并满足既有约束。显式越界起点或长度 SHALL 被拒绝。Debug/Release 和 raw/Validation 消费 SHALL 保持同一合法范围语义。

Volatile 位置 SHALL 接收类型匹配且覆盖整个逻辑 Buffer 的绑定，准备时保持对象身份和逻辑大小，实际 Draw 从对应 recording 解析版本。PushConstants 项 SHALL 按布局的 slot 与大小匹配，数据由本轮命令提供。普通 UBO、volatile 和 push SHALL 分别沿各自公开描述验证。

#### Scenario: Retain a valid set after caller releases dependencies
- **WHEN** 绑定集从合法常量、结构化 buffer、纹理视图与独立 sampler 创建, 调用方随后释放这些对象的原始句柄
- **THEN** 集合仍能提供正确描述与解析记录, 资源在集合释放依赖前保持存活, 创建不修改 Texture 的采样状态

#### Scenario: Reject a wrong or foreign binding
- **WHEN** 资源来自另一设备/实现、类型与布局不匹配, 或缺少必须的实际资源
- **THEN** 创建不发布部分集合或视图, 报告相关逻辑位置及原因, 不对该资源执行原生绑定

#### Scenario: Reject an invalid active range
- **WHEN** 请求显式越界/不兼容的纹理视图, 或越界、未对齐、不满足记录粒度或超过实际 block 限制的 buffer range
- **THEN** 创建明确失败, 不通过截断范围、改变 format 或忽略 offset 来伪装成功

#### Scenario: Bind equivalent zero sentinel and explicit remaining ranges
- **WHEN** 合法常量 Buffer 或只读结构化 Buffer 绑定使用零长度、全一长度哨兵或对应显式剩余长度, 起点/记录粒度/设备限制均满足
- **THEN** 三种表示均创建成功并保留相同精确 offset/size, 非零起点不会被清零, 实际消费者能使用该合法绑定

#### Scenario: Reject an explicit length beyond the remaining capacity
- **WHEN** 合法起点的显式长度紧邻越过剩余容量, 或起点本身越出可绑定范围
- **THEN** 不发布绑定集, 给出非法范围诊断, 不 clamp 成一个合法非空范围或改变原先有效集合

#### Scenario: Sample the selected cube view in a graphics pass
- **WHEN** 活动浮点 cube 图像与独立 Sampler 组成采样对，兼容视图选择六个具有可区分数据的面和合法 mip 区间
- **THEN** 实际绘制按方向及 LOD 取得该区间的样值，跨面线性过滤按 cube 合同取样，图形使用的原始面和 mip 范围仍可追溯

#### Scenario: Reject cube dimension or face range mismatches before a draw
- **WHEN** cube 采样位置收到二维视图，或一个请求的 cube 视图缺面或超出允许 mip 范围
- **THEN** 对应准备阶段报告实际不匹配及位置，Debug 和 Release 均拒绝发出 Draw

### Requirement: Allocate a deterministic compact plan from active cross-stage resources

Pipeline 准备 SHALL 合并各阶段最终 IR 的活动资源, 核对布局中的类别、canonical set/binding、可见性和跨阶段 block/IO 兼容性, 并生成确定且不可变的逻辑到原生映射. UBO、只读 SSBO 和组合采样单元 SHALL 在各自命名空间紧凑分配; 相同逻辑资源跨阶段共享相应物理位置, 不活动声明 SHALL 不占单元. 所有活动数量、block 大小及适用的每-stage、combined 和 binding-index 限制 SHALL 基于真实设备值核对. 诊断或后端准备记录 SHALL 可追溯逻辑集合/类别/slot、阶段与原生位置; 超限 SHALL 报告所需值、实际限制及受影响类别.

活动 volatile 常量 SHALL 作为按布局选出的 UBO 参与同一计划。一个图形 Pipeline SHALL 至多拥有一处逻辑 push 声明，其活动块在相容阶段间共享一个独立原生 UBO 位置。该位置 SHALL 避免普通和 volatile 绑定冲突，并纳入实际阶段及 combined 限制。Lowering SHALL 保留公共 push 大小、成员 offset 与数组/矩阵 stride 的兼容关系。不能准确表达或跨阶段不匹配的布局 SHALL 在成功发布前拒绝。

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
