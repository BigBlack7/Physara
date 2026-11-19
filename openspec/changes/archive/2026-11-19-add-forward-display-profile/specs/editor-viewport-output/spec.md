# Spec Delta

## MODIFIED Requirements

### Requirement: Apply viewport extent at a frame boundary

Editor SHALL 从面板内容矩形和当前实际 framebuffer 对逻辑窗口的各轴比例取得未取整的物理像素范围。正常场景的显示画布 SHALL 对该范围向下取整，渲染栅格 SHALL 对原范围乘当前请求比例后各轴向下取整，并检查有效范围和算术。无场景的诊断测试图仍 SHALL 使用按渲染比例换算的像素范围。窗口 PresentColor SHALL 按实际 framebuffer 像素独立准备。布局及相关资源请求 SHALL 在录制前确定，录制期间到达的变化只供后续帧消费。零面积或隐藏视口 SHALL 不发起新的离屏输出分配。

合成 SHALL 使用该输出携带的布局和有效图像矩形，正常场景使用显示画布的矩形，无场景诊断图使用自身的矩形。图像输入 SHALL 以当前封存布局从逻辑窗口坐标转换到已接受输出的画布，再从该有效矩形取得左上 UV。该映射直接使用已换算的画布尺寸，留边或失效布局返回无有效图像位置。导航命中 SHALL 使用与当前 Scene、相机、预览和 AA 请求以及布局相符的正常已接受图像，安全回退、未获接受的候选、占位和被剪掉的区域 SHALL 没有输入命中资格。画布的可见性和帧边界像素请求保持独立于导航捕获。

#### Scenario: Resize the window and viewport independently
- **WHEN** 连续改变窗口像素尺寸、视口停靠布局或 framebuffer 比例
- **THEN** 新帧的窗口目标匹配最新有效窗口像素范围，正常场景的输出画布与渲染栅格分别对应当帧两个像素请求，诊断测试图使用其按比例换算的范围，内容缩放只计算一次，单独改变布局不重建窗口目标

#### Scenario: Hide and reopen a viewport
- **WHEN** 面板隐藏或内容面积变为零，随后恢复可绘制面积
- **THEN** 空闲期间其余编辑器界面仍可合成，没有为该视口分配替代纹理，恢复时按当前内容矩形解析同一逻辑 View 和 token

#### Scenario: Scale diagnostic pixels independently of window pixels
- **WHEN** 本帧内容矩形有效，View 的渲染比例在准备前改变
- **THEN** 离屏请求使用新比例和各轴实际 framebuffer 比例，窗口目标仍由窗口像素决定，逻辑输入经当帧布局和有效图像矩形映射

#### Scenario: Map a point inside a letterboxed view exactly once
- **WHEN** 当前有效图像有固定比例留边，实际 framebuffer/client 比例和内容缩放不同，render scale 小于 1
- **THEN** 同一可见逻辑点按当前画布和半开的有效图像矩形得到预期 UV，留边、被裁剪区域和右下边界返回无命中

#### Scenario: Wait for a current image after layout or camera changes
- **WHEN** 用户改变大小、切换场景相机或隐藏后重开，安全 UI 或等待状态可能保留旧有效颜色
- **THEN** 旧色可按自身回退合同显示，不能启动新捕获或提供本次有效图像映射；正常图实际采样并接受匹配结果后，同一 View 和 token 可以继续使用

#### Scenario: Keep display pixels at panel resolution when render scale changes

- **WHEN** 正常视口的面板内容折算为 321.8×241.25 物理像素且窗口布局固定，渲染比例从 1 改为 0.75
- **THEN** 输出画布仍为 321×241，渲染栅格变为 241×180；图像命中使用当前已接受的显示矩形，窗口目标继续由实际 framebuffer 决定

### Requirement: Confirm effective viewport settings from accepted output

View SHALL 分别公开最新合法请求比例、最后接受的有效比例及其输出像素范围。首次确认前有效值 SHALL 显示为尚未确认。待确认的请求缺少可用布局、槽或暂时可用的上传容量时 SHALL 报告 Pending 及原因。请求尺寸超出设备能力、有效像素范围或单目标预算时 SHALL 报告具体拒绝原因。两种结果均保留最后接受的有效状态。只有匹配当前请求的测试图或 Forward 预览实际用于完整正常图合成且该提交被接受后，View 才可发布其有效比例和尺寸。窗口交换结果 SHALL 与该确认独立。正常帧无匹配颜色时 SHALL 沿现有诊断占位合同呈现。

正常候选 SHALL 匹配其所需的输出像素范围和有效图像矩形。Forward 预览还 SHALL 匹配当前 Scene 内容、相机、预览模式、AA 请求以及渲染和显示布局，以该次实际渲染的快照和显示矩形提供图像映射。View Settings SHALL 分别显示合法的 AA 请求和最后一次被接受场景实际使用的 AA、显示 profile、渲染尺寸和输出尺寸；首次场景确认前这些有效值 SHALL 标为尚未确认。Physical Lighting SHALL 消费有效的 None 或 FXAA；诊断模式 SHALL 确认实际使用的 None，说明该模式下 FXAA 不生效，保留合法请求。诊断测试图只确认自身比例和有效布局。相机或请求变化时 SHALL 等待匹配候选被采纳，准备失败或等待期间保有最后接受的输出及其矩形。

安全 UI 图读取旧输出或显示占位 SHALL 不确认当前请求, 即使旧输出的尺寸碰巧相同。最近主图失败及使用旧输出的状态 SHALL 可见, 原有效比例、尺寸和颜色代保持不变; 后续正常图成功才能清除该请求的失败/等待状态并发布匹配结果。所选显示 profile、FXAA 或缩放准备或执行失败时，当前状态 SHALL 提供可定位原因，旧结果的有效配置 SHALL 保持与旧图匹配。没有当前有效图像时图像输入映射 SHALL 不可用。

#### Scenario: Keep the last effective setting while a request is pending

- **WHEN** 新请求已经应用, 但视口隐藏、当前没有可用槽或匹配输出仍等待共享上传容量
- **THEN** 状态报告新请求及等待原因, 保留最后接受的比例和尺寸, 未获得匹配颜色的可见正常视口按现有规则显示占位

#### Scenario: Publish the setting with the matching accepted frame

- **WHEN** 新比例对应的当前颜色在完整正常图合成中被采样，且该次提交已接受
- **THEN** View 发布该比例与实际输出尺寸，测试图在取整后尺寸与有效图像矩形均相同时可沿用已经接受的匹配颜色代，Forward 预览以本次场景、相机和模式的匹配输出为准

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
