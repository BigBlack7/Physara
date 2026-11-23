# Spec Delta

## ADDED Requirements

### Requirement: Report Shader preparation and generation status independently of scene acceptance

Shader 与原生准备的事实 owner SHALL 提供有界只读快照，包含当前 Active 代、当前候选请求及其 Pending 或 Failed 状态、程序与变体、可用源路径和行号或实际原生诊断位置。快照 SHALL 区分声明的变体规模、实际准备需求、CPU 包与原生缓存的条目和限额、命中、实际编译或创建次数以及 CPU 编译和 context 线程准备耗时。因额度顺延、压力、失败或丢弃过期结果 SHALL 有对应原因。同一时刻的准备状态及最近一次接受的场景统计 SHALL 保有各自来源。

Editor SHALL 通过现有状态或统计面板显示这些事实及原因。显示开关 SHALL 只影响面板构建，不能触发编译、原生命令、阻塞查询或改变提交及回收次序。

#### Scenario: Show a failed candidate beside a usable active generation

- **WHEN** 一次有效旧代仍在渲染，而新候选先耗尽本轮准备额度，随后出现编译或原生准备错误
- **THEN** 诊断先显示 Pending 的预算原因，失败后显示关联请求和源位置。Active 代及最近接受场景的票据独立可辨，实际执行的编译与 Pipeline 创建分别计量

#### Scenario: Observe reload without changing work through panel visibility

- **WHEN** 在相同输入和请求顺序下显示或隐藏统计面板
- **THEN** 编译、准备额度、代的发布与接受统计来源保持同一合同。面板读取快照仅产生自己的界面构建成本
