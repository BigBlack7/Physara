# Spec Delta

## MODIFIED Requirements

### Requirement: Compose a sealed UI frame on a supplied RHI target

已接入 renderer 的编辑器 SHALL 从本会话封存的 UI 帧准备自持绘制与上传数据，在调用者的图中声明上传及 UI 合成，并通过图执行器使用公共 RHI 写入目标。基线目标为非零单色、单样本二维 sRGB 附件，合成前 SHALL 有图中可追溯的初始化内容。调用者提供同帧的目标图版本和实际像素范围，并拥有提交和目标用途。UI 返回其写入后的图版本。初始化 SHALL 校验 UI Shader 接口，缺失资源、超出预算或不兼容目标 SHALL 有界报告。无新帧或有效像素范围为零时 SHALL 返回无工作。

#### Scenario: Record a sealed editor frame into an offscreen target

- **WHEN** 已接入 renderer 的会话封存一帧，调用者将兼容离屏目标及其已初始化版本提供给本帧图
- **THEN** 图声明本帧几何和纹理依赖，UI 在已有底色上合成。执行器消费计划且调用者提交有效工作后，可观察目标中的 UI 像素

#### Scenario: Reject incompatible rendering inputs

- **WHEN** UI 程序接口不匹配，或准备使用不兼容的非零目标或超预算输入
- **THEN** 返回指明受影响阶段的有界失败，不发布可提交的 UI 绘制结果

#### Scenario: Cancel a partial preparation on CPU allocation failure

- **WHEN** 批次建立或临时纹理身份已安装后的准备阶段发生 CPU 分配失败
- **THEN** 返回有界失败，撤回本次创建的临时身份，保留待处理请求和像素供后续重试

#### Scenario: Stop after rejection of a draw's graphics state

- **WHEN** 同一批中先有成功绘制，后续命令设置图形状态时收到同步 RHI 错误
- **THEN** 后续命令不发出该次索引绘制，图执行停止，保留已发前缀及其提交或收拢责任

#### Scenario: Leave a paused iteration without UI work

- **WHEN** 窗口暂停而没有新 CPU UI 帧，或本轮有效像素范围为零
- **THEN** UI renderer 不发出本轮绘制或上传，不分配替代目标。待处理纹理请求可由后续有效帧继续处理

### Requirement: Confirm texture requests only after their upload is accepted

Renderer SHALL 独立持有本次 create/update 所需的像素及范围。临时 `TexID` 可以供该次准备解析，内容有效状态 SHALL 在对应上传 Pass 实际执行、完整图执行及颜色交接成功且同批获有效提交后逐请求确认。仅声明或编译入图不构成确认，被裁剪的上传保持待处理。取消、执行或提交失败 SHALL 保留请求和 CPU 数据，清理未发布的临时身份。已发的失败前缀沿实际提交退休或受控退出收拢。

#### Scenario: Publish a managed texture after its first accepted upload

- **WHEN** `WantCreate` 准备出一张供同帧 draw 采样的纹理
- **THEN** 图保证上传先于采样，只有对应上传实际执行且该批获有效提交后才设置 `OK`。提交前的 `TexID` 只供这次准备解析

#### Scenario: Apply an independently owned texture update

- **WHEN** `WantUpdates` 提供一个已验证的更新区域，准备完成后源像素借用结束
- **THEN** 实际命令消费本次拥有的区域字节，受影响 texel 按新内容采样，区域外像素保留，更新前后使用同一有效纹理身份

#### Scenario: Stop recording at a rejected upload

- **WHEN** RHI 同步拒绝本批所需的暂存纹理或几何缓冲写入
- **THEN** 录制在拒绝处停止，依赖该写入的复制或绘制不发出。暂存写入被拒绝时对应驻留区域保留原内容，请求保持待处理，已发前缀沿真实提交或受控退出退休

#### Scenario: Keep a request pending after cancelled or failed work

- **WHEN** 上传被取消、图执行区间报告失败，或该批未获有效提交
- **THEN** 请求保持待处理，临时创建身份失效。即使已发前缀取得票据也只用于保护依赖，下一次安全尝试仍可处理请求

#### Scenario: Confirm only the executed requests in a successful frame

- **WHEN** 一帧中两个独立纹理请求均已准备，其中只有一个上传被存活 UI 消费，另一个被裁剪，且当前帧成功提交
- **THEN** 仅已执行的请求变为 `OK`，被裁剪请求保留像素和待处理状态，其临时创建身份被撤回。后续真正使用该请求的帧可重新准备并确认
