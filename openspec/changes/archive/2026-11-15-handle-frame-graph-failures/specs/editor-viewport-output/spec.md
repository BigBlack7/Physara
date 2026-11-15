# Spec Delta

## MODIFIED Requirements

### Requirement: Resolve a stable viewport token to the current output version

编辑器的主视口 SHALL 在其生命周期内保持稳定的 `ViewId` 和逻辑 `UiTextureId`。视口纹理注册 SHALL 关联 View 身份及输出角色, 当前帧的采样绑定 SHALL 解析到该 View 的有效输出代。视口布局变化及重新显示 SHALL 沿用逻辑身份。已发工作使用的旧纹理和绑定 SHALL 保留至相应完成或受控收拢。

正常帧 SHALL 使用匹配当前请求的输出。独立安全 UI 图 SHALL 显式选择同一 View/角色上次成功接受、内容未被覆写且状态可验证的输出; 该只读旧输出可以按当前面板矩形显示, 即使尺寸或请求代不同, 但 SHALL 标识为上次有效输出。该选择不允许过期 token、其它 View 或未接受候选冒充旧输出。安全图 SHALL 为其重新建立本图采样绑定, 长期状态不保存旧 GraphId。无符合条件的旧输出时 SHALL 使用带原因的占位。

#### Scenario: Resolve the same viewport token after a resize

- **WHEN** 视口由一个有效尺寸变为另一个有效尺寸, 旧代仍被先前的提交采样
- **THEN** 已准备的新正常帧用同一 token 解析新尺寸对应的输出代, 旧提交继续持有原纹理和绑定, 真实完成后原代可退休

#### Scenario: Resolve an expired or missing viewport binding

- **WHEN** UI 引用已失效的会话或 View 身份, 或正常帧没有与请求匹配的可用输出且未选择独立安全 UI 图
- **THEN** 视口呈现可辨认且带原因的占位, 过期的 token 和输出代不能解析到其它 View 或被复用的纹理

#### Scenario: Show a retained output after a resized candidate fails

- **WHEN** 新尺寸或比例的主图失败, 安全图选择同一 View 已接受的旧尺寸输出, 且旧输出仍被此前提交引用
- **THEN** 当前面板只读显示旧像素并明确标注恢复状态, 两次提交均保有各自依赖, 新候选没有覆写旧输出, 正常帧恢复后才可按当前请求发布新代

### Requirement: Confirm effective viewport settings from accepted output

View SHALL 分别公开最新合法请求比例、最后接受的有效比例及其输出像素范围。首次确认前有效值 SHALL 显示为尚未确认。待确认的请求缺少可用布局、槽或暂时可用的上传容量时 SHALL 报告 Pending 及原因。请求尺寸超出设备能力、有效像素范围或单目标预算时 SHALL 报告具体拒绝原因。两种结果均保留最后接受的有效状态。只有匹配当前请求的诊断输出实际用于完整正常图合成且该提交被接受后, View 才可发布其有效比例和尺寸。窗口交换结果 SHALL 与该确认独立。正常帧无匹配颜色时 SHALL 沿现有诊断占位合同呈现。

安全 UI 图读取旧输出或显示占位 SHALL 不确认当前请求, 即使旧输出的尺寸碰巧相同。最近主图失败及使用旧输出的状态 SHALL 可见, 原有效比例、尺寸和颜色代保持不变; 后续正常图成功才能清除该请求的失败/等待状态并发布匹配结果。

#### Scenario: Keep the last effective setting while a request is pending

- **WHEN** 新请求已经应用, 但视口隐藏、当前没有可用槽或匹配诊断输出仍等待共享上传容量
- **THEN** 状态报告新请求及等待原因, 保留最后接受的比例和尺寸, 未获得匹配颜色的可见正常视口按现有规则显示占位

#### Scenario: Publish the setting with the matching accepted frame

- **WHEN** 新比例对应的诊断颜色在完整正常图合成中被采样, 且该次提交已接受
- **THEN** View 发布该比例与实际输出尺寸, 如果取整后的尺寸仍相同则可沿用已经接受的匹配颜色代

#### Scenario: Recover from an unsupported extent

- **WHEN** 当前请求和布局产生的尺寸超出可支持范围, 随后编辑或布局变化产生可准备的范围
- **THEN** 拒绝阶段报告具体范围或预算原因并保留最后有效状态, 恢复后只发布成功接受的当前请求和输出

#### Scenario: Keep requested and effective values distinct during fallback

- **WHEN** 新请求的主图失败, 旧输出通过安全 UI 图成功呈现, 随后一次正常图按新请求成功
- **THEN** 安全图期间请求及失败原因可见而有效值不变, 只有后续正常图的匹配回执推进有效比例、尺寸和输出代
