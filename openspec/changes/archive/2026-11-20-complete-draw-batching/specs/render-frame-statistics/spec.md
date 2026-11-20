# Spec Delta

## ADDED Requirements

### Requirement: Report prepared surfaces and executed draw batches with distinct units

Runtime SHALL 在正常接受场景的既有只读快照中区分所选 View 的可绘表面数、实际几何 Pass 使用的批次数及间接记录数. 表面数在该 View 工作集内计一次, 批次与记录数随实际执行的几何 Pass 计, 并可辨认 depth 和 color 的贡献. Renderer 的实例/参数上传 SHALL 按正常计划中实际执行的写入报告次数和字节, 与静态写入和每 View 常量写入区分.

整图原生 Draw、绑定和上传 SHALL 保留原采样范围, 包括该图实际执行的 UI 等消费. 一次 MDI SHALL 只计一次原生 Draw, 它所消费的记录按实际记录数在 Renderer 侧计. 绑定 SHALL 来自实际原生状态变更, 上传 SHALL 来自实际传输. Editor 面板 SHALL 标明计数单位和所属 View 或图. 失败、取消或安全 UI SHALL 保留原正常场景快照, 统计显隐 SHALL 保持现有只读行为.

#### Scenario: Reconcile one instanced command and two commands in a batch

- **WHEN** 一个接受的 View 有多个可绘实例, 一次存活颜色 Pass 以一批包含两个间接记录的 MDI 绘制, 其中一条记录消费多个实例
- **THEN** 该 View 的表面数、批次和记录数各自可核对, 该 MDI 仅增加一次原生 Draw, 绑定及实例/参数上传按各自实际变化和发生次数记录

#### Scenario: Keep batch statistics attached to the accepted work

- **WHEN** 两路候选中有分支被裁剪, 或后续只有失败候选的前缀或安全 UI 得到提交
- **THEN** 已接受的 View/图统计只包括各自实际完成的 Pass 和上传, 后续非正常场景提交保持上一正常场景的身份及统计值
