# Proposal

## Why

[D03-T03/T04](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t03) 已分别完成基础 Draw 与提交退休, 但尚未形成固定输入、独立预期和可重复自动判定的完整图像门槛. 本 change 覆盖 [D03-T05](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t05) 全部范围, 为 D03 阶段出口提供 canonical Shader → 公共 RHI Draw → 真实完成 → 读回的集成证据.

## What Changes

- 在既有 graphics capability 中补充整体图像门槛: 同一非对称图案表达四角身份、深度梯度、方向、非零实例及索引偏移, 结果由事先固定的像素/数值预期自动判定.
- 通过正式生成接口、编译包、GL lowering、绑定与即时列表串联 clear → Draw → 同格式 copy → 后继采样及读回, 覆盖不同兼容 Framebuffer 的目标切换.
- 用最少的 Validation 非法输入检查证明集成消费者没有绕过拒绝合同; 复用已覆盖的单项边界, 不重做 T01–T04 测试矩阵.
- 开发 fixture、Shader/JSON、脚本和临时 CMake 工程全部归类到 `build/d03-t05/`, 不加入正式源码或默认构建. 若图案暴露正式链的具体缺陷, 仅在对应现有 owner 修正并记录影响.
- 保留 A 实现/本地检查、维护者调度的 B 独立审查/注释、A 整改及 B 最终复核, 之后才执行最终 verify、同步和归档. 仅完成本次规划不勾选实现或 D03 出口.

## Capabilities

### New Capabilities

无. 复用既有图形能力的准确路径, 不建立第二套图形或测试 capability.

### Modified Capabilities

- `opengl-immediate-draw`: 新增稳定原生图像的端到端交付要求, 明确固定参考输入、跨目标/采样/copy 输出和真实完成后的重复验收. 现有九条逐项执行合同保持不变; T03 局部证据仍不能独自代表 T05 或 D03 出口通过.

## Impact

- 直接消费者是 build 下的有限本地 fixture, 复用 Platform DeviceHost、Engine/Shader 的生成/CMake 与 `LoadRhiProgram`, 以及公共 RHI/Validation/OpenGL 的既有执行和完成链. Renderer 与正式业务 Shader 尚无本包所需生产消费者, 不为工作包的目录提示创建空模块或测试 Pass.
- 正式公共 RHI、Shader profile/包 ABI、第三方快照与模块依赖方向无计划变更. 如实现发现需要改变现有合同, 先更新本 change 的对应 delta 和设计, 不以测试通过为由静默扩大能力.
- 读回仅允许完成后的显式本地冷路径观察. 公共 staging/map/异步 readback、截图/拾取、Editor、呈现、RenderGraph 和 D04 工作保持后续范围.
- 状态只更新开发计划与本 change; 架构依据为 [12.1](../../../../Docs/Architecture.md#tests)、[坐标/颜色约定](../../../../Docs/Architecture.md#conventions)、[8.1](../../../../Docs/Architecture.md#shader-runtime), 不把验证进度写入 Architecture 或历史归档.
- B 首轮发现的已归档绑定范围缺陷 CR-001 由 [fix-opengl-binding-buffer-ranges](../2026-11-08-fix-opengl-binding-buffer-ranges/proposal.md) 独立承接, 本包记录其对图像链的回归和最终输入, 不将存量修正记作 T05 新能力.
