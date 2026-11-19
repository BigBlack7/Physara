# Spec Delta

## ADDED Requirements

### Requirement: Prepare independent work for requested offscreen views

正式帧准备 SHALL 支持同一 Scene 同时至少两个合法离屏 View 请求。请求集合 SHALL 有明确容量，每个有效请求 SHALL 带唯一 View 身份、当前请求版本、有效渲染与输出像素范围，并采用所属 View 的相机、曝光和设置。各路 SHALL 共用同一已准备 SceneFrame，以各自的稳定裁剪、投影、曝光及可见索引形成工作集。View 工作集和静态快照借用 SHALL 限于当前安全槽的该次使用。纯 View 请求变化 SHALL 独立更新该 View 所需的数据。

缺失、陈旧、重复或超出容量或尺寸限制的请求 SHALL 有可解释的状态，非法候选无法推进有效输出。局部 View 准备失败 SHALL 允许仍可用的 View 和基础 UI 继续准备。能在读取场景记录前确定全部请求均不具备可准备布局或相机输入的轮次 SHALL 跳过场景提取。

#### Scenario: Prepare two camera and image configurations from one scene

- **WHEN** 同一场景有一个主 View 和一个附加离屏请求，两者相机与画布不同，仅更新附加 View 的曝光或布局
- **THEN** 场景记录始终按同一帧快照共享，两个独立的相机、可见索引和图像矩形与各自请求匹配，主 View 的请求及有效设置保持一致

#### Scenario: Reject an invalid offscreen request locally

- **WHEN** 附加 View 交回陈旧身份或无效尺寸的请求，主 View 仍具备合法输入，随后全部请求均不可准备
- **THEN** 附加 View 的诊断可定位，主 View 仍可准备，全部请求不可准备的轮次保持零场景提取
