# Shader Source Profile Spec Delta

## MODIFIED Requirements

### Requirement: Compile against a declared canonical profile

系统 SHALL 使用显式的源语言、入口、阶段、目标环境、IR 版本和编译选项生成并验证 canonical Vulkan SPIR-V. 基础 profile SHALL 限于后续 GL 4.6 lowering 可承接的已声明公共子集; 超出 profile 的阶段、资源或能力 SHALL 明确失败, 不因工具能够编译而自动成为项目支持能力. 源扩展白名单 SHALL 允许 `GL_GOOGLE_include_directive` 与 `GL_EXT_samplerless_texture_functions` 的 enable/require, 以便 texture-only fetch 无需声明虚构的逻辑 Sampler; 其它源扩展 SHALL 拒绝. 新增源写法 SHALL 保留 Vulkan 1.1/SPIR-V 1.3、既有资源类型、包格式和数据/绑定 ABI, 最终 IR SHALL 仍满足既有能力与接口约束.

#### Scenario: Compile the baseline graphics program
- **WHEN** 合法的顶点/片元程序使用支持的常量、只读 buffer、独立纹理/采样器及阶段接口
- **THEN** 各阶段输出通过所声明目标环境验证的 SPIR-V, 并记录实际入口、阶段及 profile 身份

#### Scenario: Reject an unsupported capability
- **WHEN** 输入请求基础 profile 之外的 descriptor indexing、subpass input、push constant、compute 或未声明扩展
- **THEN** 编译包构建失败并说明不支持的请求, 不隐式启用、更换目标或改写源文本来伪装兼容

#### Scenario: Compile a texture-only fetch without a logical sampler
- **WHEN** 基线 Shader 显式请求已批准的 samplerless 源扩展, 并在不构造逻辑采样对的情况下执行合法 texture-only fetch
- **THEN** 正式包编译链产生通过验证的 canonical IR, 活动反射保留所需 Texture 而不为该 fetch 制造 Sampler, 原生后端仍按已声明 dummy 组合准备, ABI 不因源写法改变
