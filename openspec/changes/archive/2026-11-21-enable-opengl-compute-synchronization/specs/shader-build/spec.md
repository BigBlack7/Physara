# shader-build Spec Delta

## MODIFIED Requirements

### Requirement: Compile against a declared canonical profile
系统 SHALL 使用显式的源语言、入口、阶段、目标环境、IR 版本和编译选项生成并验证 canonical Vulkan SPIR-V. 基础 profile SHALL 限于后续 GL 4.6 lowering 可承接的已声明公共子集; 超出 profile 的阶段、资源或能力 SHALL 明确失败, 不因工具能够编译而自动成为项目支持能力. 源扩展白名单 SHALL 允许 `GL_GOOGLE_include_directive` 与 `GL_EXT_samplerless_texture_functions` 的 enable/require, 以便 texture-only fetch 无需声明虚构的逻辑 Sampler; 其它源扩展 SHALL 拒绝. 新增源写法 SHALL 保留 Vulkan 1.1/SPIR-V 1.3、既有资源类型、包格式和数据/绑定 ABI, 最终 IR SHALL 仍满足既有能力与接口约束.

`ComputeBaselineV1` SHALL 使用固定 `main` 入口的单个 Compute stage、固定工作组尺寸与受支持的存储资源。Compute 包沿用 Vulkan 1.1/SPIR-V 1.3、完整发布和身份检查。现有 `GraphicsBaselineV1` 程序继续按其 Vertex/Fragment profile 构建。

#### Scenario: Compile the baseline graphics program
- **WHEN** 合法的顶点/片元程序使用支持的常量、只读 buffer、独立纹理/采样器及阶段接口
- **THEN** 各阶段输出通过所声明目标环境验证的 SPIR-V, 并记录实际入口、阶段及 profile 身份

#### Scenario: Reject an unsupported capability
- **WHEN** 输入请求基础 profile 之外的 descriptor indexing、subpass input、push constant、与请求 profile 不匹配的阶段或未声明扩展
- **THEN** 编译包构建失败并说明不支持的请求, 不隐式启用、更换目标或改写源文本来伪装兼容

#### Scenario: Compile a texture-only fetch without a logical sampler
- **WHEN** 基线 Shader 显式请求已批准的 samplerless 源扩展, 并在不构造逻辑采样对的情况下执行合法 texture-only fetch
- **THEN** 正式包编译链产生通过验证的 canonical IR, 活动反射保留所需 Texture 而不为该 fetch 制造 Sampler, 原生后端仍按已声明 dummy 组合准备, ABI 不因源写法改变

#### Scenario: Compile and load one compute stage

- **WHEN** 通过包构建入口提交带权威生成接口的 Compute 程序，含普通 UBO、定长结构化存储块和显式格式的 image
- **THEN** 生成的单阶段包经过 canonical 验证并携带入口、profile、工作组信息与 ABI 身份。包读取方可按 Compute profile 完整取得 CS 字节码，按图形 profile 加载时明确拒绝

### Requirement: Verify final IR against the declared interface

系统 SHALL 从最终发布的 SPIR-V 反射资源与数据布局, 核对活动资源的类型、set/binding、可见性以及成员 offset、矩阵/数组 stride 和块范围, 并核对跨阶段的输入输出兼容性. 所有活动业务资源 SHALL 能追溯到权威接口; 生成预期与最终 IR 不匹配 SHALL 阻止发布成功编译包. 不活动的合法声明 SHALL 不被误判为必须占用原生资源.

Compute 的最终反射 SHALL 核对实际工作组尺寸、活动结构化块的访问方向与布局，以及 storage image 的维度、显式格式和访问方向。生成的 CPU 布局、canonical GLSL 声明与逻辑资源身份 SHALL 出自同一接口。反射结果 SHALL 拒绝会扩大声明权限的存储访问。

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

#### Scenario: Reject mismatching compute storage reflection

- **WHEN** 已声明的 image 格式、数组维度、读写方向或 SSBO 记录布局与最终活动 IR 不符
- **THEN** 编译指出冲突的逻辑绑定和预期 ABI，拒绝发布该次产物
