# Spec Delta

## ADDED Requirements

### Requirement: Report graph admission and pool operation with distinct units

Runtime SHALL 通过现有只读统计提供最近编译的有界图摘要，以及图池当前的预算和占用。摘要 SHALL 标识图代、正常或安全 UI 以及可用的提交身份，并报告声明、存活、裁剪 Pass 数，实际使用的导入和出口数，以及计划的物理槽数和逻辑字节。只有接受后才标为有效接受的提交。失败及当前准入原因 SHALL 独立可辨认，最近有效的正常场景统计保持其原身份。

池 owner SHALL 报告逻辑字节及对象数的硬上限、空闲上限、当前用量和高水位，并分别提供在录制或预留、在途、外部保留及可回收的容量。总容量 SHALL 对同一物理对象计一次。若标记重叠，口径 SHALL 明确可辨。命中、实际新建、淘汰以及因压力未准入 SHALL 按事实发生计数。图内多个逻辑资源共槽和 RHI 实际 native 创建 SHALL 有独立口径。诊断 SHALL 标明逻辑预算的单位及其适用范围。

Editor SHALL 从该有界快照显示池容量、图摘要和最近原因，保持已有隐藏时跳过面板构建的行为。快照和显示 SHALL 不修改图、持有实际资源、查询阻塞结果或改变回收和命令次序。

#### Scenario: Explain reuse and pressure without replacing the last accepted scene

- **WHEN** 代表帧暖机后再次取得兼容槽，随后尺寸或格式切换使正常候选未准入并执行了独立的安全 UI 图
- **THEN** 诊断可核对存活需求、兼容命中、新建及淘汰的实际计数，显示最近候选的容量原因和安全图身份。上一有效场景的提交、图像身份与计时仍可独立识别
