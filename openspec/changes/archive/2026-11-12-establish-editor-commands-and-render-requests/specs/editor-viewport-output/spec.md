# Editor viewport output delta

## MODIFIED Requirements

### Requirement: Apply viewport extent at a frame boundary

Editor SHALL 从面板内容矩形、本次有效的 framebuffer 对逻辑窗口比例和主 View 当前请求的渲染比例计算 View 像素请求。各轴 SHALL 在比例相乘后向下取整并检查范围和算术。窗口 PresentColor 和离屏 View 输出 SHALL 按各自尺寸准备。布局及相关资源请求 SHALL 在录制前确定，录制期间到达的变化只供后续帧消费。零面积或隐藏视口 SHALL 不发起新的离屏输出分配。

#### Scenario: Resize the window and viewport independently
- **WHEN** 连续改变窗口像素尺寸、视口停靠布局或 framebuffer 比例
- **THEN** 新帧的窗口目标匹配最新有效窗口像素范围，离屏输出对应当帧内容区域的像素请求，内容缩放不被重复计入，单独改变布局不重建窗口目标

#### Scenario: Hide and reopen a viewport
- **WHEN** 面板隐藏或内容面积变为零，随后恢复可绘制面积
- **THEN** 空闲期间其余编辑器界面仍可合成，没有为该视口分配替代纹理，恢复时按当前内容矩形解析同一逻辑 View 和 token

#### Scenario: Scale diagnostic pixels independently of window pixels
- **WHEN** 本帧内容矩形有效，View 的渲染比例在准备前改变
- **THEN** 离屏请求使用新比例和各轴实际 framebuffer 比例，窗口目标仍由窗口像素决定，视口逻辑矩形和输入命中继续使用布局坐标

## ADDED Requirements

### Requirement: Confirm effective viewport settings from accepted output

View SHALL 分别公开最新合法请求比例、最后接受的有效比例及其输出像素范围。首次确认前有效值 SHALL 显示为尚未确认。待确认的请求缺少可用布局、槽或暂时可用的上传容量时 SHALL 报告 Pending 及原因。请求尺寸超出设备能力、有效像素范围或单目标预算时 SHALL 报告具体拒绝原因。两种结果均保留最后接受的有效状态。只有匹配当前请求的诊断输出实际用于完整合成且该提交被接受后，View 才可发布其有效比例和尺寸。窗口交换结果 SHALL 与该确认独立。无匹配颜色的帧 SHALL 沿现有诊断占位合同呈现。

#### Scenario: Keep the last effective setting while a request is pending
- **WHEN** 新请求已经应用，但视口隐藏、当前没有可用槽或匹配诊断输出仍等待共享上传容量
- **THEN** 状态报告新请求及等待原因，保留最后接受的比例和尺寸，未获得匹配颜色的可见视口按现有规则显示占位

#### Scenario: Publish the setting with the matching accepted frame
- **WHEN** 新比例对应的诊断颜色在完整合成中被采样，且该次提交已接受
- **THEN** View 发布该比例与实际输出尺寸，如果取整后的尺寸仍相同则可沿用已经接受的匹配颜色代

#### Scenario: Recover from an unsupported extent
- **WHEN** 当前请求和布局产生的尺寸超出可支持范围，随后编辑或布局变化产生可准备的范围
- **THEN** 拒绝阶段报告具体范围或预算原因并保留最后有效状态，恢复后只发布成功接受的当前请求和输出
