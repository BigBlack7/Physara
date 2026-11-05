# Physara 开发约定

Physara 是面向静态场景、带轻量编辑器的物理渲染器。先定位当前工作包和它引用的架构章节，只加载本次任务用到的技能。

<a id="document-boundaries"></a>
## 文档入口

- 工作范围、前置和状态从[开发计划的当前阶段](Docs/Developments/Developments.md#current-stage)进入。工作包链接到 [Architecture.md](Docs/Architecture.md) 的具体合同。这是项目唯一的架构指南，仅在设计纠错或正式采纳长期方案时修改。
- 核实上游事实时按主题查 [Filament](Docs/Reference/Filament_Rendering_Pipeline_Analysis.md) 或 [NVRHI](Docs/Reference/NVRHI_RHI_Vulkan_Analysis.md)。构建和运行入口见[构建说明](Docs/Build.md)。
- 在对应 OpenSpec change 中细化可观察行为、设计取舍、实施任务与验收证据。开发计划只记录阶段、前置与范围调整、完成状态和 change 引用。归档保留当时的决定。

<a id="workflow"></a>
## 每次交付

- 一次解决一个可验收目标。读合同和直接消费者，沿已有模块实现。每项状态有明确所有者，依赖单向，新抽象须解决当前问题。例外在 change design 中写清原因、范围和验证方法。
- 按项目锁定的 OpenSpec 流程提案、实施、验证和归档。开始实现前确定可观察结果、实际风险和最小验证。完成后才勾选属于自己的任务，明确实际运行、复用和未覆盖的检查。
- 在信任边界校验外部输入、设备能力和协议参数，Release 也必须拒绝非法操作。内部沿用已建立的合同，用类型或断言表达前提。只为可触发的失败增加处理。同一前提只有在数据、线程或生命周期已经改变时才重新检查。
- 优先运行受影响的既有检查。新增测试须能指出它独自捕获的缺陷。文档改动核对内容与链接。达到本次验收要求且证据对应当前输入时停止。输入变化、检查失败或具体缺口才扩大验证。性能结论必须有同口径测量。
- 开发测试的代码、脚本、样例与临时工程只写入 `build/` 下按工作包归类的目录，不进入正式源码或默认构建；删除 build 后正式工程仍须可重新配置和构建。防御处理与测试规模按实际风险取最小充分范围，细则见对应技能。
- <a id="annotation"></a>第一方 C++、Shader 和 CMake 用中文注释、英文标点。简单功能简述用途，复杂算法说明阶段、关键不变量和失败/寿命条件。修改时同步更新注释。
- <a id="review-loop"></a>归档前必须经过独立会话 **A 开发 → B 审查并注释 → A 整改并自检 → B 复核并完善注释**。维护者调度交接，各自勾选自己的项。B 验收最终版本后才执行最终 verify、同步及归档。交接使用[当前问题单](Docs/CodeReview.md)。
- 历史改写、推送、发布及不可逆删除须有维护者明确授权。

## 按任务加载技能

仅打开命中本次工作的入口。涉及多个边界时组合使用，按 OpenSpec 步骤继续使用仓库已有的原生 skills。

| 正在做的事 | 读取 |
|---|---|
| 修改第一方 C++、模块接口、所有权或代码注释 | [C++ 实现与可读性](.agents/skills/physara-cpp/SKILL.md) |
| 修改 CMake、生成步骤、第三方接入或移植记录 | [构建与依赖](.agents/skills/physara-cmake/SKILL.md) |
| 修改 RHI、资源同步、Shader、渲染或物理输出 | [图形合同](.agents/skills/physara-graphics/SKILL.md) |
| 规划验证、决定是否新增测试、核查性能结论 | [验证范围与停止条件](.agents/skills/physara-verification/SKILL.md) |
| 交接审查、整改、复核或准备归档 | [独立审查闭环](.agents/skills/physara-review/SKILL.md) |
