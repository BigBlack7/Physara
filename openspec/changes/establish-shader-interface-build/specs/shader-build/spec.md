# Shader Build Spec Delta

## Purpose

定义从权威接口声明与 Shader 源输入生成 CPU/Shader 合同和 canonical 编译包的可观察行为. 使下游能在不依赖原生图形上下文的条件下识别接口、追溯完整编译输入, 并可靠拒绝不匹配、损坏或陈旧的构建结果.

## ADDED Requirements

### Requirement: Generate one authoritative CPU and Shader interface

系统 SHALL 从同一接口声明生成 C++ 数据表示及布局检查、canonical GLSL 数据/阶段 IO 声明、逻辑绑定描述、反射预期与 ABI 身份. 支持的数据类型、布局规则、矩阵方向和数组范围 SHALL 明确, 生成物 SHALL 仅写入 binary tree; 业务消费者 SHALL 无需手写第二份字段布局、stage IO 或资源 binding.

#### Scenario: Generate a supported interface
- **WHEN** 接口包含受支持的固定宽度标量、向量、列主序矩阵及定长数组, 并指定其 buffer 布局
- **THEN** CPU 与 Shader 产物表达相同字段含义, 且尺寸、对齐、成员 offset、矩阵 stride 和数组 stride 有可执行的核对手段

#### Scenario: Distinguish layout rules
- **WHEN** 同一组字段分别采用 std140 与 std430, 且两者规则产生不同数组 stride 或块对齐
- **THEN** 两份生成结果各自遵守所选规则, 不以宿主类型的自然布局假定二者相同

#### Scenario: Reject an invalid interface
- **WHEN** 输入包含重复身份、未知类型、不支持的布局或维度、零长数组, 或导致尺寸/offset 计算溢出
- **THEN** 生成失败并指出声明位置与原因, 不发布可供消费者当作成功结果的新接口

### Requirement: Preserve logical resource binding identities

生成的资源接口 SHALL 保留逻辑集合、b/t/s/u 类别、slot、资源类型和阶段可见性. Canonical descriptor set/binding SHALL 与公开 RHI 的 Vulkan offset 合同一致, 不同类别的同号 slot SHALL 保持独立; 该结果 SHALL 不预先声明或占用 OpenGL 原生绑定单元.

#### Scenario: Use equal slots in different resource categories
- **WHEN** 同一逻辑集合声明 b0、t0 与 s0
- **THEN** 三者在生成的 RHI 描述中保持各自类别, 在 canonical IR 中具有无冲突且可追溯的 set/binding

#### Scenario: Reject conflicting or unrepresentable bindings
- **WHEN** 声明重复的集合/类别/slot, 超出受支持集合或 slot 范围, 或无法用公开 RHI 描述安全表示
- **THEN** 构建在生成成功结果之前拒绝输入, 不截断数值、不越过固定容量, 也不静默合并资源

### Requirement: Compile against a declared canonical profile

系统 SHALL 使用显式的源语言、入口、阶段、目标环境、IR 版本和编译选项生成并验证 canonical Vulkan SPIR-V. 基础 profile SHALL 限于后续 GL 4.6 lowering 可承接的已声明公共子集; 超出 profile 的阶段、资源或能力 SHALL 明确失败, 不因工具能够编译而自动成为项目支持能力.

#### Scenario: Compile the baseline graphics program
- **WHEN** 合法的顶点/片元程序使用支持的常量、只读 buffer、独立纹理/采样器及阶段接口
- **THEN** 各阶段输出通过所声明目标环境验证的 SPIR-V, 并记录实际入口、阶段及 profile 身份

#### Scenario: Reject an unsupported capability
- **WHEN** 输入请求基础 profile 之外的 descriptor indexing、subpass input、push constant、compute 或未声明扩展
- **THEN** 编译包构建失败并说明不支持的请求, 不隐式启用、更换目标或改写源文本来伪装兼容

### Requirement: Resolve and trace complete source dependencies

系统 SHALL 以明确且稳定的规则解析源文件与生成接口的 include, 记录实际使用的传递输入及其内容身份, 并保留诊断所需源映射. 缺失、歧义、越出允许根目录的 include 或被禁止的循环 SHALL 明确失败; 本机绝对工作区路径 SHALL 不成为可重建内容身份的一部分.

#### Scenario: Follow a transitive include
- **WHEN** 顶层 Shader 经公共 include 引用生成接口
- **THEN** 构建记录完整依赖链, 并能将该链中错误定位回对应源文件及行号

#### Scenario: Change an included file
- **WHEN** 按支持的构建入口修改实际使用的传递 include 内容并重新构建
- **THEN** 受影响程序被重新评估, 旧内容身份不再被当作新输入的成功编译结果

#### Scenario: Fail include resolution
- **WHEN** include 不存在、解析含糊、越过允许根目录或形成 profile 禁止的循环
- **THEN** 构建报告请求文件、引用位置及解析原因, 不忽略该 include 或退回另一份未声明来源

### Requirement: Verify final IR against the declared interface

系统 SHALL 从最终发布的 SPIR-V 反射资源与数据布局, 核对活动资源的类型、set/binding、可见性以及成员 offset、矩阵/数组 stride 和块范围, 并核对跨阶段的输入输出兼容性. 所有活动业务资源 SHALL 能追溯到权威接口; 生成预期与最终 IR 不匹配 SHALL 阻止发布成功编译包. 不活动的合法声明 SHALL 不被误判为必须占用原生资源.

#### Scenario: Accept a matching program
- **WHEN** 最终 IR 的活动资源与接口匹配, 且片元输入具有兼容的顶点输出
- **THEN** 编译包包含通过核对的最终反射, 并区分声明集合与各阶段实际活动资源

#### Scenario: Detect a stale or changed layout
- **WHEN** 编译结果的成员 offset、数组/矩阵 stride、资源类型或 binding 与本次接口预期不同
- **THEN** 构建失败并指出预期值、实际值及相关声明, 不仅凭生成头能够编译就报告 ABI 通过

#### Scenario: Reject incompatible stage interfaces
- **WHEN** 片元输入缺少对应顶点输出, 或其 location、类型、分量占用或必要插值限定不兼容
- **THEN** 程序构建失败并指出冲突的阶段接口, 不把两个单独可编译的阶段当作合法程序

#### Scenario: Reflect an optimized-away declaration
- **WHEN** 合法接口中的资源未被该程序实际使用而未出现在活动反射中
- **THEN** 反射如实标记活动范围, 且不因此制造资源或错误宣称该声明经过实际使用验证

### Requirement: Identify every compilation input

编译包身份 SHALL 覆盖顶层及传递源内容、接口输入、合法变体及其值、入口/阶段、全部有效编译选项、目标 profile、生成规则与编译/验证/反射工具指纹. ABI 身份 SHALL 基于接口语义, 不依赖 C++ padding、地址、时间戳或不稳定的容器迭代顺序.

#### Scenario: Reuse unchanged inputs
- **WHEN** 相同逻辑路径、内容、接口与工具选项在不同绝对工作目录中构建
- **THEN** 编译内容身份与 ABI 身份一致, 且结果不依赖旧 binary tree

#### Scenario: Invalidate a previous identity
- **WHEN** 任一实际 include 内容、接口布局、合法变体值、入口/阶段、有效选项、目标 profile 或工具指纹发生变化
- **THEN** 编译内容身份变化, 旧编译包不能作为此次输入命中的结果

#### Scenario: Preserve semantic ABI identity
- **WHEN** 接口仅发生不改变字段、顺序、布局或绑定含义的格式变化
- **THEN** ABI 身份保持一致, 同时输入内容变化仍可被编译依赖身份追踪

### Requirement: Publish and consume complete compilation packages

成功编译包 SHALL 具有明确版本, 并包含各阶段 SPIR-V、已核对反射、依赖及源映射、内容身份、ABI 身份和工具/profile 指纹. 读取方 SHALL 验证完整性、长度/范围和所需 ABI, 以拥有明确寿命的 CPU 数据提供字节码; 失败或中断 SHALL 不使部分更新被识别为成功包, 也不把保留的旧包伪装成新输入成功.

#### Scenario: Read a complete matching package
- **WHEN** CPU 消费者读取完整、受支持版本且 ABI 匹配的包
- **THEN** 它取得与元数据一致的各阶段字节码和反射, 不需要图形 context 或 Engine 专用类型进入公开 RHI

#### Scenario: Reject damaged or incompatible output
- **WHEN** 包被截断、长度溢出、内容摘要错误、版本未知或 ABI 与消费者要求不同
- **THEN** 读取明确失败, 不返回部分可用数据或假成功的默认 Shader

#### Scenario: Fail while replacing a previous package
- **WHEN** 上一次成功包存在, 但新一轮编译、验证或写入失败
- **THEN** 本轮构建失败, 上一次完整包仍保持完整且可识别其旧身份, 修复输入后可以重新构建成功

### Requirement: Build offline with correct incremental dependencies

受支持的 CMake 构建 SHALL 从仓库保留的固定依赖及已声明本机工具生成接口和 Shader 包, configure SHALL 不下载或安装依赖. 生成步骤 SHALL 声明真实输入、输出及传递依赖, 并在 Debug/Release、并行构建和从空 binary tree 构建时保持正确顺序; 不变输入 SHALL 不引起无关 Shader 或消费者的持续重建.

#### Scenario: Build without previous outputs
- **WHEN** 维护者在依赖来源完整、所需工具已安装且网络不可用的支持环境中配置新的 binary tree 并构建
- **THEN** 接口、生成头消费者和 Shader 包按依赖顺序成功产生, 源码目录不新增生成物, 不依赖旧可执行文件或缓存

#### Scenario: Perform a no-change build
- **WHEN** 成功构建后未改变任何有效输入而再次构建
- **THEN** 不重新执行 Shader 编译, 不重写不变生成内容或触发无关消费者重编译

#### Scenario: Isolate dependent work
- **WHEN** 某公共 include、接口或编译配置变化并重新构建
- **THEN** 受影响的包及必要消费者更新, 无依赖关系的程序保持有效, Debug/Release 生成物不互相覆盖

#### Scenario: Remove a required generated output
- **WHEN** 一个声明的生成输出被删除后重新构建
- **THEN** 构建恢复缺失输出或明确失败, 不因残留成功标记而跳过必要生成

### Requirement: Report actionable build failures

接口生成、编译、验证、反射、包读取与输出发布失败 SHALL 与成功可区分. 诊断 SHALL 在适用时包含程序/阶段/入口、源路径/行号或 schema 字段路径、依赖链及失败原因; 不支持输入、资源不足与不可写输出 SHALL 在 Debug 和 Release 均产生受控失败, 第三方异常 SHALL 不越过未声明异常合同的公共边界.

#### Scenario: Diagnose an included syntax error
- **WHEN** 实际 include 中有语法错误
- **THEN** 构建以失败状态结束, 诊断定位到原始文件和行号并标识所属程序/阶段, 不只报告生成临时文件

#### Scenario: Diagnose resource or output failure
- **WHEN** 输入超过声明预算、CPU 分配失败或目标输出无法写入
- **THEN** 操作明确失败并保留可解释的诊断, 不终止为无上下文的成功进程或发布半初始化结果

### Requirement: Deliver a real compile-time conformance consumer

交付 SHALL 包含实际使用生成常量、独立纹理/采样器和跨阶段接口的最小顶点/片元程序, 及消费生成 C++ 数据/绑定与编译包的 CPU 检查入口. 检查 SHALL 覆盖正常、布局边界、ABI 不匹配及构建失效场景, 并明确该证据不能代替后续 native Pipeline、Draw、GPU 布局读回或整阶段验收.

#### Scenario: Exercise generated interfaces end to end
- **WHEN** 从空 binary tree 构建并运行配套 CPU 检查
- **THEN** 实际编译的程序消费生成接口, CPU 端布局检查通过, 最终 IR 反射包含测试实际使用的常量与独立纹理/采样器, 且包可被校验读取

#### Scenario: Report the completed work package
- **WHEN** 本能力按项目审查闭环完成并提交验收证据
- **THEN** 完成声明限于 D03-T01 和实际执行的检查, 不把 CPU 编译成功、公开声明存在或旧原生检查当作后续 GL 绘制能力已经通过
