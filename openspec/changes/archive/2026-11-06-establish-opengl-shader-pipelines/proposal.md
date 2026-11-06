# Proposal

## Why

[D03-T01](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t01) 已交付权威接口和 canonical Shader 包, 但 GL 的 Shader、BindingLayout/Set、InputLayout 和 GraphicsPipeline 创建仍返回 Unsupported. 本 change 完整覆盖下一工作包 [D03-T02](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t02), 将这些 CPU 输入准备为真实可链接的 GL Program 和可追溯的紧凑绑定计划, 为后续公共 RHI Draw 提供唯一准备链.

## What Changes

- 增加 Engine/Shader 到公共 RHI 的薄包消费入口, 核对预期 ABI 后创建自持 SPIR-V 的 Shader 对象; RHI 不依赖 Engine 或包格式.
- 实现当前 `GraphicsBaselineV1` 对应的 BindingLayout/BindingSet, 保留独立 Texture/Sampler、逻辑集合、可见阶段和资源视图/range, 在创建边界检查归属与实际限制.
- 在 GraphicsPipeline 创建时合并跨阶段活动资源, 生成不可变 `PipelineBindingPlan`, 紧凑分配 UBO/只读 SSBO 和采样 pair 的 texture units, 正确统计每 stage 与 combined limits; 不活动声明不占原生单元.
- 使用固定版本 SPIRV-Cross GLSL backend 将 canonical IR lowering 为 GLSL 460 并原生编译/link, 明确 clip 与 instance/vertex builtin 语义, 同一纹理可搭配多个独立 sampler.
- 实现基础 InputLayout、单样本 GraphicsPipeline 固定状态及 FramebufferInfo 兼容检查, 新对象复用已有设备/context、失败清理和延迟删除协议.
- 精简补入同一 SPIRV-Cross commit 的 GLSL 实现和必要头, 更新实际支持说明与来源记录. 开发检查材料只在 build 下存放.
- 明确 D03-T02 与后续工作分工: 本包建立准备链; D03-T03/T04/T05 继续负责命令执行、在途消费和像素验收. image/UAV/compute 在 D08-T04 扩充, volatile/push 的消费在 D08-T02 扩充. 按维护者 2026-11-06 批准, 仅为已规划 texture-only fetch 补入 `GL_EXT_samplerless_texture_functions` 源扩展白名单; canonical 版本、资源/能力子集、包格式与 ABI 不变.

## Capabilities

### New Capabilities

- `opengl-shader-pipelines`: 已校验 Shader 包消费、GL Shader/Bindings/InputLayout/GraphicsPipeline 准备、活动资源展平、独立采样对、原生 link、兼容性与对象寿命.

### Modified Capabilities

- `opengl-device-resources`: 更新 `Reject unavailable operations without fictitious GPU work`, 将本次实际交付的创建子集从原有 unavailable Shader/Binding 范围中区分出来, 保留 Draw、未交付命令与扩展的明确拒绝.
- `shader-build`: 更新 canonical profile 的源扩展约定, 允许 samplerless fetch 的已批准源写法, 保留未知扩展拒绝及最终 IR profile 核对.

## Impact

- 实现范围为 `RHI/OpenGL`、必要的 `RHI/Validation` 校验、`Engine/Shader` 的包消费适配, 及这些模块的 CMake. 保留公共 RHI 签名、枚举和 descriptor 语义, 不引入公共 CombinedTextureSampler 或 GL 句柄协议.
- `PhysaraRHIOpenGL` 私有依赖 `spirv-cross-glsl`, `PhysaraShader` 的公共包消费接口按实际签名传播 `PhysaraRHI`; 通用 `PhysaraRHI` 继续可独立使用, 不依赖 loader、编译工具链、Core 或 Engine.
- 按 [Architecture 1.3](../../../../Docs/Architecture.md#shader-build)、[7.2](../../../../Docs/Architecture.md#rhi-objects)、[7.3](../../../../Docs/Architecture.md#opengl) 和 [坐标合同](../../../../Docs/Architecture.md#conventions) 实现, 不改长期架构或历史归档. 更新开发计划状态/change 引用, 实施时同步 `RHI/README.md`、`Docs/Build.md`、`ThirdParty/README.md` 的实际能力与来源.
- 验收使用 build 中的有限真实 GL 创建/link/反射检查, 不新增测试框架、默认样例目标、产品入口、Renderer、Editor、呈现或完整 D03 阶段声明. A/B 审查和最终 verify 保持独立, 规划文档完成不代表实现通过.
