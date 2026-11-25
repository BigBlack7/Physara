# view-preparation Spec Delta

## ADDED Requirements

### Requirement: Prepare and confirm an opaque strategy per view

主 View 和显式离屏 View SHALL 从自己的运行时请求选择 Forward 或 Forward+。缺省 SHALL 使用 Forward。普通受光视图在两种策略下 SHALL 支持当前单样本 None 和 FXAA，按当前请求准备实际显示阶段。路径请求 SHALL 沿用现有场景、资产、编辑状态和兼容的驻留资源。

图声明前，所选策略 SHALL 按同轮 Scene、View、相机、布局、画质和冻结的 Shader 代准备可消费的视图计划。实际所需的 Shader/Pipeline、绑定配方和其资源满足时才为 Ready。需跨帧推进时 SHALL 交付 Pending 和原因。不可兼容或实际准备失败时 SHALL 交付 Failed 和原因。图准备 SHALL 用该计划建立实际绑定。前一帧结果仍具有读取资格时，未就绪或未接受的尝试 SHALL 保有该 View 的原输出和原身份。构图和 Draw SHALL 消费封存计划，无即时 Shader 编译、等待全设备空闲或隐式改用另一条受光路径。

就绪方案 SHALL 从帧边界进入正常图。已接受记录 SHALL 通过现有确认入口与实际使用的计划、颜色、View、Shader 代和提交票据配对。有效布局中的主视口 SHALL 继续遵循其正常图实际采样条件。显式离屏视图 SHALL 确认同一轮自己请求的输出。某路拒绝、取消、未提交或执行失败 SHALL 不确认该候选。

已接受场景的统计快照和对应的延迟 GPU 计时 SHALL 保有该票据实际接受的不透明策略。后续请求的策略和 CPU 分箱状态不得替换旧票据的路径身份。

#### Scenario: Activate a ready strategy at a frame boundary for either supported AA mode

- **WHEN** 同一静态场景的受光 View 分别在 None 和 FXAA 配置下从 Forward 请求 Forward+，所需计划在帧边界就绪
- **THEN** 下一次匹配的正常接受记录对应实际执行的 Forward+ 计划、原 AA 请求及其 Pipeline 和提交票据。场景、同版本的几何及材质驻留和编辑状态继续可用

#### Scenario: Preserve the last usable output while the new plan is pending or failed

- **WHEN** 一个已有正常接受输出的 View 等待所需 Pipeline，或新策略因活动绑定、设备能力或预算未满足而被拒绝
- **THEN** 该轮报告相应的 Pending 或 Failed 和原因。该候选未获确认时，仍可读取的旧结果及其已确认身份保持，恢复后只确认第一份匹配的正常提交

#### Scenario: Resolve independent plans from the same scene frame

- **WHEN** 主 View 使用 Forward，离屏 View 以自己的相机、布局和设置请求 Forward+，随后其中一路候选不可用
- **THEN** 两路按各自的准备和正常图消费结果确认。未接受的一路维持自己的记录，另一路可继续使用共享场景与驻留资源完成正常提交
