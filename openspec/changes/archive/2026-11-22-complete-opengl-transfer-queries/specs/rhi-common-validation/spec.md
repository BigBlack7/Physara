# Spec Delta

## ADDED Requirements

### Requirement: Reject invalid staging and transfer input before affected work

OpenGL Validation SHALL 在可由公共数据判定时核对录制、必要句柄和输出、CPU 访问模式、copy 的源和目标方向、格式、样本、extent、mip/layer、byte range、块范围及 typed clear 的资源角色。用于 resolve 或 mip 生成的显式范围 SHALL 在解析前通过边界检查。受支持的请求 SHALL 保留给定范围和底层结果。必要的设备归属、实际能力、映射借用、在途状态和已完成数据的准入 SHALL 由后端判定，raw 路径 SHALL 同样在受影响原生操作前拒绝非法输入。Debug 与 Release SHALL 遵守相同公开边界。

#### Scenario: Reject representable but semantically invalid ranges and modes
- **WHEN** 已打开列表或设备收到相同字节数的错误格式、显式超范围 mip/layer、不匹配的 staging 方向或缺少必要 pitch 输出
- **THEN** 从公共数据可知的非法调用在委托前诊断，后端检查失败不发出受影响工作，旧数据及输出保持，合法调用仍按给定范围委托

#### Scenario: Preserve a backend map rejection
- **WHEN** 公共描述和参数合法，但 raw Device 判定该 staging resource 属于别的 Device、未完成或仍然映射
- **THEN** 包装路径保留底层失败结果及对应诊断或 Pending 边界，没有替代指针、pitch 或完成身份
