# Spec Delta

## ADDED Requirements

### Requirement: Declare and authorize indirect argument reads by exact range

图 SHALL 在允许 indirect 参数用途的 Buffer 上提供独立的 `IndirectArgument` 读用途，引用明确版本及实际使用的非空字节区间。声明与执行授权 SHALL 校验 Buffer 的角色和范围，映射至公共 RHI 的 `IndirectArgument` 状态。同一 Buffer 的普通 Shader 读取许可 SHALL 无法授权参数消费。间接读取的生产、覆盖、活性及 Pass 查询 SHALL 使用既有精确范围规则，支持上传生产的版本和 owner 已声明有效的导入版本。

GraphManaged 中，录制方 SHALL 依据对应 Pass 的用途和当前物理 Buffer 对实际间接参数范围再次核对。实例等 Shader 数据 SHALL 保持各自 Shader 读用途。由准备阶段可知的几何和绑定范围 SHALL 与该批实际使用的资源版本一致。

#### Scenario: Order the retained upload before both surviving consumers

- **WHEN** 本图在同一参数 Buffer 中写入两个独立范围，深度和颜色都读取第一范围，第二范围只服务于后来被裁剪的分支
- **THEN** 两个存活 Pass 引用相同的实际生产版本和所需状态，第一范围的写入在两次读取前执行，第二个独立写入与消费都被裁剪

#### Scenario: Deny an indirect range or role outside the declaration

- **WHEN** Pass 试图从错误用途的 Buffer 建立参数读取，或者在一条合法的 indirect 声明旁用不同版本、另一用途或跨出区间的命令参数发起访问
- **THEN** 角色错误在声明阶段拒绝，未授权的查询或录制在发出相应原生 Draw 前拒绝。合法的同版本、同用途、完整范围可以执行
