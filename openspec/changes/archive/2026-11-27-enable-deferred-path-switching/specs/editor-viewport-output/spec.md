# Spec Delta

## MODIFIED Requirements

### Requirement: Confirm effective viewport settings from accepted output

View SHALL 分别公开最新合法请求比例、最后接受的有效比例及其输出像素范围。首次确认前有效值 SHALL 显示为尚未确认。待确认的请求缺少可用布局、槽或暂时可用的上传容量时 SHALL 报告 Pending 及原因。请求尺寸超出设备能力、有效像素范围或单目标预算时 SHALL 报告具体拒绝原因。两种结果均保留最后接受的有效状态。只有匹配当前请求的测试图或 正常场景预览实际用于完整正常图合成且该提交被接受后，View 才可发布其有效比例和尺寸。窗口交换结果 SHALL 与该确认独立。正常帧无匹配颜色时 SHALL 沿现有诊断占位合同呈现。

正常候选 SHALL 匹配其所需的输出像素范围和有效图像矩形。正常场景预览还 SHALL 匹配当前 Scene 内容、相机、预览模式、AA 请求以及渲染和显示布局，以该次实际渲染的快照和显示矩形提供图像映射。就绪的请求策略或仍有效的原策略 SHALL 以其实际正常输出确认这些值。原策略正常交付期间，最新路径请求 SHALL 继续显示自己的准备状态。View Settings SHALL 分别显示合法的 AA 请求和最后一次被接受场景实际使用的 AA、显示 profile、渲染尺寸和输出尺寸；首次场景确认前这些有效值 SHALL 标为尚未确认。Physical Lighting SHALL 消费有效的 None 或 FXAA；诊断模式 SHALL 确认实际使用的 None，说明该模式下 FXAA 不生效，保留合法请求。诊断测试图只确认自身比例和有效布局。相机或请求变化时 SHALL 等待匹配候选被采纳，准备失败或等待期间保有最后接受的输出及其矩形。

安全 UI 图读取旧输出或显示占位 SHALL 不确认当前请求, 即使旧输出的尺寸碰巧相同。最近主图失败及使用旧输出的状态 SHALL 可见, 原有效比例、尺寸和颜色代保持不变; 后续正常图成功才能清除该请求的失败/等待状态并发布匹配结果。所选显示 profile、FXAA 或缩放准备或执行失败时，当前状态 SHALL 提供可定位原因，旧结果的有效配置 SHALL 保持与旧图匹配。没有当前有效图像时图像输入映射 SHALL 不可用。

#### Scenario: Keep the last effective setting while a request is pending

- **WHEN** 新请求已经应用, 但视口隐藏、当前没有可用槽或匹配输出仍等待共享上传容量
- **THEN** 状态报告新请求及等待原因, 保留最后接受的比例和尺寸, 未获得匹配颜色的可见正常视口按现有规则显示占位

#### Scenario: Publish the setting with the matching accepted frame

- **WHEN** 新比例对应的当前颜色在完整正常图合成中被采样，且该次提交已接受
- **THEN** View 发布该比例与实际输出尺寸，测试图在取整后尺寸与有效图像矩形均相同时可沿用已经接受的匹配颜色代，正常场景预览以本次场景、相机和模式的匹配输出为准

#### Scenario: Recover from an unsupported extent

- **WHEN** 当前请求和布局产生的尺寸超出可支持范围, 随后编辑或布局变化产生可准备的范围
- **THEN** 拒绝阶段报告具体范围或预算原因并保留最后有效状态, 恢复后只发布成功接受的当前请求和输出

#### Scenario: Keep requested and effective values distinct during fallback

- **WHEN** 新请求的主图失败, 旧输出通过安全 UI 图成功呈现, 随后一次正常图按新请求成功
- **THEN** 安全图期间请求及失败原因可见而有效值不变, 只有后续正常图的匹配回执推进有效比例、尺寸和输出代

#### Scenario: Replace the matching image layout without resizing the canvas

- **WHEN** 同一输出像素范围改用不同固定 aspect 的相机，有效图像矩形随之变化
- **THEN** 先前内容只具备旧输出资格，当前正常合成采用匹配矩形的候选，接受后才确认该布局和颜色代

#### Scenario: Keep scene preview identity consistent at the same extent

- **WHEN** 当前画布及图像矩形相同，场景内容、相机或预览模式改变，随后新候选等待上传或发生图失败
- **THEN** 先前预览只可从安全 UI 以旧输出标明，当前有效模式和图像映射等待匹配的新候选；该候选实际采样并获接受后，颜色与渲染相机记录一并推进

#### Scenario: Confirm the actual display profile and AA only with a matching frame

- **WHEN** 已有一个接受的 Physical Lighting 输出，用户在相同尺寸请求 FXAA；其首个候选失败或过期，后一匹配图实际采样新输出并成功提交
- **THEN** 在安全 UI 期间可辨认当前请求、原因及旧图实际的 profile 和 AA；有效配置及图像命中由后续完整正常帧一并确认，窗口交换状态独立可见

#### Scenario: Keep the FXAA request while inspecting diagnostic colors

- **WHEN** 保有 FXAA 请求的 View 从已接受的 Physical Lighting 切到一个诊断预览，再切回 Physical Lighting
- **THEN** 已接受的诊断帧报告诊断显示和实际 None，请求仍为 FXAA；匹配的下一物理帧实际执行并确认 FXAA

#### Scenario: Match display and image mapping when a deferred switch overlaps a new viewport size

- **WHEN** 主 View 请求 Deferred 和新的视口尺寸，原有效策略先以当前 FXAA 请求正常交付，随后 Deferred 的匹配颜色被正常图采样并接受
- **THEN** 有效比例、布局、实际 AA 和图像映射各自对应真实接受的颜色。原策略在新尺寸的帧保持自身生产身份，Deferred 确认时显示与该次提交配对的路径及配置
