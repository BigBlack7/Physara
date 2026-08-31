## Why

阶段一之后的性能任务目前没有可对照的实测数据，优先级只能靠猜测；同时 CPU↔GPU 契约脚本仍带失效任务编号，后续改绑定或布局时无法可靠拦住漂移。模块 P 必须先锁住测量协议、跑通契约检查，再按电脑 B 的轮次 0 数据复核阶段一性能任务顺序。

## What Changes

- 建立手工性能基线协议：固定场景 `Assets/Scenes/default.scene.json`、相机、分辨率、渲染设置和 Release 构建；三条管线（Forward / Forward+ / Deferred）分别记录 CPU、GPU、各 pass 时间、提交统计和硬件环境。
- 本轮只采集 **电脑 B**（低性能台式）数据，作为后续优化对照。**电脑 A**（高性能笔记本）只预留表位，不阻塞本 change。
- 数据来源为本机性能面板、内置 pipeline benchmark，以及后续由人工提供的三条管线 RenderDoc `.rdc`。视觉对错由人工观察场景后口头/文字反馈，不建 golden 图或自动截图回归。
- 基线记为 **轮次 0**。之后每次性能优化只覆盖旧数字并更新轮次标记，不保留历史副本。
- 完善并运行 `Tools/verify_gpu_contracts.py`：校验结构布局、绑定、枚举、SSBO 前缀与着色器声明；清理脚本和契约头中已失效的任务编号注释（含残留 `P.4`）。
- 根据电脑 B 轮次 0 结果，复核 `Docs/Development.md` 阶段一中性能相关任务的实际优先级；只调整顺序或标注依据，不在本 change 实施那些任务。

无 **BREAKING** 运行时 API 变更。本 change 不引入自动性能脚本、测试框架或新渲染功能。

## Capabilities

### New Capabilities

- `performance-baseline`: 手工性能基线的固定条件、双机表位、三条管线采集范围、轮次覆盖规则，以及面板 / benchmark / rdc / 人工视觉反馈的职责划分。
- `gpu-contracts`: CPU↔GPU 契约必须由 `Tools/verify_gpu_contracts.py` 校验通过；覆盖布局、绑定、枚举与着色器声明，并禁止失效任务编号污染契约源。

### Modified Capabilities

- 无。`openspec/specs/` 当前为空，本 change 不修改既有主规格。

## Impact

- 文档：`Docs/Development.md`、`Docs/TaskDetail.md` 的模块 P 记录；基线数字与测量条件的落盘位置在 design 中确定。
- 工具：`Tools/verify_gpu_contracts.py` 的覆盖面、告警/失败语义和过期注释。
- 契约源：`Engine/Renderer/GPUContracts.hpp`、`Assets/Shaders/Includes/Common.glsl`、`Assets/Shaders/Includes/Material.glsl`（以脚本实际读取范围为限）。
- 测量消费方：Editor 性能面板、`FrameStatistics` / pipeline benchmark、后续 RenderDoc 抓帧。不改渲染算法，不改默认场景内容。
- 下游：阶段一模块 0 / 2 / 3 等性能任务以本基线为对照；GPU 契约变更前后必须重跑校验脚本。
