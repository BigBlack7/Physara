# Proposal

## Why

D02 已完成资源、上传与原生一致性基线, 但项目尚无权威 Shader 接口、canonical IR 或可重建的 Shader 编译入口. [D03-T01](../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t01) 必须先消除 CPU/Shader 布局和编译输入的歧义, 才能为后续 GL Pipeline 与首次 Draw 提供可信输入.

## What Changes

- 仅交付 D03-T01: 在 Engine/Shader/Interface 建立单一声明 schema, 生成 C++ 数据布局及检查、canonical GLSL、RHI BindingLayoutDesc、反射预期和 ABI 身份.
- 在 Engine/Shader 内集中实现 include 解析、受限且明确的编译 profile、固定工具链、canonical Vulkan SPIR-V 生成、验证、最终 IR 反射与接口核对, 输出可定位的失败诊断.
- 通过模块 CMake 声明生成步骤、真实输入/输出及传递依赖, 在 build 树输出可校验的 Shader 编译包, 支持干净构建和正确的增量失效.
- 交付一个实际使用生成常量、独立 Texture/Sampler 和跨阶段接口的顶点/片元测试程序, 配套 ABI 不匹配、依赖变化与失败后重试检查. 本包的验收止于 CPU 编译/反射与构建证据.
- 按项目闭环安排 A 本地实现检查、独立 B 审查/注释、A 整改、B 复核及最终 OpenSpec verify, 再同步/归档.

## Capabilities

### New Capabilities

- `shader-build`: 从单一接口声明及完整编译输入生成一致的 CPU/Shader 合同和 canonical 编译包, 拒绝布局/资源/阶段接口不匹配, 并提供可重建、可追踪的构建行为.

### Modified Capabilities

无. 现有 `project-baseline` 与 `rhi-public-contract` 的离线构建、依赖方向和完整公开接口要求继续适用, 本次不改变其要求.

## Impact

- 预计实现范围: Engine/Shader、Engine/Shader/Interface、Shaders/Common、最小测试 Shader 入口、Engine 与 ThirdParty 的 CMake 集成, 以及对应构建/依赖来源说明.
- 新增依赖范围: 固定源版本的 glslang、SPIRV-Tools、SPIRV-Headers、SPIRV-Cross core 反射库; 复用现有 JSON 依赖. 编译所需脚本解释器须明确记录, configure 不下载或安装工具/依赖, 不要求 Vulkan SDK 或原生 Vulkan backend.
- Shader 模块可依赖 Core/RHI 与编译库, RHI/OpenGL 不依赖 Engine 的包类型. 生成器不链接消费其生成物的 Renderer, 不新增独立 Tools 模块、测试框架或第二个产品入口.
- 非目标: D03-T02 的 GLSL 460 lowering、native Shader/Bindings/Pipeline, D03-T03 的 Draw/坐标验收, D03-T04 的在途寿命, D03-T05 的 GPU 图案读回, D04 的产品呈现, 以及 D08 的热重载、通用变体缓存和 compute/UAV 执行.
- 维持 [Architecture 1.3](../../../Docs/Architecture.md#shader-build) 与 [8.1](../../../Docs/Architecture.md#shader-runtime) 已采纳方案. 当前仅新增 change 规划产物; 不更新架构、工作包完成状态或审查问题单, 不声明 GPU 行为已经实现或验证.
