# render-frame-statistics Spec Delta

## ADDED Requirements

### Requirement: Report accepted shadow production and consumption per view

现有统计 SHALL 报告当前 View 的请求开关、过滤、请求和有效边长、有效预算、计划准入及降级原因。各路 View 的最近正常接受记录 SHALL 带自身 Scene 和提交票据，以及实际消费的分支、过滤、有效边长、使用的组及层数、实际生产及复用的层数、阴影绘制批次与配对深度存储按描述符计算的逻辑字节。当前 CPU 计划字节、已接受图使用的存储逻辑字节、池容量及原生计数 SHALL 各自标明来源和范围。失败、取消或安全 UI SHALL 保留受影响 View 上次正常接受的统计和提交身份。整图 CPU 与 GPU 计时 SHALL 各自仍对应单次提交。

Editor SHALL 沿用只读统计及非阻塞计时。显示与隐藏面板 SHALL 只改变面板构建。GPU 时间只在匹配已接受区间有结果时发布。

#### Scenario: Reconcile planned shadow pressure and accepted shadow work

- **WHEN** 受光 View 首次接受深度并以静态输入复用，随后预算不足使一组阴影被拒绝，且继任帧只有前缀或安全 UI
- **THEN** 接受帧的实际生产、消费和复用计数可与存活 Pass 及其图提交核对。当前规划显示新原因，较早已接受统计仍带原来源。轮询中的 GPU 时长保持原计时状态与身份。

#### Scenario: Keep prior shadow work for a rejected View alongside a newer graph

- **WHEN** 主 View 与离屏 View 同时接受，下一轮离屏 View 的尺寸无效且主 View 继续接受，随后离屏 View 恢复并接受无阴影颜色
- **THEN** 失败期间可同时读取主 View 的新票据、离屏 View 的旧票据及工作量，以及本轮离屏准备的拒绝原因。离屏 View 恢复后报告自己的新票据与零消费。
