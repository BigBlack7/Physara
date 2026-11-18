# Spec Delta

## MODIFIED Requirements

### Requirement: Consume canonical modules through the existing Shader and RHI boundaries

包消费入口 SHALL 在创建 RHI Shader 前核对包完整性、所需 ABI 和已声明 profile，并以全有或全无的结果交付对应阶段。OpenGL Shader 创建 SHALL 接收自持的 canonical Vulkan SPIR-V，保留原字节码、入口和阶段，拒绝非法字节范围、入口或执行模型不匹配及受限 profile 之外的输入。当前支持范围 SHALL 与 `GraphicsBaselineV1` 的 Vertex/Fragment、main 入口、常量、只读定长结构化 buffer、独立采样图像和 Sampler 及 stage IO 一致。浮点采样图像支持 `texture2D` 与 `textureCube`。公共 RHI SHALL 无需了解包格式或 Engine 类型。

#### Scenario: Load a matching package and release its CPU storage
- **WHEN** 消费者按预期 ABI 加载合法的基线图形包, 成功创建 Shader 后释放包的原始 CPU 存储
- **THEN** 返回的阶段集合完整, 各 Shader 仍可提供与输入一致的字节码并用于后续 Pipeline 创建

#### Scenario: Reject a mismatching package before native preparation
- **WHEN** 包损坏、ABI 与消费者期望不同或 profile 不受支持
- **THEN** 包消费明确失败且不交付部分 Shader 集合, 不把保留的旧包或默认 Shader 当作本次成功

#### Scenario: Reject invalid direct Shader input
- **WHEN** 直接创建请求具有非法字节长度/头、不可解析模块、与入口不符的阶段或基线外能力
- **THEN** Debug 和 Release 均不交付可用 Shader, 并按既有错误边界标明操作、阶段/入口及原因

### Requirement: Create immutable binding sets with exact resource ownership and views

绑定集 SHALL 对应同一设备的常规布局，按布局核对独立资源类型、slot 与范围，保存不可变描述及预解析视图和实际范围。Buffer range SHALL 满足资源用途、已知记录粒度、UBO/SSBO offset 对齐和实际设备限制。Texture SRV SHALL 保留所请求的兼容二维单层视图或单个 cube 视图，以及 format、mip/layer 范围和分量映射。cube 采样视图 SHALL 选择完整六面和合法 mip 区间，并支持启用时的跨面线性过滤。不可用或不兼容的资源和视图 SHALL 拒绝。图形状态 SHALL 在原生绑定和 Draw 前核对活动图像类型与实际视图维度。绑定集 SHALL 强持有布局与实际资源依赖。`trackLiveness` SHALL 保持其公开的命令列表保活含义及集合自身的依赖所有权。

Buffer 绑定的 `byteSize=0` 与全一长度哨兵 SHALL 按公共范围语义解释为从合法 byteOffset 到 Buffer 末尾，与该剩余长度的显式表示等价。解析后的范围 SHALL 非空并满足既有约束。显式越界起点或长度 SHALL 被拒绝。Debug/Release 和 raw/Validation 消费 SHALL 保持同一合法范围语义。

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
