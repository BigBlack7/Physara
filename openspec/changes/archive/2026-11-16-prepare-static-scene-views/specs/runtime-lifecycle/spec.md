# Runtime Lifecycle Delta

## ADDED Requirements

### Requirement: Prepare current scene views before graph declaration

正常可准备帧 SHALL 在资产与文档发布、CPU UI 封存及领域事务应用后，用安全取得的槽准备当前 SceneFrame 和 View 工作集。构图与执行 SHALL 仅消费帧内准备结果、声明的图资源及其已拥有依赖。无可准备布局、窗口暂停或无安全槽时 SHALL 跳过本轮场景提取，继续按现有边界推进 CPU 文件、编辑和退休工作。局部 View 无法准备时 SHALL 沿用可用的编辑器图和占位，保留相应诊断。仅诊断颜色经接受提交发布其设置，场景渲染历史仍等待真实场景输出。

#### Scenario: Prepare the scene after this iteration's publication and edits

- **WHEN** 已完成文档或资产结果在继续轮次发布，同一轮 UI 领域事务更新当前 View 或修改场景，且有匹配的布局和安全槽
- **THEN** SceneFrame 和相机准备来自该轮最终接受的同一 Scene/View 状态，构图与执行无需访问活动 ECS

#### Scenario: Skip preparation until a layout and slot are available

- **WHEN** 视口隐藏、窗口不可绘制或无安全槽，本轮仍有可取 CPU 工作，随后取得新布局及槽
- **THEN** 前一轮只按已有生命周期推进允许的 CPU 和 UI 工作，后续帧从届时的最新接受状态准备，未占用在途槽

#### Scenario: Keep the diagnostic UI usable after camera preparation fails

- **WHEN** 可见 View 的相机或有效图像矩形准备失败且基础 UI 可用
- **THEN** 该 View 呈现原因，窗口仍可合成其余 UI，无效准备值无法推进任何场景输出或被确认的渲染历史
