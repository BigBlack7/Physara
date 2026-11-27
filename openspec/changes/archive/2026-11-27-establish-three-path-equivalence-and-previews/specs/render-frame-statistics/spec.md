# Render Frame Statistics Spec Delta

## ADDED Requirements

### Requirement: Attribute opaque attachment and route workload to the accepted view

现有正常场景统计 SHALL 按所选 View、实际路径与提交票据报告主深度、GBuffer 及不透明工作色的逻辑附件字节，并公开按存活 Pass 的声明范围计算的附件相关读量和写量。逻辑附件存储以 View 中存活的纹理为单位，同一纹理的多个图版本计一次。读写量按实际使用的 Pass 和范围定口径，清楚标为逻辑指标。诊断颜色的需求可单独辨认，普通路径的统计保持同一边界。整图逻辑槽、池容量、原生分配和实际上传继续保有各自的来源与单位。

相同接受快照 SHALL 报告该 View 的可绘不透明表面数、其中按材质归为 lit `ForwardExtension` 的项数和 Unlit 项数，并能由这两个路由计数与总数得到各自的项数占比。可绘项以当前冻结工作集为准。分母为空 SHALL 表示为不适用。只有新正常场景获接受才 SHALL 替换已有快照，等待新路径时仍按本轮实际接受的策略记录。统计面板沿用只读合同。

#### Scenario: Account for the actual attachments of the three paths

- **WHEN** 先后接受尺寸相同的 Forward、Forward+ 和 Deferred 帧，且 Deferred 有存活的核心与辅助表面生产
- **THEN** 两条前向策略报告实际 D32 与工作色。Deferred 以相应格式独立报告 G0 至 G3，终版表面深度和普通深度角色指向同一 D32 时只计一次存储。每项逻辑读写能追到其存活使用，共享附件的多次使用与一次逻辑存储可分别核对。

#### Scenario: Keep route counts and scope tied to the accepted view

- **WHEN** 主 View 与另一 View 的可绘项或路径不同，随后发生仅安全 UI 接受或仅另一 View 正常接受的帧
- **THEN** 所发布的附件和路由计数来自该正常统计所标明的 View 和票据。Unlit 项单独可辨，lit 前向扩展占比可由实际项数复算。最近正常场景统计在仅有安全图时保留原身份。

### Requirement: Compare opaque path cost from matched accepted samples

三路径的固定输入比较 SHALL 在通过同口径画质门槛后记录实际有效策略、工作集、画质、运行环境和采样范围。每组 SHALL 报告 CPU 工作及等待、匹配 GPU 区间、附件与实际操作计数、ForwardExtension 占比和实际 Froxel 工作量。CPU 和 GPU 的有效样本各有分布与样本量。缺失、跳过和失败的查询 SHALL 可辨认，GPU 统计只采用同一运行内可追溯到对应接受票据的原区间。等价、资源或成本结论 SHALL 以实际登记的输入、设备及口径为限。

#### Scenario: Compare matched quality under normal and full-table fallback workloads

- **WHEN** 以同一 Release 构建、可见灯序列、场景、相机与显示设置，在三策略采集等质暖帧并单列实际溢出对照
- **THEN** 每组有可复算的 CPU/GPU 分布及对应的附件、路由、计数和 Froxel 记录。普通与溢出组可按实际查询工作区分。GPU 完成晚到或同轮解析多项时，每条有效样本按自己的生产输入和提交身份归组。
