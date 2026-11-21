# opengl-immediate-draw Spec Delta

## MODIFIED Requirements

### Requirement: Satisfy actual access uses independently of native cache hits

Automatic 与显式用途 SHALL 使用同一资源跟踪和原生同步发射权威, 资源状态与 native 状态缓存、数据更新 SHALL 表达不同事实. clear/copy SHALL 先满足自己的 CopyDest/CopySource 用途, Draw SHALL 满足当前实际附件/vertex/index/constant/SRV 用途. 缓存命中或数据更新未改变绑定身份 SHALL 不跳过依赖. 显式模式 SHALL 要求调用方已请求并提交本次用途, BeginTracking SHALL 不代替转换; 非法跟踪 SHALL 阻止有效执行/提交, Close 的初态恢复 SHALL 满足同一合同.

同步 SHALL 基于实际生产者、后继域与重叠访问, 不为当前无 Shader 写入的有序访问无条件发全域 barrier 或等待. 重叠采样与可写 attachment 的非法 feedback SHALL 被拒绝, 不能以宽泛 barrier 使其合法.

当图形或传输操作消费已支持 Compute 的 Shader 写入时 SHALL 按 `opengl-immediate-compute` 的可见性合同同步实际用途，包括缓存命中的 indirect 参数、普通更新和复制。其它条件相同的无 Shader 写入链路 SHALL 继续遵守其现有原生顺序。

#### Scenario: Consume updated data without another graphics-state call
- **WHEN** 在一次 SetGraphicsState 后更新已绑定的普通 Buffer/Texture, 或 clear/copy 后再次 Draw 而不重新设置同一状态
- **THEN** 新数据和正确图形状态被实际使用, clear/copy 用途先于后续 Draw 用途成立, 状态缓存命中不掩盖该顺序

#### Scenario: Use the explicit access path without duplicate inference
- **WHEN** 同一合法序列分别使用 Automatic 和关闭自动推导后的显式用途/commit
- **THEN** 两者得到一致原生结果并各只由一个权威满足依赖, 缺失或错误的显式用途不被静默自动补成成功

#### Scenario: Reject overlapping sampling and writable attachments
- **WHEN** 图形状态使同一底层重叠子资源同时被活动采样并作为可写附件
- **THEN** 受影响绘制在发出前被拒绝, 显式 copy/ping-pong 的合法后继访问不因同一对象身份被误当作该反馈

#### Scenario: Use compute-produced indexed arguments without changing the binding

- **WHEN** Compute 就地更新合法参数 Buffer，恢复有效 GraphicsState 后用现有 `DrawIndexedIndirect` 读取实际字节区间
- **THEN** 有序图形结果对应新命令记录。原生间接 Draw 以实际生产和授权为前提，消费其它用途的同步不会代替命令消费依赖
