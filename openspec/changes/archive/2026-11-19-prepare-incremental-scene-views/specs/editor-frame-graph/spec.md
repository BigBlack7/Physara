# Spec Delta

## ADDED Requirements

### Requirement: Execute and publish independent offscreen view outputs

Runtime SHALL 在同一正常产品图中声明存活的主视口与附加离屏 View 生产者，并按各自实际消费决定活性。主视口的显示版本 SHALL 沿现有 UI 采样合同交付。显式离屏消费者 SHALL 声明所需 View 的确切显示色出口，并从本图执行结果取得对应对象。各路颜色和设置 SHALL 按 View 身份、当前请求和独立输出代交付。普通离屏输出 SHALL 使用既有外部着色读交接，窗口目标的呈现交接仍归宿主。

只有本轮实际被消费或显式导出、其生产已完整执行、且同图获得可靠的正常提交回执的 View SHALL 确认对应输出和有效状态。未准备、裁剪或失败的路 SHALL 保留自身最近已接受输出和可辨认的当前状态。正常图整体失败时 SHALL 沿既有安全 UI、前缀提交及退出合同处理。已发工作引用的旧输出 SHALL 由真实完成机制保活。

#### Scenario: Accept two distinct offscreen images from one graph submission

- **WHEN** 当前主视口被 UI 实际采样，且同一 Scene 的附加 View 以另一相机、尺寸或曝光显式导出
- **THEN** 一次正常图与提交交付两份匹配各自请求的颜色资源和输出代，主视口仍采样自己的颜色，两个 View 分别更新各自的有效设置

#### Scenario: Keep a live export when another view loses its consumer

- **WHEN** 两个 View 曾被接受，后续主视口图像被全裁剪且失去最后一个消费者，同时附加离屏出口仍被请求
- **THEN** 主视口分支未物化且保留原有效输出，附加 View 的导出及其真实消费的共享生产者继续执行，成功回执只推进符合确认条件的 View
