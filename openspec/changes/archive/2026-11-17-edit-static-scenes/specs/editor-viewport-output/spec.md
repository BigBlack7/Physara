# Editor viewport output delta

## MODIFIED Requirements

### Requirement: Apply viewport extent at a frame boundary

Editor SHALL 从面板内容矩形、本次有效的 framebuffer 对逻辑窗口比例和主 View 当前请求的渲染比例计算 View 像素请求。各轴 SHALL 在比例相乘后向下取整并检查范围和算术。窗口 PresentColor 和离屏 View 输出 SHALL 按各自尺寸准备。布局及相关资源请求 SHALL 在录制前确定，录制期间到达的变化只供后续帧消费。零面积或隐藏视口 SHALL 不发起新的离屏输出分配。

编辑器 SHALL 以同批逻辑布局和 View 准备的有效图像矩形建立内容映射。图像及其输入坐标 SHALL 采用该有效区域，留边或失效布局返回无有效图像位置。输入 SHALL 从当前封存布局的逻辑窗口坐标转换到画布、再映射到有效图像的左上 UV；不得再次计入 DPI 或 render scale。导航命中 SHALL 使用与当前 Scene、相机、预览请求以及布局相符的正常已接受图像，安全回退、未获接受的候选、占位和被剪掉的区域 SHALL 没有输入命中资格。画布的可见性和帧边界像素请求保持独立于导航捕获。

#### Scenario: Resize the window and viewport independently
- **WHEN** 连续改变窗口像素尺寸、视口停靠布局或 framebuffer 比例
- **THEN** 新帧的窗口目标匹配最新有效窗口像素范围，离屏输出对应当帧内容区域的像素请求，内容缩放不被重复计入，单独改变布局不重建窗口目标

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
