# Design

## Context

范围见 [proposal](proposal.md). [公共 BufferRange](../../../../RHI/RHI.hpp) 与 [Common Resolve](../../../../RHI/Common/Misc.cpp) 将零长度解释为剩余范围, Validation 核对解析结果后仍把原 range 委托给 backend. [Bindings.cpp](../../../../RHI/OpenGL/Bindings.cpp) 的 `ResolveBuffer` 已先证明 offset<capacity, 但 size 表达式仅将全一哨兵转为剩余长度, 导致零长度恒被拒绝. B 在共享问题单记录 CR-001, 并只补充了6行说明性注释; 接收版本文件 SHA256 为 `f81dc175504fa35cb27143570cfb4edadac1833b63a8010d4b346cc5e0240d9e`.

本修正属于已归档 [D03-T02](../../../../Docs/Developments/D03-D05-FramePipeline.md#d03-t02) 行为, 在当前 [T05](../2026-11-08-establish-opengl-native-draw-gate/tasks.md#review-evidence) 直接消费链中发现. 新 change 负责修复及 delta, T05 保留图像门槛范围. 两者共用现有审查轮次及问题单.

## Goals / Non-Goals

**Goals:** 在原 owner 接受公共合法剩余范围, 保持原始边界、对齐、record stride 和 block limit, 交付匹配修复后输入的最小原生证据.

**Non-Goals:** 不改公共 Resolve 的 clamp 行为、Validation、原生绑定记录类型、Shader ABI 或缓存/退休. 不扩大到其它资源 profile、OOM/设备失效或无关历史矩阵.

## Decisions

### 1. 在已经证明合法的起点计算剩余长度

仅扩展现有 size 分支: 零或全一长度取 capacity-offset, 其它显式长度保持原值; 后续原始长度上界、非空、CB部分范围256-byte约束、UBO/SSBO对齐、记录粒度与设备 block limit 检查不变. 原 offset 校验保证减法安全, 不重复创建算术 helper 或第二份解析状态. 补充零长度来源说明, 保留 B 的全部有效注释.

不直接用带 clamp 的公共 Resolve 替换边界校验, 因为那会把明确越界裁为可绑定范围. 不在 Validation 预改原描述或在 Draw 再解释长度, 因为 raw factory 同样必须满足合同, 且 BindingSet 已拥有一次解析后的不可变记录.

### 2. 复用已有消费者, 每种独立风险一个有限对照

本地资料在 `build/fix-opengl-binding-buffer-ranges/` 分类留存, 回归复用 `build/d03-t05/` 的既有消费者/缓存. 不创建正式测试入口、CMake选项或新框架. 保存修正前交接 metadata/fixture 与 B 接收源字节, 不覆盖旧联合输入所代表的历史.

使用两个512-byte Buffer: CB的{0,0}/{0,512}/{0,all-ones}, 以及stride=32的SSBO的{256,0}/{256,256}/{256,all-ones}; 各自增加一个剩余长度+1的明确越界, raw/Validation 都观察工厂结果与精确解析记录. 这是两种资源类别共享的少量等价/拒绝输入, 不遍历对齐/limit枚举. Release 先记录未修复零长度失败, 修复后 Debug/Release 验证相同输入. 原有默认绑定改为合法零长度后消费固定图案, 预期、尺寸、Shader/JSON和容差不变.

图案的本地 CB 分配容量为256以满足现有公共 CBV 对齐要求, 生成 Parameters 与实际写入仍为64 bytes. 零长度不豁免既有输入约束; 不为使原64-byte分配的这种写法通过而改 Validation 或放宽公共规则.

`Bindings.cpp` 和本地消费者变化后, 重建 Debug/Release 并运行 T05 的规定共享链及 Release 独立进程复现, 不将旧 exe 当作当前实现. 正式 Release 直接消费者按实际影响增量重建/启动. 三种长度表示等价、紧邻越界仍拒绝、既有图案通过且输入身份匹配后停止, 不重跑其它历史矩阵.

### 3. 复核与收尾沿同一所有者闭环

本修正承接 A 的 T05 交接 → B 首轮审查/注释(CR-001) → 当前 A 整改/自检 → B 复核. 不另建审查会话或复写 B 的任务勾选. B 复核时同时确认本修正的完整 Bindings.cpp 注释及 T05 最终输入, 由 B 自行勾选其项; A 分别保留两份 tasks 的真实证据, 验收前不最终 verify/sync/archive.

两份 change 完成 B 最终验收后分别执行最终 verify、对应 capability 同步与归档. 共用问题单只在这轮关联范围都完成归档/留存后重置, 不因其中一份先归档而清除尚未闭环的另一份范围.

## Risks / Trade-offs

- [接受零长度时无意放宽显式越界] → 保留显式长度原值及原 owner 所有条件, raw/Validation 用剩余+1拒绝对照, 不调用 clamp 代替校验.
- [新代码/注释与旧输入身份混用] → 原 A/B 接收基准另存, 重建实际消费者并更新联合输入/包/exe, 原图像数据只作为历史.
- [小缺陷扩大为测试项目] → 只复用既有 build 消费者, 完成有限范围检查及受影响链即停止.

## Migration Plan

在原 owner 修复并交回共享 CR-001, 无 API/资产迁移. B 未通过时继续保留同一编号整改, 不归档或重置问题单. 主规范和历史归档在 B 最终验收前保持不变.
