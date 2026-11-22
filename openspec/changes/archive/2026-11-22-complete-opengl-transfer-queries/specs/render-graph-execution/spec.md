# Spec Delta

## ADDED Requirements

### Requirement: Schedule staging readback with existing graph permissions

图 SHALL 以现有 CopySource Use 和显式读回根安排选定 Texture 版本到外部 Read staging 的传输 Pass。录制 SHALL 仅消费当前 Pass 授权的物理 Texture 与精确范围。staging 由请求调用方拥有，其复制端点按 RHI 的所有权、范围和退休合同验证。图尾 Readback 导出的对象 SHALL 继续允许调用方在图退出后用普通即时复制接入相同的 staging 合同，适用于 Texture 与 Buffer。

图执行、图尾导出和复制记录的成功 SHALL 只交付各自的录制事实，CPU 数据须在接受实际复制所属录制及观察其完成后才可发布。执行失败、裁剪或取消的请求 SHALL 无成功的新结果，其实发前缀 SHALL 按既有退休协议保留。发生在不同列表中的导出与复制 SHALL 遵守真实提交和资源终态协议。

#### Scenario: Read back only the live requested texture version
- **WHEN** 图声明选层的 CopySource 和读回根，将该 Texture 复制到外部 staging，同时包含无根的传输分支
- **THEN** 编译保留声明的生产者和读回，裁剪无根分支；录制实际复制被授权版本，随后同一次有效提交和非阻塞完成观察使 CPU 能读取对应数据

#### Scenario: Reject a staging copy outside the declared graph use
- **WHEN** 回调请求复制超出已授权 mip/layer 的数据，或试图用非 CopySource 用途消费图资源
- **THEN** 无该非法复制和成功的请求结果，图保留实际错误及任何已发前缀，调用方按真实完成或受控退出收拢
