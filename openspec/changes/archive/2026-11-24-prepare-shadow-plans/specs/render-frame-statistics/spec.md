# Render Frame Statistics Spec Delta

## ADDED Requirements

### Requirement: Report current shadow planning with logical budget units

Renderer SHALL 为每个已处理 View 提供有界的只读 CPU 规划摘要，标识所属 Scene、View、当前请求和计划版本。摘要 SHALL 报告启用状态和明确原因、请求及有效质量、已分配和未准入的灯数、规划的区域数及层跨度、逻辑字节和上限、所需内容变化及缓存复用。禁用、灯删除、尺寸无效和预算不足的当前结果 SHALL 可辨认。逻辑计划的额度和用量 SHALL 以 CPU 规划单位显示。已接受的场景快照、GPU 时长及原生分配计数 SHALL 保持各自身份和采样口径。

Editor SHALL 从现有统计通道读取摘要，显隐 SHALL 仅改变面板构建。正常帧取消或只接受安全 UI 时，当前 CPU 规划摘要仍可按自身身份辨认，最后已接受的正常场景统计仍保有原值。

#### Scenario: Show planning pressure alongside the last accepted scene

- **WHEN** 一个 View 已有接受的正常场景，之后尺寸或预算使当前 CPU 规划降级，且后续帧没有新的正常场景接受
- **THEN** 面板从摘要显示本次 View、质量与额度原因，最近接受的场景仍带原票据，显示或隐藏面板时规划与提交行为一致
