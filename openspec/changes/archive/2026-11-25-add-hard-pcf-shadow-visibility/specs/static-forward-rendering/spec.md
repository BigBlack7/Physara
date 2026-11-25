# static-forward-rendering Spec Delta

## ADDED Requirements

### Requirement: Consume shared shadow visibility in the current forward output

普通 Direct Lighting 与 Physical Lighting SHALL 在写预曝光线性 HDR 时通过同一直接光查询消费当前 View 的阴影可见性。级联、聚光和点光的线性诊断 SHALL 使用相同的覆盖、偏移和过滤查询，并沿用各自的颜色和显示合同。禁用阴影或无可用阴影分配时，普通受光输出 SHALL 使用完整直接光及中性可见性。非受光预览、Unlit、背景合成、自发光和环境贡献 SHALL 保留其各自输出合同。实际准备、录制或提交失败 SHALL 沿用现有失败退路与已接受输出合同。

#### Scenario: Observe a shadow in ordinary HDR and its matching diagnostic

- **WHEN** 以相同场景、相机和曝光读取阴影关闭及开启的受光 HDR，并从同一 View 请求遮挡灯的诊断
- **THEN** 两种受光模式的变化可由该灯的可见性和原直接贡献解释。稳定无遮挡样点、Unlit 和同输入的环境及自发光贡献保持。诊断明暗与受光使用的同一过滤选择及可见性相符。
