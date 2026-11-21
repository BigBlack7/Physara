# Proposal

## Why

D08-T02 已建立版本化上传和在途保活，D08-T03 已交付固定数量的 indexed indirect 消费。D08-T04 为后续 Renderer 计算 Pass 补齐真正的 Compute Pipeline、存储访问和同步，使 Shader 产出的数据能安全地交给采样、间接绘制及传输。 [D08-T04 工作包](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t04) · [D08-T02 验收](../2026-11-20-complete-versioned-gpu-residency/tasks.md#final-verification) · [D08-T03 验收](../2026-11-20-complete-draw-batching/tasks.md#final-verification)

## What Changes

- 通过现有 Shader 接口、包和 Vulkan SPIR-V lowering 交付有限 Compute profile。使用公共 `ComputePipeline`、`ComputeState` 和直接 `Dispatch` 在现有 Graphics 即时列表执行，按真实设备能力校验 Shader 工作组、调度数量和活动绑定。
- 交付有界的 structured SSBO 和单样本颜色 image 访问。显式区分读、写与读改写，并核对实际字节范围或 mip/layer、格式、GL 单元与设备限制。
- RenderGraph 以显式存储访问声明、内容版本和精确使用范围编排 Compute。Automatic、Explicit 和 GraphManaged 共用 RHI 的原生同步发射路径。为每个后继消费域跟踪 Shader 写入可见性，并为重叠的后继 Shader 写保留先前访问的顺序约束。
- 用真实 GPU 数据和原生命令观测验收 Compute → 采样、Compute → indexed indirect、Compute → 更新/复制、连续 Dispatch 及读后写。验证拒绝边界、图裁剪和已发前缀的实际退休，能力表只记录验收覆盖的组合。

## Capabilities

### New Capabilities

- `opengl-immediate-compute` 定义直接 Dispatch、存储访问的多消费域同步、即时录制和 GPU 保活。

### Modified Capabilities

- `shader-build` 接通单计算阶段的 canonical 接口、反射和包构建。
- `opengl-device-resources` 定义有限 Buffer/Texture 存储用途、目标与格式能力以及可执行边界。
- `opengl-shader-pipelines` 预解析 Compute Pipeline、精确 SSBO/image 绑定及其活动映射。
- `opengl-immediate-draw` 延续现有 Draw 和传输入口对 Shader 写产出的实际消费依赖。
- `rhi-common-validation` 定义 Compute 候选、存储输入及调度调用的公共预检。
- `render-graph-contract` 表达存储读取、完整写入与读改写的精确授权和内容版本。
- `render-graph-compilation` 校验存储资源能力及重叠访问，并排定存活生产与消费。
- `render-graph-execution` 将 Compute 用途交给已有单一同步路径，按当前 Pass 验证实际访问并延续失败清理。

## Impact

- 涉及 Engine/Shader、RHI/OpenGL、RHI/Validation、Engine/Renderer/RenderGraph 及 RHI 能力说明。使用现有公共类型和 Graphics 队列，不引入平台新队列或修改公开 ABI。
- Compute 的首批范围是 canonical 单计算阶段、直接 Dispatch、有限 structured Buffer 与颜色 image，以及与现有 graphics/transfer 的同步。间接消费使用 D08-T03 的 `DrawIndexedIndirect`。间接 Dispatch、typed/raw UAV、图形阶段 UAV、其它扩展计算 profile 及定制渲染算法仍属后续范围。传输与公开 staging/query 扩展归 D08-T05，资源池和预算扩展归 D08-T06，变体和热重载归 D08-T07。 [D08 阶段计划](../../../../Docs/Developments/D08-D11-RenderPaths.md#dev-d08) · [同步合同](../../../../Docs/Architecture.md#barriers) · [能力合同](../../../../Docs/Architecture.md#capabilities)
