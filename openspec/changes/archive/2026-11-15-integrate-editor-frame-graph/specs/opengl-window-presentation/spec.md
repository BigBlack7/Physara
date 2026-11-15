# Spec Delta

## MODIFIED Requirements

### Requirement: Acquire a bounded window presentation target

宿主 SHALL 在所属线程按 Runtime 提供的有界帧槽索引为可渲染窗口取得与实际 framebuffer 像素尺寸一致的 `PresentColor`。每个索引 SHALL 只取得对应槽的目标，槽仍在途时返回可重试状态。只有首次使用或该槽上次提交已经真实完成时，宿主才可复用或更换目标。基线目标 SHALL 为单 mip、单样本二维 `SRGBA8_UNORM` 颜色附件，导出为 ShaderResource。绘制消费者接收公共 RHI 目标、像素范围及 owner 明确提供的入口和出口状态，默认 framebuffer SHALL 留在原生呈现边界。取得身份 SHALL 关联当前宿主、帧槽索引、目标代和本次取得，仅可结束一次。槽索引和取得身份 SHALL 在 Debug 和 Release 的宿主入口校验。零像素、暂停或暂不可用 SHALL 返回明确状态而无可录制目标。

目标的入口状态 SHALL 对应实际已建立的 RHI 状态。新代尚未被提交使用时使用 `Common`，已被接受并正常交接的目标后续使用 `ShaderResource`，出口为 `ShaderResource`。仅取得或未执行取消不得推进状态事实。输入内容有效性 SHALL 独立表达，当前产品帧在图中先清除或完整覆盖窗口颜色。

#### Scenario: Acquire a renderable window target

- **WHEN** 窗口有非零像素范围且调用者指定可用呈现槽
- **THEN** 返回该槽中与当前像素尺寸匹配的 RHI 目标、明确进出状态及有效取得身份，首个写入清除或完整覆盖颜色

#### Scenario: Suspend acquisition without a substitute target

- **WHEN** 窗口最小化或像素范围为零，随后恢复
- **THEN** 暂停期间无目标取得、分配、复制或交换，恢复后按新一轮观测的像素范围取得目标

#### Scenario: Reject an expired presentation identity

- **WHEN** 调用者指定越界槽索引，或提交已经取消、结束或属于其他宿主、槽或目标代的取得身份
- **THEN** 返回可诊断拒绝，不发出原生复制或交换，也不消耗另一份有效取得

#### Scenario: Rebuild one slot using the newest framebuffer extent

- **WHEN** 同一窗口连续改变像素尺寸，指定的呈现槽仍在途，随后真实完成
- **THEN** 待完成期间该槽的源和目标代保持有效，取得恢复时按最新有效像素尺寸成组重建该槽的目标，其它在途槽保持各自的目标和票据

#### Scenario: Describe first use cancellation and accepted reuse

- **WHEN** 首次取得的新目标在执行前取消，之后重新取得并完成有效图提交，再于真实完成后取得同一代
- **THEN** 首次和取消后的入口均为 `Common`，有效交接后的入口为 `ShaderResource`，各次出口均为 `ShaderResource`，取得次数本身不代表内容有效或状态已转换
