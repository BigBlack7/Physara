# Spec Delta

## MODIFIED Requirements

### Requirement: Cancel failed frames without discarding issued work

窗口目标失效、录制或关闭失败 SHALL 取消本帧的复制与交换。末端复制报告 Error 或 Fatal SHALL 取消交换。取得取消 SHALL 保留已发工作的真实提交或受控退出责任。完整 UI 录制及其确认 SHALL 沿同一帧的有效回执, 失败的录制或复制 SHALL 保留未确认的纹理请求。已取得的前缀票据只能用于相应资源的退休。

尚未开放列表的取得取消 SHALL 消耗本次取得身份, 不改变目标的状态事实, 允许后续合法取得。已经开放的录制 SHALL 不作为无工作取得直接归还。普通图失败后, 若原录制可合法关闭和提交、需要再次使用的其它持久导入仍有可靠状态、设备仍可用、无 Fatal 且完成观察成功绑定, 宿主 SHALL 消耗本次取消并允许后续帧继续。失败目标的未知内容和结束状态 SHALL 不被猜测为可复用; 对应槽完成前保留旧目标及依赖, 真实完成后在下一次使用时更换目标代并从已知初态建立新颜色。

关闭或提交被拒绝、无有效票据、完成观察绑定/查询失败、末端原生复制失败或 Fatal SHALL 停止新帧并沿既有关闭协议收拢。有效票据与可靠完成观察 SHALL 分别记录, 即使取得非零票据, 后续观察失败也不得复用该槽。正常取消恢复 SHALL 不增加帧槽或引入全设备等待。

#### Scenario: Invalidate a frame before the terminal copy

- **WHEN** 颜色准备、录制或关闭被拒绝, 或取得的尺寸在末端复制前已过期
- **THEN** 本帧没有末端复制或交换, 可能已发出的前缀沿原设备的接受或收拢路径处理

#### Scenario: Fail a native terminal copy

- **WHEN** 末端原生复制实际报告错误
- **THEN** 保留有界诊断, 禁止交换和本帧 UI 上传确认, 停止新帧并保活所有可能已发工作直到受控收拢

#### Scenario: Receive no accepted submission after the copy

- **WHEN** 复制发出后本次 Execute 未发布有效票据
- **THEN** 不交换、不报告有效完成, 停止新帧并保留未证明完成的引用至受控退出

#### Scenario: Cancel an acquisition before opening the list

- **WHEN** 主图与 fallback 均在执行前失败, 宿主仍可用且尚未关联或开放列表
- **THEN** 本次 token 被消费且后续重复使用被拒绝, 没有复制、提交或交换, 目标入口状态不推进, 下次可合法取得

#### Scenario: Retire a recoverable prefix and replace its target

- **WHEN** 普通图错误前已经发出合法工作, 原列表可关闭并获有效前缀票据及可靠 Event, 其它待复用持久导入状态可靠且设备没有 Fatal
- **THEN** 本次取消不复制或交换, 该槽及目标在真实完成前保持在途, 其它安全槽可继续; 完成后该槽以新目标代从已知初态进入正常帧, 原 token 不再有效

#### Scenario: Preserve an accepted ticket when its observer cannot be bound

- **WHEN** 失败前缀已经获非零票据, 随后的完成观察绑定或查询报告失败
- **THEN** 已接受票据和所有引用仍有 owner, 停止新帧和槽复用, 不把错误当作普通 Pending 或完成, 受控退出保留原始原因
