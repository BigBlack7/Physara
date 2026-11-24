# static-forward-rendering Spec Delta

## ADDED Requirements

### Requirement: Display a per-view directional cascade diagnostic

Forward SHALL 在正式 ViewOutput 链提供方向光级联诊断。主 View SHALL 由现有 Renderer 面板提交会话开关，离屏 View SHALL 使用其所属 View 的独立请求。有效诊断 SHALL 使用匹配于当前帧场景版本的已定义阴影层和查询数据，通过颜色显示级联及混合权重，通过明暗显示实际可见性，并沿线性诊断显示配置交付。关闭诊断 SHALL 恢复该 View 请求的显示模式。场景文档 SHALL 继续通过 v1 合同读写。

面板 SHALL 可区分当前请求、已接受诊断及不可用原因。阴影禁用或无有效分配时 SHALL 允许可用的普通 Forward 继续出图。未获接受的诊断候选 SHALL 沿现有视口失败退路处理。只有正常图实际消费并接受的诊断结果 SHALL 被标为本轮有效。

#### Scenario: Observe an accepted cascade diagnostic in the current viewport

- **WHEN** 有效 View 开启方向光级联诊断，场景存在遮挡物、可见接收区和跨越级联边界的覆盖，随后关闭诊断
- **THEN** 正常接受后的诊断颜色可追溯到所查级联和实际遮挡，过渡权重在带内变化。关闭后下一次接受的输出按当前请求模式显示。面板可分别核对两次已接受的输出

#### Scenario: Retain a usable viewport when the diagnostic is unavailable

- **WHEN** 当前 View 请求级联诊断，但关闭了阴影规划、未准入级联，或新的诊断候选未获接受
- **THEN** 可用的普通模式或上次有效 ViewOutput 通过原有路径显示，面板提供对应原因。当前完全接受的诊断仍可在设置和资源有效时发布
