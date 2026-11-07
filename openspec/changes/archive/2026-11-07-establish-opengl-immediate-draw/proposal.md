# Proposal

## Why

[D03-T02](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t02) 已交付真实 Shader/绑定/Pipeline 准备, 但正式 CommandList 的 clear、图形状态、Draw 和 copy 仍返回 Unsupported. 下一工作包 [D03-T03](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t03) 需要让这些对象经同一公共 RHI 即时列表产生可判定的原生结果, 为后续提交寿命与稳定图像门槛提供实际消费者.

## What Changes

- 在现有唯一即时 Graphics 列表上交付基础 clear、SetGraphicsState、Draw/DrawIndexed 及实例绘制, 沿用 Open/Close、recording claim、物理包和 fence 协议; Execute 不重放.
- 消费既有 InputLayout、PipelineBindingPlan、不可变 BindingSet 和 FramebufferInfo, 落实普通 VB/IB、UBO/只读 SSBO、独立 Texture/Sampler 组合及必要的普通 Buffer/二维子资源 copy.
- 扩展唯一 ContextState, 将原生状态缓存、资源访问跟踪和数据更新分别管理. Automatic 与显式用途共享 tracker/backend 发射入口, clear/copy 先满足自身用途再进入 Draw 用途.
- 保持 upper-left/zero-to-one、公共 raster 和颜色语义, 以有限的非对称像素/已知数据检查覆盖 CCW/负缩放、Reverse-Z、viewport/scissor、depth/stencil/blend/write mask、sRGB 以及非零 base instance/base vertex. 复现并解决已有 UPPER_LEFT 区域 depth clear 观测, 不将旧辅助 LOWER_LEFT 结果当作产品验收.
- 把本次新增的真实绘制依赖纳入既有包的保活、完成及删除协议, 仅完成本包安全执行所需接入. 同步修订旧规范中的能力边界, 保留不支持操作的明确拒绝.

范围仅为 **D03-T03**. D03-T04 继续负责 Draw 在途引用、提交/完成/失败/退出的完整验收; D03-T05 负责可复用的稳定图像门槛与阶段出口. 本次不提前完成这两个工作包, 不实现 Renderer/Editor/呈现、RenderGraph、异步读回/查询、volatile/push、UAV/Compute/indirect、多 viewport、MSAA 绘制或扩展 profile.

## Capabilities

### New Capabilities

- `opengl-immediate-draw`: 基线图形对象在唯一即时列表上的真实执行, 包括逻辑/原生状态、实际用途、clear/copy、直接绘制参数和坐标/深度/颜色结果, 以及最小寿命接入与诚实验收边界.

### Modified Capabilities

- `opengl-device-resources`: 将已经准备的对象与本次明确支持的即时图形命令区分, 更新不可用操作和分层验收要求; 基础绘制通过不等于完整渲染、呈现或 D03 阶段出口通过.

## Impact

- 主要实现位置为 RHI/OpenGL 的 CommandList、ContextState 和按职责拆分的 Graphics/clear/copy/state lowering; 直接消费者为现有 Pipeline/InputLayout/Bindings/Framebuffer, 提交/内部 owner 记账和删除入口按需要接入. 通用 Validation 仅补本次执行可触发的公开输入缺口, 保留现有拒绝和委托合同.
- 保留完整公共 RHI 的签名、布局和枚举, 不变更 canonical Shader profile、包/接口 ABI、源扩展白名单或第三方快照; 无新增依赖. CMake 仅显式纳入必要正式实现文件, 通用 RHI 闭包继续独立.
- 遵循 [Architecture 7.6](../../../../Docs/Architecture.md#command-execution)、[7.5](../../../../Docs/Architecture.md#barriers)、[坐标/颜色约定](../../../../Docs/Architecture.md#conventions) 及 [11.1](../../../../Docs/Architecture.md#retirement). 当前不提出架构改动; 无法满足既有坐标合同的发现必须先形成具体纠错决定, 不能以实现便利改写合同.
- 所有开发检查只在 `build/d03-t03/` 归类, 不进入正式源码/默认目标. 复用输入匹配的既有证据, 对新增实际执行风险采用最小充分检查; 独立 B 审查/注释和复核完成后才最终 verify、同步和归档.
