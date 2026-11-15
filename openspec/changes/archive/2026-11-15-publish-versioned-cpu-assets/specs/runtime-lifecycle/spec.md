# Spec Delta

## ADDED Requirements

### Requirement: Advance CPU asset completion at the continuing input boundary

Runtime SHALL 在主线程装配 CPU 资产服务，并在每个继续运行的有效输入轮次处理可用结果。完成处理 SHALL 位于终止判定之后、帧槽取得、编辑器准备及图执行之前，且无需等待 worker 或 GPU 完成。窗口不可渲染和帧槽忙时仍使用同一处理边界。单独的资产请求失败 SHALL 作为可恢复的资产状态报告，已就绪的编辑器继续按既有生命周期运行。正常关闭与受控退出 SHALL 在释放发布目标之前停止资产接单及发布，取消并收拢作业。

#### Scenario: Publish CPU results while the window cannot acquire a frame
- **WHEN** 资产完成结果已待取，Runtime 接到一个继续运行的输入轮次，窗口处于暂停状态或全部帧槽在途
- **THEN** 主线程可以当轮发布匹配的 CPU 记录。窗口、帧槽和呈现依然遵循既有取得与等待合同

#### Scenario: Exit instead of publishing a terminal batch
- **WHEN** 关闭请求、轮次上限或受控退出使 Runtime 停止运行，且一个作业仍在执行或有结果待取
- **THEN** 系统停止接单和发布并收拢 CPU 作业，终止路径没有新的资产发布。编辑器、宿主和原有完成协议继续按所有权顺序退出

#### Scenario: Continue the editor after an optional asset failure
- **WHEN** 一个可选图片请求完成且报告读取或解码失败，现有基础 UI 可用
- **THEN** 调用者可观察失败原因，继续的输入轮次和可呈现帧仍按产品原有路径推进
