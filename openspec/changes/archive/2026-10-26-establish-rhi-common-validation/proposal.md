# Proposal

## Why

[D02-T01 已归档](../2026-10-25-establish-rhi-public-contract/tasks.md). 本 change 开始时公共合同可独立编译, Common 外部定义和 Validation 尚不能链接使用. [D02-T02](../../../../Docs/Developments.md#d02-t02) 交付可验证的通用实现、状态记录和校验包装, 为真实 GL Device 提供同谱系入口.

维护者已明确同意本 change 的限定推进方案: 保留 D01 跨屏缺口, 先完成 Common/Validation 通用实现与 CPU 验证; 真实 GL Device 包装、设备 caps 与原生行为验收归 T03/T05, D01/D02 总出口继续未完成. 实施第一项同步当前开发计划, 避免把原先仅覆盖 T01 的例外默认扩大, 并消除 T02 验收依赖 T03、T03 又前置 T02 的顺序冲突.

## What Changes

- 按同一固定 NVRHI commit 移植通用 misc、格式信息、工具、资源状态跟踪和完整 Validation 声明/包装实现; 补齐当前公共头已声明的通用外部定义, 保持来源、许可、完整命名及已修复绑定键语义.
- 将 PhysaraRHI 从公共头 INTERFACE 目标演进为含真实 Common/Validation 源的静态库, 保持消费名称、模块 include 和 C++20 合同, 无 native/backend/业务依赖.
- 提供可组合的校验包装入口, 保留 intrusive CPU 所有权和既有 handle/bool/MessageCallback 失败形式; 明确借用、线程、异常和底层失败传递条件.
- 为 OpenGL 定义初始限制策略: 单 Graphics 队列、即时列表; 扩展、不支持操作与重放明确拒绝. Feature/format 的正支持只来自允许范围内的底层查询, 不从枚举、格式表或 CPU double 推导设备能力.
- 对范围算术、Release 类型/空值检查、批次预检的状态修改和 tracker 错误退出做有限正确性适配, 在 design/来源说明中逐项登记; 不裁剪未来后端声明或重写另一套状态系统.
- 随每项交付链接、正反 CPU 用例和来源对照; 最后完成新目录 Debug/Release 消费者/产品构建及现有宿主 smoke, 按实际证据更新文档和工作包状态.
- 按维护者归档前明确要求统一第一方命名空间: RHI 源子域迁为 Utils、RT/RT::Cluster、CoopVec、Validation 及其 Input/Profile, 无旧拼写 alias. 同步通用规范并去除 RHI/Core/Platform/Runtime 的匿名域, 以所属具名模块和文件内部链接表达作用域, 保持算法、数据和宿主行为.

本 change 不实现 GL 工厂/资源/Shader/提交/fence, 不发出原生 barrier, 不接入 Vulkan/DXGI/Aftermath SDK、RTXMU 或任何生产假 Device. CPU fixture 只证明逻辑, 不替代 T03/T05 的真实设备、caps、资源归属和原生行为验收. 初次 apply 未提交或同步/归档; 维护者本轮已请求完成风格审查、验证后归档, 主规格按归档同步选择执行, 不自动提交 Git.

## Capabilities

### New Capabilities

- `rhi-common-validation`: 通用辅助的实际链接与确定结果、有界索引分配、可组合校验包装、Release 输入拒绝、API/profile 与底层能力区分、命令状态/归属、抽象状态依赖及失败重置, 连同统一域拼写、来源和分阶段验收边界.

### Modified Capabilities

无. 已读取三个主规格的全文: 既有 rhi-public-contract 继续约束完整头、数值/布局、引用、键语义和声明/实现区分; 本 change 提供其中已声明 Common 的后续实现, 不改变这些要求. project-baseline 的无 native/参考依赖泄漏、轻量构建及 native-window-host 的真实宿主合同继续有效.

## Impact

落点为 RHI/Common、RHI/Validation、Validation.hpp、RHI/CMakeLists.txt, 既有公共头的必要域拼写/合同注释, 及 AGENTS、RHI/README、Architecture 7.1/7.2/7.5/7.7、Developments D02-T02/T03/T05 和 Build. Core/Platform/Runtime 仅修正 cpp 私有作用域及必要引用, 生产职责、资源寿命与依赖方向不变, Runtime 不链接未使用的 RHI. 具体方法见 design.

第三方参考、旧活动 change、既有归档和 OpenSpec 配置保持不变. 主规格只在归档的同步步骤中按最终 delta 合并. 临时 fixture、映射、负例与原始输出在 build/artifacts/establish-rhi-common-validation/, 不新增 CI、持久测试框架、Tools 程序或构建时安装/生成门禁. 进入、失败、取消、提交 metadata 与退出协议必须在相应任务验收, 未运行的 native 证据不计为完成.
