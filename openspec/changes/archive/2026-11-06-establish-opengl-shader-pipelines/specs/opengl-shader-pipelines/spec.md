# OpenGL Shader and Pipeline Spec Delta

## Purpose

定义将已校验的 canonical Shader 输入、逻辑资源布局和目标兼容信息准备为真实 OpenGL 图形程序的可观察合同. 消费者能够保持跨 API 的资源身份与所有权, 获得符合实际设备限制的绑定映射, 并区分准备成功、非法输入和未实现的命令执行能力.

## ADDED Requirements

### Requirement: Consume canonical modules through the existing Shader and RHI boundaries

包消费入口 SHALL 在创建 RHI Shader 前核对包完整性、所需 ABI 和已声明 profile, 并以全有或全无的结果交付对应阶段. OpenGL Shader 创建 SHALL 接收自持的 canonical Vulkan SPIR-V, 保留原字节码、入口和阶段, 拒绝非法字节范围、入口/执行模型不匹配及受限 profile 之外的输入. 当前支持范围 SHALL 与 `GraphicsBaselineV1` 的 Vertex/Fragment、main 入口、常量、只读定长结构化 buffer、独立 float texture2D/Sampler 和 stage IO 一致. 公共 RHI SHALL 无需了解包格式或 Engine 类型.

#### Scenario: Load a matching package and release its CPU storage
- **WHEN** 消费者按预期 ABI 加载合法的基线图形包, 成功创建 Shader 后释放包的原始 CPU 存储
- **THEN** 返回的阶段集合完整, 各 Shader 仍可提供与输入一致的字节码并用于后续 Pipeline 创建

#### Scenario: Reject a mismatching package before native preparation
- **WHEN** 包损坏、ABI 与消费者期望不同或 profile 不受支持
- **THEN** 包消费明确失败且不交付部分 Shader 集合, 不把保留的旧包或默认 Shader 当作本次成功

#### Scenario: Reject invalid direct Shader input
- **WHEN** 直接创建请求具有非法字节长度/头、不可解析模块、与入口不符的阶段或基线外能力
- **THEN** Debug 和 Release 均不交付可用 Shader, 并按既有错误边界标明操作、阶段/入口及原因

### Requirement: Preserve logical layout identities without reserving native holes

常规绑定布局 SHALL 保留资源类别、slot、数量、阶段可见性和公开 register-space 策略. descriptor-set 模式 SHALL 以 registerSpace 解释 canonical set; 顺序模式 SHALL 以 Pipeline 布局位置解释 set, 且不接受与该策略冲突的非零 registerSpace 或混合模式. 支持的布局 SHALL 使用普通 ConstantBuffer、只读 StructuredBuffer_SRV、Texture_SRV 和 Sampler, 每项数量为一; 其它类型/数组 SHALL 明确拒绝. 布局创建 SHALL 不按逻辑集合或 slot 预先占用 GL 单元.

#### Scenario: Reuse equal slots in distinct logical namespaces
- **WHEN** 两个集合分别声明 b0、t0 与 s0, 且类别与可见阶段合法
- **THEN** 布局保留全部独立身份, 不把它们合并为同一资源, 也不按集合预留固定原生区段

#### Scenario: Interpret the selected set strategy
- **WHEN** Pipeline 使用合法的顺序布局, 或使用顺序不同但 registerSpace 唯一的 descriptor-set 布局
- **THEN** 每项活动资源按所选策略解析到同一 canonical set/binding, 混合策略、重复 set 或不可表示的映射在成功准备前被拒绝

#### Scenario: Refuse a resource outside the current profile
- **WHEN** 请求 descriptor 数组、image/UAV、typed/raw buffer、volatile/push、bindless 或不支持的可见阶段
- **THEN** 创建以 Unsupported 或对应非法输入诊断失败, 保留公共声明, 不替换资源类型或静默启用后续 profile

### Requirement: Create immutable binding sets with exact resource ownership and views

绑定集 SHALL 对应同一设备的常规布局, 按布局核对独立资源类型、slot 与范围, 保存不可变描述及预解析视图/range. Buffer range SHALL 满足资源用途、已知记录粒度、UBO/SSBO offset 对齐和实际设备限制; Texture SRV SHALL 保留所请求的兼容二维视图、format、mip/layer 范围和分量映射. 不可用或不兼容的资源/视图 SHALL 拒绝. 绑定集 SHALL 强持有布局与实际资源依赖; trackLiveness SHALL 保持其公开的命令列表保活含义, 不改变集合自身的依赖所有权.

#### Scenario: Retain a valid set after caller releases dependencies
- **WHEN** 绑定集从合法常量、结构化 buffer、纹理视图与独立 sampler 创建, 调用方随后释放这些对象的原始句柄
- **THEN** 集合仍能提供正确描述与解析记录, 资源在集合释放依赖前保持存活, 创建不修改 Texture 的采样状态

#### Scenario: Reject a wrong or foreign binding
- **WHEN** 资源来自另一设备/实现、类型与布局不匹配, 或缺少必须的实际资源
- **THEN** 创建不发布部分集合或视图, 报告相关逻辑位置及原因, 不对该资源执行原生绑定

#### Scenario: Reject an invalid active range
- **WHEN** 请求显式越界/不兼容的纹理视图, 或越界、未对齐、不满足记录粒度或超过实际 block 限制的 buffer range
- **THEN** 创建明确失败, 不通过截断范围、改变 format 或忽略 offset 来伪装成功

### Requirement: Allocate a deterministic compact plan from active cross-stage resources

Pipeline 准备 SHALL 合并各阶段最终 IR 的活动资源, 核对布局中的类别、canonical set/binding、可见性和跨阶段 block/IO 兼容性, 并生成确定且不可变的逻辑到原生映射. UBO、只读 SSBO 和组合采样单元 SHALL 在各自命名空间紧凑分配; 相同逻辑资源跨阶段共享相应物理位置, 不活动声明 SHALL 不占单元. 所有活动数量、block 大小及适用的每-stage、combined 和 binding-index 限制 SHALL 基于真实设备值核对. 诊断或后端准备记录 SHALL 可追溯逻辑集合/类别/slot、阶段与原生位置; 超限 SHALL 报告所需值、实际限制及受影响类别.

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

### Requirement: Combine sampling by logical texture and sampler pairs

每个活动 Texture/Sampler 逻辑采样对 SHALL 对应一个确定的物理 texture unit, 同一对跨阶段能够共享, 同一 Texture 与不同 Sampler SHALL 保持不同组合. 对的两端 SHALL 能来自不同逻辑集合, 不依赖两个 slot 数值相等或集合同时更新. Texture-only fetch SHALL 使用后端自有的固定 nearest/clamp dummy sampler 组合, 不要求调用方虚构逻辑 Sampler. 公开绑定合同 SHALL 继续区分 Texture_SRV 与 Sampler, 不修改 Texture 对象采样参数.

#### Scenario: Sample one texture with two samplers
- **WHEN** Shader 将同一逻辑 Texture 分别与两个不同逻辑 Sampler 配对
- **THEN** 原生映射包含两个不同组合单元, 二者引用同一纹理视图及各自 sampler, 且参与相应阶段和 combined 限制计算

#### Scenario: Pair resources across sets and stages
- **WHEN** Texture 与 Sampler 来自不同 set, 且同一逻辑对被多个阶段使用
- **THEN** 组合正确解析双方逻辑身份, 跨阶段复用同一对的物理单元, 不把 set 或 slot 相等当作配对条件

#### Scenario: Use a texture without a declared sampler
- **WHEN** 合法模块仅用 Texture-only fetch 访问某纹理
- **THEN** 准备生成有界、可退休的后端 dummy 组合并计入单元预算, 不改变接口 ABI 或增加公共绑定类型

### Requirement: Lower and link the native program while preserving the canonical ABI

Pipeline 准备 SHALL 从 canonical IR 生成 desktop GLSL 460, 依计划配置原生资源绑定并完成真实编译/link. lowering SHALL 保留字段 offset、数组/矩阵 stride、只读性、location/插值及既有 upper-left/zero-to-one clip 合同. InstanceIndex SHALL 表达 gl_InstanceID 与原生 base instance 之和, VertexIndex SHALL 使用已含 baseVertex 的顶点索引; 不得依赖逐 Draw 更新的模拟 uniform 或运行时源文本替换. 失败 SHALL 沿既有 RHI 空句柄/MessageCallback 合同返回, 保留可用的阶段、入口、源身份、GLSL/log 位置和原生原因, 清理未发布状态, 不破坏先前有效 Pipeline.

#### Scenario: Link the baseline graphics program
- **WHEN** 合法阶段、布局与目标信息在真实支持环境中准备成功
- **THEN** 返回的 Pipeline 拥有实际 link 成功的 Program, 其可观察资源绑定与活动接口符合准备映射及 ABI, 不只生成一份 GLSL 文本就报告成功

#### Scenario: Preserve vertex and clip semantics during lowering
- **WHEN** canonical 模块活动使用 InstanceIndex、VertexIndex 或位置/深度 builtin
- **THEN** lowering 不遗漏 firstInstance、不重复添加 baseVertex、不额外执行旧 GL 深度范围修正或 Y 翻转, 不留下需要业务更新的 base-instance uniform

#### Scenario: Fail native compilation or link transactionally
- **WHEN** 当前输入不能完成原生编译/link 或其准备步骤失败
- **THEN** 不发布部分 Pipeline, 原因与关联阶段/程序可定位, 已取得的临时 shader/program/view 得到正确清理, 旧有效对象保持可用

### Requirement: Prepare input and target compatibility independently of attachment identity

图形 Pipeline SHALL 验证阶段组合、InputLayout、受支持固定状态及目标格式/样本信息, 并强持有其 CPU 依赖. 基础 profile SHALL 支持单样本的普通图形准备, 拒绝未交付的 tessellation/geometry、MSAA 绘制 profile 和扩展状态. 顶点属性 SHALL 按公开描述顺序及 arraySize 展开为 location, 保留 bufferIndex、offset、stride 和实例频率, 检查活动 VS 输入的类型/位置与设备顶点输入限制. Framebuffer 兼容身份 SHALL 使用公开 FramebufferInfo 的全部字段, 不含纹理对象、尺寸或本帧 framebuffer 身份; 片元输出 SHALL 与有效附件位置和数值类型兼容.

#### Scenario: Reuse compatibility across different framebuffer objects
- **WHEN** 两个有效 framebuffer 的纹理身份或尺寸不同, 但颜色/depth 格式、sampleCount 与 sampleQuality 一致
- **THEN** 它们具有相同 Pipeline 兼容信息, 无需因附件身份不同而准备另一份 Pipeline

#### Scenario: Match a declared vertex input layout
- **WHEN** 合法属性覆盖活动 VS 输入, 或 VS 只使用 builtin 且不需要属性
- **THEN** Pipeline 分别保留正确的属性位置/格式/频率或接受空输入, 不通过属性 debug name 猜测另一个 location ABI

#### Scenario: Reject incompatible graphics preparation
- **WHEN** 属性位置/类型不兼容、阶段或状态不受支持、目标样本/format 不可用或片元输出与附件不兼容
- **THEN** 准备在发布前明确失败; 实际 framebuffer 与 Pipeline 兼容信息不一致时不得被视为可绘制状态

### Requirement: Extend the existing context-owned retirement protocol

新 Shader、布局、集合、Pipeline 及其 Program/自有 view/sampler SHALL 保持设备归属和实际依赖寿命, 创建/原生准备 SHALL 满足既有 owner thread/current-context 前提. 最终 CPU Release SHALL 不在 foreign thread 执行 GL 或为退休分配新存储, 原生对象 SHALL 经已有删除协议在正确 context 退休. 失败清理 SHALL 保留主要诊断, 不引入固定帧延迟、无界缓存或把 CPU 所有权描述为 GPU 完成.

#### Scenario: Keep the prepared dependency chain alive
- **WHEN** 调用方释放 Shader、布局、集合资源等原始句柄, 但相应 Pipeline 或 BindingSet 仍持有它们
- **THEN** 所需依赖持续有效, 最后一个实际 owner 释放后才退休, 不产生悬空 CPU/原生引用

#### Scenario: Release prepared native objects from a worker
- **WHEN** 未进入 GPU 执行的 Pipeline 或 BindingSet 在 worker 上失去最后一个 CPU owner, 随后 owner context 执行回收/关闭
- **THEN** worker 仅转交退休状态, 原生 Program/自有 view/sampler 被正确回收, 不借此声称 Draw 在途寿命已经验收

### Requirement: Deliver a rebuildable preparation capability with honest acceptance evidence

正式工程 SHALL 使用固定精简依赖, configure 不下载, 通用 RHI 消费者不引入原生或业务依赖, 包编译步骤不反向依赖其消费层. 所有开发检查代码、Shader/JSON、脚本和临时工程 SHALL 仅在 build 下归类, 不进入正式源码/默认目标; 删除这些材料后普通工程 SHALL 可重新配置/构建. 验收 SHALL 使用当前输入对应的真实 GL compile/link/接口观察及最小 CPU/拒绝检查, 清楚区分实际运行、复用和未覆盖范围. 创建/link 证据 SHALL 不代表命令执行、像素、在途绘制退休、呈现或 D03 阶段出口通过.

#### Scenario: Build formal modules without local fixtures
- **WHEN** 从无本地测试材料的 binary tree 配置并构建正式 Debug/Release 目标
- **THEN** Shader 消费适配与 GL 准备实现可构建, 通用 RHI 可独立消费, 不需要恢复 build 内测试或下载依赖

#### Scenario: Report preparation acceptance at the correct boundary
- **WHEN** 本能力经本地检查及独立审查后记录验收
- **THEN** 记录匹配的输入、构建、可执行文件与原生环境及观察结果, 后续 Draw/像素/在途和呈现门槛仍明确归属各自工作包
